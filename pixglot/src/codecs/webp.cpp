#include "../decoder.hpp"
#include "pixglot/buffer.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/utils/cast.hpp"

#include <webp/demux.h>

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





  class webp_frame_iterator {
    public:
      webp_frame_iterator(const webp_frame_iterator&) = delete;
      webp_frame_iterator(webp_frame_iterator&&)      = delete;
      webp_frame_iterator& operator=(const webp_frame_iterator&) = delete;
      webp_frame_iterator& operator=(webp_frame_iterator&&)      = delete;

      ~webp_frame_iterator() {
        if (iterator_) {
          WebPDemuxReleaseIterator(&*iterator_);
        }
      }



      explicit webp_frame_iterator(WebPDemuxer* demux) :
        iterator_{WebPIterator{}} {
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




  class webp_decoder_config {
    public:
      webp_decoder_config(webp_decoder_config&&)      = delete;
      webp_decoder_config(const webp_decoder_config&) = delete;
      webp_decoder_config& operator=(const webp_decoder_config&) = delete;
      webp_decoder_config& operator=(webp_decoder_config&&)      = delete;

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
        rgba.stride = buffer.stride();
        rgba.rgba   = utils::byte_pointer_cast<uint8_t>(buffer.data().data());
        rgba.size   = buffer.data().size();
      }



      [[nodiscard]] alpha_mode alpha() const {
        return (config_.output.colorspace == MODE_rgbA ?
          alpha_mode::premultiplied : alpha_mode::straight);
      }



      [[nodiscard]] WebPDecoderConfig* get() {
        return &config_;
      }



    private:
      WebPDecoderConfig config_{};
  };





  class webp_decoder : public decoder {
    public:
      explicit webp_decoder(decoder&& dec) :
        decoder{std::move(dec)},
        data   {input()},
        demux  {WebPDemux(&data.get())}
      {
        if (!demux) {
          throw decode_error{codec::webp, "unable to parse webp"};
        }
      }



      void decode() {
        for (auto& webp_frame: webp_frame_iterator{demux.get()}) {
          if (webp_frame.complete == 0) {
            warn("fragment does not contain full frame");
          }

          auto pixels = create_pixel_buffer(&webp_frame);

          webp_decoder_config config{&webp_frame, pixels,
                                format_out().alpha.prefers(alpha_mode::premultiplied)};

          if (WebPDecode(webp_frame.fragment.bytes, webp_frame.fragment.size,
                config.get()) != VP8_STATUS_OK) {
            throw decode_error{codec::webp, "unable to decode frame"};
          }

          progress(webp_frame.frame_num, webp_frame.num_frames);

          output_image().animated = output_image().animated || webp_frame.duration > 0;

          append_frame(frame {
            .pixels      = std::move(pixels),
            .orientation = square_isometry::identity,

            .alpha       = config.alpha(),
            .gamma       = gamma_s_rgb,
            .endianess   = std::endian::native,

            .duration    = std::chrono::microseconds{webp_frame.duration * 1000}
          });
        }
      }



    private:
      webp_data data;

      struct demux_deleter {
        void operator()(WebPDemuxer* demux) const {
          WebPDemuxDelete(demux);
        }
      };

      std::unique_ptr<WebPDemuxer, demux_deleter> demux;



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
  };
}



image decode_webp(decoder&& dec) {
  webp_decoder wdec{std::move(dec)};

  wdec.decode();
  return wdec.finalize_image();
}
