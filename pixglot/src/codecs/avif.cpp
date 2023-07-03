#include "pixglot/details/decoder.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/utils/cast.hpp"

#include <avif/avif.h>

using namespace pixglot;



namespace {
  class avif_reader {
    public:
      ~avif_reader() = default;

      avif_reader(const avif_reader&) = delete;
      avif_reader(avif_reader&&)      = delete;

      avif_reader& operator=(const avif_reader&) = delete;
      avif_reader& operator=(avif_reader&&)      = delete;

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

        out->size = self->input_->read(self->buffer_, offset);
        out->data = utils::byte_pointer_cast<const uint8_t>(self->buffer_.data());

        if (out->size == 0) {
          if (++self->zero_count_ > 1) {
            return AVIF_RESULT_IO_ERROR;
          }
        }

        return AVIF_RESULT_OK;
      }
  };



  struct avif_rgb_image {
    public:
      avif_rgb_image& operator=(const avif_rgb_image&) = delete;
      avif_rgb_image& operator=(avif_rgb_image&&)      = delete;
      avif_rgb_image(const avif_rgb_image&) = delete;
      avif_rgb_image(avif_rgb_image&&)      = delete;

      ~avif_rgb_image() {
        avifRGBImageFreePixels(&rgb_);
      }

      explicit avif_rgb_image(avifImage* image, const output_format& out) {
        avifRGBImageSetDefaults(&rgb_, image);

        rgb_.format = (image->alphaPlane == nullptr && !out.add_alpha.prefers(true))
                        ? AVIF_RGB_FORMAT_RGB : AVIF_RGB_FORMAT_RGBA;
        try_satisfy_preferences(out);

        allocate_pixels();
      }





      [[nodiscard]] avifRGBImage* get() {
        return &rgb_;
      }



      [[nodiscard]] pixel_buffer take_pixels() {
        rgb_.pixels = nullptr;
        return std::move(std::exchange(pixels_, {})).value();
      }



      [[nodiscard]] alpha_mode to_alpha_mode() const {
        if (avifRGBFormatHasAlpha(rgb_.format) == AVIF_FALSE) {
          return alpha_mode::none;
        }

        return rgb_.alphaPremultiplied != 0 ?
          alpha_mode::premultiplied : alpha_mode::straight;
      }





    private:
      avifRGBImage                rgb_{};
      std::optional<pixel_buffer> pixels_;



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



      void allocate_pixels() {
        pixels_.emplace(rgb_.width, rgb_.height, determine_pixel_format());
        rgb_.pixels   = utils::byte_pointer_cast<uint8_t>(pixels_->data().data());
        rgb_.rowBytes = pixels_->stride();
      }



      void try_satisfy_preferences(const output_format& out) {
        if (out.alpha.prefers(alpha_mode::straight)) {
          rgb_.alphaPremultiplied = AVIF_FALSE;
        } else if (out.alpha.prefers(alpha_mode::premultiplied)) {
          rgb_.alphaPremultiplied = AVIF_TRUE;
        }


        if (out.component_type.prefers(data_format::f16)) {
          rgb_.isFloat = AVIF_TRUE;
          rgb_.depth   = 16;
        } else if (out.component_type.prefers(data_format::u16) ||
            (!out.component_type.prefers(data_format::u8) && rgb_.depth > 8)) {
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
      if (image->imir.mode % 2 == 0) {
        return square_isometry::flip_y;
      }
      return square_isometry::flip_x;
    }

    return square_isometry::identity;
  }



  [[nodiscard]] square_isometry isometry_from(avifImage* image) {
    return get_flipping(image) * get_rotation(image);
  }



  class avif_decoder {
    public:
      explicit avif_decoder(details::decoder& decoder) :
        decoder_{&decoder},
        reader_ {decoder_->input()},
        dec_    {avifDecoderCreate(), avifDecoderDestroy}
      {
        avifDecoderSetIO(dec_.get(), reader_.avif_io());
      }



      void decode() {
        assert_avif(avifDecoderParse(dec_.get()), "avifDecoderParse");



        decoder_->image().animated = dec_->imageCount > 1 &&
          dec_->progressiveState == AVIF_PROGRESSIVE_STATE_UNAVAILABLE;



        uint32_t task_count{static_cast<uint32_t>(dec_->imageCount) * 2};
        uint32_t prog      {0};

        while (avifDecoderNextImage(dec_.get()) == AVIF_RESULT_OK) {
          decoder_->progress(++prog, task_count);

          avif_rgb_image rgb{dec_->image, decoder_->output_format()};
          assert_avif(avifImageYUVToRGB(dec_->image, rgb.get()),
            "avifImageYUVToRGB");

          frame frame{rgb.take_pixels()};
          frame.orientation(isometry_from(dec_->image));
          frame.alpha      (rgb.to_alpha_mode());
          frame.duration   (std::chrono::microseconds{
                              static_cast<long int>(dec_->duration) * 1000 * 1000});

          decoder_->finish_frame(std::move(frame));

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
  };
}



void decode_avif(details::decoder& dec) {
  avif_decoder adec{dec};
  adec.decode();
}
