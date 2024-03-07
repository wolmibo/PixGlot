#include "config.hpp"
#include "pixglot/buffer.hpp"
#include "pixglot/details/decoder.hpp"
#include "pixglot/details/exif.hpp"
#include "pixglot/details/hermit.hpp"
#include "pixglot/details/int_cast.hpp"
#include "pixglot/details/string-bytes.hpp"
#include "pixglot/details/xmp.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/frame-source-info.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/utils/cast.hpp"

#include <webp/demux.h>
#include <webp/mux.h>

using namespace pixglot;



namespace {
  struct decoder_config_deleter {
    void operator()(WebPDecoderConfig* cfg) const {
      WebPFreeDecBuffer(&cfg->output);
      delete cfg; // NOLINT
    }
  };





  class webp_data {
    public:
      explicit webp_data(reader& input) :
        data_{input.size()},
        data_ptr_{
          .bytes = data_.data(),
          .size  = data_.size()
        }
      {
        if (input.read(data_.as_bytes()) != data_.byte_size()) {
          throw decode_error{codec::webp, "unexpected eof"};
        }
      }



      [[nodiscard]] WebPData& get() {
        return data_ptr_;
      }



    private:
      buffer<uint8_t> data_;
      WebPData        data_ptr_;
  };





  //NOLINTNEXTLINE(*-special-member-functions)
  class webp_frame_iterator : details::hermit {
    public:
      ~webp_frame_iterator() {
        if (iterator_) {
          WebPDemuxReleaseIterator(&*iterator_);
        }
      }



      explicit webp_frame_iterator(WebPDemuxer* demux) :
        iterator_{WebPIterator{}}
      {
        if (WebPDemuxGetFrame(demux, 1, &*iterator_) == 0) {
          WebPDemuxReleaseIterator(&*iterator_);
          iterator_ = {};
        }
      }





      class iterator {
        public:
          explicit iterator(WebPIterator* ptr) : iter_{ptr} {}


          iterator& operator++() {
            if ((iter_ != nullptr) && (WebPDemuxNextFrame(iter_) == 0)) {
              iter_ = nullptr;
            }
            return *this;
          }



          [[nodiscard]] bool operator==(const iterator& rhs) const {
            return iter_ == rhs.iter_;
          }



          [[nodiscard]] WebPIterator& operator*()  { return *iter_; }
                        WebPIterator* operator->() { return iter_;  }



        private:
          WebPIterator*  iter_;
      };



      [[nodiscard]] iterator begin() {
        return iterator{iterator_ ? &*iterator_ : nullptr};
      }

      [[nodiscard]] iterator end() { // NOLINT(*-static)
        return iterator{nullptr};
      }



    private:
      std::optional<WebPIterator> iterator_;
  };




  //NOLINTNEXTLINE(*-special-member-functions)
  class webp_decoder_config : details::hermit {
    public:
      ~webp_decoder_config() {
        WebPFreeDecBuffer(&config_.output);
      }



      explicit webp_decoder_config(
          WebPIterator* iter,
          pixel_buffer& buffer,
          bool          premultiply
      ) {
        if (WebPInitDecoderConfig(&config_) == 0) {
          throw decode_error{codec::webp, "unable to initialize decoder config"};
        }

        config_.output.width  = iter->width;
        config_.output.height = iter->height;

        config_.output.colorspace = (premultiply ? MODE_rgbA : MODE_RGBA);

        config_.output.is_external_memory = 1;

        auto& rgba = config_.output.u.RGBA; //NOLINT(*union*)
        rgba.stride = int_cast<int>(buffer.stride());
        rgba.rgba   = utils::byte_pointer_cast<uint8_t>(buffer.data().data());
        rgba.size   = buffer.data().size();
      }



      [[nodiscard]] WebPDecoderConfig* get() {
        return &config_;
      }



    private:
      WebPDecoderConfig config_{};
  };





  class webp_decoder : details::hermit {
    public:
      explicit webp_decoder(details::decoder& decoder) :
        decoder_{&decoder},
        data_   {decoder_->input()},
        demux_  {WebPDemux(&data_.get())}
      {
        decoder_->image().codec(codec::webp);

        if (!demux_) {
          throw decode_error{codec::webp, "unable to parse webp"};
        }
      }



      void decode() {
        fill_metadata();

        for (auto& webp_frame: webp_frame_iterator{demux_.get()}) {
          decoder_->frame_total(webp_frame.num_frames);

          if (webp_frame.complete == 0) {
            decoder_->warn("fragment does not contain full frame");
          }

          auto& frame = decoder_->begin_frame(
              saturating_cast(webp_frame.width),
              saturating_cast(webp_frame.height),
              rgba<u8>::format()
          );

          frame.source_info().color_model(color_model::yuv);
          frame.source_info().subsampling(chroma_subsampling::cs420);
          frame.source_info().color_model_format({
              data_source_format::u8,
              data_source_format::u8,
              data_source_format::u8,
              webp_frame.has_alpha != 0 ?
                data_source_format::u8 : data_source_format::none
          });

          frame.duration(std::chrono::microseconds{webp_frame.duration * 1000});
          frame.alpha_mode(
              decoder_->output_format().alpha_mode().prefers(alpha_mode::premultiplied) ?
              alpha_mode::premultiplied :
              alpha_mode::straight
          );

          if (decoder_->wants_pixel_transfer()) {
            decoder_->begin_pixel_transfer();
            webp_decoder_config config{&webp_frame, decoder_->target(),
              frame.alpha_mode() == alpha_mode::premultiplied};

            if (WebPDecode(webp_frame.fragment.bytes, webp_frame.fragment.size,
                  config.get()) != VP8_STATUS_OK) {
              throw decode_error{codec::webp, "unable to decode frame"};
            }
            decoder_->finish_pixel_transfer();
          }

          decoder_->finish_frame();
        }
      }



    private:
      details::decoder* decoder_;
      webp_data         data_;

      struct demux_deleter {
        void operator()(WebPDemuxer* demux) const {
          WebPDemuxDelete(demux);
        }
      };

      std::unique_ptr<WebPDemuxer, demux_deleter> demux_;



      [[nodiscard]] static constexpr size_t saturating_cast(int value) {
        return value >= 0 ? value : 0;
      }



      [[nodiscard]] static pixel_buffer create_pixel_buffer(WebPIterator* iter) {
        pixel_format format{
          .format   = data_format::u8,
          .channels = color_channels::rgba
        };

        return pixel_buffer{
          saturating_cast(iter->width),
          saturating_cast(iter->height),
          format
        };
      }



      void fill_metadata() {
        struct chunk_iterator_deleter {
          void operator()(WebPChunkIterator* iter) {
            WebPDemuxReleaseChunkIterator(iter);
            //NOLINTNEXTLINE(*-owning-memory)
            delete iter;
          }
        };

        std::unique_ptr<WebPChunkIterator, chunk_iterator_deleter>
          iter{new WebPChunkIterator()};

        auto flags = WebPDemuxGetI(demux_.get(), WEBP_FF_FORMAT_FLAGS);

#ifdef PIXGLOT_WITH_XMP
        if ((flags & XMP_FLAG) != 0) {
          if (WebPDemuxGetChunk(demux_.get(), "XMP ", 1, iter.get()) != WEBP_MUX_OK) {
            decoder_->warn("unable to obtain xmp data");
          }

          details::fill_xmp_metadata(
              details::string_from(iter->chunk.bytes, iter->chunk.size),
              decoder_->image().metadata(),
              *decoder_
          );
        }
#endif
#ifdef PIXGLOT_WITH_EXIF
        if ((flags & EXIF_FLAG) != 0) {
          if (WebPDemuxGetChunk(demux_.get(), "EXIF", 1, iter.get()) != WEBP_MUX_OK) {
            decoder_->warn("unable to obtain xmp data");
          }

          std::span buffer{iter->chunk.bytes, iter->chunk.size};

          details::fill_exif_metadata(std::as_bytes(buffer),
              decoder_->image().metadata(), *decoder_);
        }
#endif

      }
  };
}



void decode_webp(details::decoder& dec) {
  webp_decoder wdec{dec};
  wdec.decode();
}
