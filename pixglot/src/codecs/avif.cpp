#include "pixglot/details/decoder.hpp"
#include "pixglot/details/exif.hpp"
#include "pixglot/details/hermit.hpp"
#include "pixglot/details/string-bytes.hpp"
#include "pixglot/details/xmp.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/frame-source-info.hpp"
#include "pixglot/metadata.hpp"
#include "pixglot/utils/cast.hpp"

#include <avif/avif.h>

using namespace pixglot;



namespace {
  class avif_reader : details::hermit {
    public:
      explicit avif_reader(reader& input) :
        input_  {&input},
        avif_io_{
          .destroy    = nullptr,
          .read       = read,
          .write      = nullptr,
          .sizeHint   = input_->size(),
          .persistent = AVIF_FALSE,
          .data       = this}
      {}



      [[nodiscard]] avifIO* avif_io() { return &avif_io_; }



    private:
      reader*                input_;
      avifIO                 avif_io_;
      std::vector<std::byte> buffer_;
      int                    zero_count_{0};



      [[nodiscard]] static avifResult read(
          avifIO*     io,
          uint32_t    /*unused*/,
          uint64_t    offset,
          size_t      size,
          avifROData* out
      ) {
        auto *self = static_cast<avif_reader*>(io->data);

        self->buffer_.resize(size);

        if (!self->input_->seek(offset)) {
          return AVIF_RESULT_IO_ERROR;
        }
        out->size = self->input_->read(self->buffer_);
        out->data = utils::byte_pointer_cast<const uint8_t>(self->buffer_.data());

        if (out->size == 0) {
          if (++self->zero_count_ > 1) {
            return AVIF_RESULT_IO_ERROR;
          }
        }

        return AVIF_RESULT_OK;
      }
  };





  void set_frame_source_info(frame_source_info& fsi, avifImage* image) {
    auto color_depth = static_cast<data_source_format>(image->depth);

    fsi.color_model(color_model::yuv);

    switch (image->yuvFormat) {
      case AVIF_PIXEL_FORMAT_YUV400:
        fsi.color_model(color_model::value);
        break;
      case AVIF_PIXEL_FORMAT_YUV444:
        fsi.subsampling(chroma_subsampling::cs444);
        break;
      case AVIF_PIXEL_FORMAT_YUV422:
        fsi.subsampling(chroma_subsampling::cs422);
        break;
      case AVIF_PIXEL_FORMAT_YUV420:
        fsi.subsampling(chroma_subsampling::cs420);
        break;
      default:
        color_depth = data_source_format::none;
        fsi.color_model(color_model::value);
        break;
    }

    auto alpha_depth{data_source_format::none};

    if (image->imageOwnsAlphaPlane != AVIF_FALSE) {
      alpha_depth = color_depth;
    }

    fsi.color_model_format({color_depth, color_depth, color_depth, alpha_depth});
  }





  //NOLINTNEXTLINE(*-special-member-functions)
  class avif_rgb_image : details::hermit {
    public:
      ~avif_rgb_image() {
        rgb_.pixels = nullptr;
        avifRGBImageFreePixels(&rgb_);
      }

      explicit avif_rgb_image(avifImage* image, const output_format& out) :
        image_{image}
      {
        avifRGBImageSetDefaults(&rgb_, image_);

        rgb_.format = (image->alphaPlane == nullptr && !out.fill_alpha().prefers(true))
                        ? AVIF_RGB_FORMAT_RGB : AVIF_RGB_FORMAT_RGBA;
        try_satisfy_preferences(out);
      }





      frame& begin_frame(details::decoder* decoder) {
        auto& frame = decoder->begin_frame(rgb_.width, rgb_.height,
            determine_pixel_format());
        frame.alpha_mode(get_alpha_mode());

        return frame;
      }




      void set_pixel_buffer(pixel_buffer& buffer) {
        rgb_.pixels   = utils::byte_pointer_cast<uint8_t>(buffer.data().data());
        rgb_.rowBytes = buffer.stride();
      }




      [[nodiscard]] avifRGBImage* get() {
        return &rgb_;
      }





    private:
      avifImage*    image_;
      avifRGBImage  rgb_{};



      [[nodiscard]] data_format determine_data_format() const {
        if (rgb_.depth == 8 && (rgb_.isFloat == AVIF_FALSE)) {
          return data_format::u8;
        }
        if (rgb_.depth == 16) {
          return rgb_.isFloat == AVIF_FALSE ? data_format::u16 : data_format::f16;
        }
        throw decode_error{codec::avif, std::string{"unsupported data format: "}
          + (rgb_.isFloat == AVIF_FALSE ? "u" : "f")
          + std::to_string(rgb_.depth)};
      }



      [[nodiscard]] color_channels determine_color_channels() const {
        switch (rgb_.format) {
          case AVIF_RGB_FORMAT_RGB:
            return color_channels::rgb;
          case AVIF_RGB_FORMAT_RGBA:
            return color_channels::rgba;
          default:
            throw decode_error{codec::avif, "unsupported color channels: "
              + std::to_string(rgb_.format)};
        }
      }



      [[nodiscard]] pixel_format determine_pixel_format() const {
        return pixel_format {
          .format   = determine_data_format(),
          .channels = determine_color_channels()
        };
      }



      [[nodiscard]] alpha_mode get_alpha_mode() const {
        if (avifRGBFormatHasAlpha(rgb_.format) == AVIF_FALSE) {
          return alpha_mode::none;
        }

        return rgb_.alphaPremultiplied != AVIF_FALSE ?
          alpha_mode::premultiplied : alpha_mode::straight;
      }



      void try_satisfy_preferences(const output_format& out) {
        if (out.alpha_mode().prefers(alpha_mode::straight)) {
          rgb_.alphaPremultiplied = AVIF_FALSE;
        } else if (out.alpha_mode().prefers(alpha_mode::premultiplied)) {
          rgb_.alphaPremultiplied = AVIF_TRUE;
        }


        if (out.data_format().prefers(data_format::f16)) {
          rgb_.isFloat = AVIF_TRUE;
          rgb_.depth   = 16;
        } else if (out.data_format().prefers(data_format::u16) ||
            (!out.data_format().prefers(data_format::u8) && rgb_.depth > 8)) {
          rgb_.isFloat = AVIF_FALSE;
          rgb_.depth   = 16;
        } else {
          rgb_.isFloat = AVIF_FALSE;
          rgb_.depth   = 8;
        }
      }
  };



  [[nodiscard]] square_isometry get_rotation(avifImage* image) {
    if ((image->transformFlags & AVIF_TRANSFORM_IROT) != 0) {
      switch (image->irot.angle % 4) {
        case 0: return square_isometry::identity;
        case 1: return square_isometry::rotate_ccw;
        case 2: return square_isometry::rotate_cw;
        case 3: return square_isometry::rotate_half;
        default:
          // unreachable
          return square_isometry::identity;
      }
    } else {
      return square_isometry::identity;
    }
  }



  [[nodiscard]] square_isometry get_flipping(avifImage* image) {
    if ((image->transformFlags & AVIF_TRANSFORM_IMIR) != 0) {
#if AVIF_VERSION_MAJOR >= 1
      if (image->imir.axis % 2 == 0) {
#else
      if (image->imir.mode % 2 == 0) {
#endif
        return square_isometry::flip_y;
      }
      return square_isometry::flip_x;
    }

    return square_isometry::identity;
  }



  [[nodiscard]] square_isometry isometry_from(avifImage* image) {
    return get_flipping(image) * get_rotation(image);
  }



  class avif_decoder : details::hermit {
    public:
      explicit avif_decoder(details::decoder& decoder) :
        decoder_{&decoder},
        reader_ {decoder_->input()},
        dec_    {avifDecoderCreate(), avifDecoderDestroy}
      {
        decoder_->image().codec(codec::avif);

        avifDecoderSetIO(dec_.get(), reader_.avif_io());
      }



      void decode() {
        assert_avif(avifDecoderParse(dec_.get()), "avifDecoderParse");



        long int time_multi = (dec_->imageCount > 1 &&
          dec_->progressiveState == AVIF_PROGRESSIVE_STATE_UNAVAILABLE) ? 1000 * 1000 : 0;



        uint32_t task_count{static_cast<uint32_t>(dec_->imageCount) * 2};
        uint32_t prog      {0};

        while (avifDecoderNextImage(dec_.get()) == AVIF_RESULT_OK) {
          decoder_->progress(++prog, task_count);

          avif_rgb_image rgb{dec_->image, decoder_->output_format()};

          auto& frame = rgb.begin_frame(decoder_);
          set_frame_source_info(frame.source_info(), dec_->image);
          frame.orientation(isometry_from(dec_->image));
          frame.duration   (std::chrono::microseconds{
                              static_cast<long int>(dec_->duration) * time_multi});

          fill_metadata(frame.metadata(), *dec_->image);

          if (decoder_->wants_pixel_transfer()) {
            decoder_->begin_pixel_transfer();

            rgb.set_pixel_buffer(decoder_->target());

            assert_avif(avifImageYUVToRGB(dec_->image, rgb.get()),
              "avifImageYUVToRGB");

            decoder_->finish_pixel_transfer();
          }

          decoder_->finish_frame();

          decoder_->progress(++prog, task_count);
        }
      }



    private:
      details::decoder*                                           decoder_;
      avif_reader                                                 reader_;
      std::unique_ptr<avifDecoder, decltype(&avifDecoderDestroy)> dec_;

      static void assert_avif(
          avifResult                  res,
          const std::string&          message,
          const std::source_location& location = std::source_location::current()
      ) {
        if (res != AVIF_RESULT_OK) {
          throw decode_error{codec::avif,
            message + ": " + avifResultToString(res), location};
        }
      }





      void fill_metadata(metadata& md, const avifImage& img) {
#ifdef PIXGLOT_WITH_XMP
        if (img.xmp.size > 0) {
          details::fill_xmp_metadata(details::string_from(img.xmp.data, img.xmp.size),
              md, *decoder_);
        }
#endif
#ifdef PIXGLOT_WITH_EXIF
        if (img.exif.size > 0) {
          details::fill_exif_metadata(
              std::as_bytes(std::span{img.exif.data, img.exif.size}), md, *decoder_);
        }
#endif
      }
  };
}



void decode_avif(details::decoder& dec) {
  avif_decoder adec{dec};
  adec.decode();
}
