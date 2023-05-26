#include "../decoder.hpp"
#include "pixglot/codecs-magic.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/square-isometry.hpp"
#include "pixglot/utils/cast.hpp"

#include <algorithm>

#include <jxl/decode_cxx.h>

using namespace pixglot;



namespace {
  void assert_jxl(
      JxlDecoderStatus            stat,
      const std::string&          msg,
      const std::source_location& location = std::source_location::current()
  ) {
    if (stat != JXL_DEC_SUCCESS) {
      throw decode_error{codec::jxl, msg, location};
    }
  }



  [[nodiscard]] std::optional<square_isometry> convert_orientation(JxlOrientation trafo) {
    using enum square_isometry;

    switch (trafo) {
      case JXL_ORIENT_IDENTITY:        return identity;
      case JXL_ORIENT_FLIP_HORIZONTAL: return flip_x;
      case JXL_ORIENT_FLIP_VERTICAL:   return flip_y;
      case JXL_ORIENT_ROTATE_180:      return rotate_half;
      case JXL_ORIENT_TRANSPOSE:       return transpose;
      case JXL_ORIENT_ANTI_TRANSPOSE:  return anti_transpose;
      case JXL_ORIENT_ROTATE_90_CW:    return rotate_cw;
      case JXL_ORIENT_ROTATE_90_CCW:   return rotate_ccw;

      default: return {};
    }
  }



  [[nodiscard]] JxlDataType convert_data_format(data_format format) {
    switch (format) {
      case data_format::u8:  return JXL_TYPE_UINT8;
      case data_format::u16: return JXL_TYPE_UINT16;
      case data_format::f16: return JXL_TYPE_FLOAT16;
      case data_format::f32: return JXL_TYPE_FLOAT;
      default: break;
    }

    throw decode_error{codec::jxl, "unsupported data format"};
  }



  [[nodiscard]] JxlEndianness convert_endian(std::endian end) {
    if (end == std::endian::native) {
      return JXL_NATIVE_ENDIAN;
    }

    switch (end) {
      case std::endian::big:    return JXL_BIG_ENDIAN;
      case std::endian::little: return JXL_LITTLE_ENDIAN;
    }

    throw decode_error{codec::jxl, "unsupported endian format"};
  }



  [[nodiscard]] std::chrono::microseconds convert_duration(
      const JxlBasicInfo& info,
      uint32_t            d
  ) {
    if (info.animation.tps_denominator == 0) {
      return std::chrono::microseconds{d * 1000 * 1000};
    }

    return std::chrono::microseconds{
      (static_cast<uint64_t>(info.animation.tps_numerator) * 1000ull * 1000ull *
      static_cast<uint64_t>(d)) /
      static_cast<uint64_t>(info.animation.tps_denominator)
    };
  }





  class jxl_reader {
    public:
      explicit jxl_reader(reader& input) :
        data{input.size()}
      {
        if (input.read(data.as_bytes()) != data.byte_size()) {
          throw decode_error{codec::jxl, "unexpected eof"};
        }
      }



      void set_input(JxlDecoder* dec) {
        assert_jxl(JxlDecoderSetInput(dec, data.data(), data.size()),
          "unable to set input");
      }



    private:
      buffer<uint8_t> data;
  };





  class jxl_decoder : public decoder {
    public:
      explicit jxl_decoder(decoder&& dec) :
        decoder{std::move(dec)},
        jxl    {JxlDecoderMake(nullptr)},
        reader {input()}
      {
        reader.set_input(jxl.get());

        alpha_strategy  = create_alpha_strategy();  // NOLINT(*initializer)
        endian_strategy = create_endian_strategy(); // NOLINT(*initializer)


        if (!format_out().orientation.prefers(square_isometry::identity)) {
          assert_jxl(JxlDecoderSetKeepOrientation(jxl.get(), JXL_TRUE),
            "unable to take responsibility for image orientation");
        }

        assert_jxl(JxlDecoderSetCoalescing(jxl.get(), JXL_TRUE),
          "unable to request frame coalescing");
      }



      void decode() {
        assert_jxl(JxlDecoderSubscribeEvents(jxl.get(),
          JXL_DEC_BASIC_INFO |
          JXL_DEC_FRAME      |
          JXL_DEC_FULL_IMAGE
        ), "unable to subscribe to events");

        event_loop();
      }



    private:
      JxlDecoderPtr  jxl;
      jxl_reader     reader;

      JxlBasicInfo   info{};
      JxlFrameHeader frame_header{};

      alpha_mode     alpha_strategy {alpha_mode::premultiplied};
      std::endian    endian_strategy{std::endian::native};

      size_t         pixels_done{0};
      size_t         pixels_total{0};

      std::unique_ptr<pixel_buffer> current_pixels;





      void update_pixel_count(size_t num_pixels) {
        pixels_done += num_pixels;
        progress(pixels_done, pixels_total);
      }



      [[nodiscard]] alpha_mode create_alpha_strategy() {
        auto straight = format_out().alpha.prefers(alpha_mode::straight);

        assert_jxl(JxlDecoderSetUnpremultiplyAlpha(jxl.get(), static_cast<int>(straight)),
          "unable to request alpha mode");

        return straight ? alpha_mode::straight : alpha_mode::premultiplied;
      }



      void on_basic_info() {
        assert_jxl(JxlDecoderGetBasicInfo(jxl.get(), &info),
          "unable to obtain basic info");
      }



      void on_full_image() {
        if (!current_pixels) {
          throw decode_error{codec::jxl, "trying to complete image without active pixels"};
        }

        auto duration = convert_duration(info, frame_header.duration);
        output_image().animated = output_image().animated
                                  || duration > std::chrono::microseconds{0};


        append_frame(frame {
          .pixels      = std::move(*current_pixels),
          .orientation = unwrap_orientation(convert_orientation(info.orientation)),

          .alpha       = get_alpha_mode(info),
          .gamma       = gamma_s_rgb,
          .endianess   = endian_strategy,

          .duration    = duration
        });

        current_pixels.reset(nullptr);
      }



      static void on_pixels(
          void*       opaque,
          size_t      x,
          size_t      y,
          size_t      num_pixels,
          const void* pixels
      ) {
        auto* self = static_cast<jxl_decoder*>(opaque);

        if (x + num_pixels > self->current_pixels->width()) {
          throw decode_error{codec::jxl, "trying to write pixels out of bounds"};
        }

        if (self->current_pixels) {
          std::span<const std::byte> source{
            static_cast<const std::byte*>(pixels),
            num_pixels * self->current_pixels->format().size()
          };

          std::ranges::copy(source, self->current_pixels->row_bytes(y).begin());

          self->update_pixel_count(num_pixels);
        } else {
          throw decode_error{codec::jxl,
                  "trying to write pixels without active pixel buffer"};
        }
      }



      void on_frame() {
        pixels_total += static_cast<size_t>(info.xsize) * info.ysize;

        current_pixels = std::make_unique<pixel_buffer>(info.xsize, info.ysize,
                                 select_pixel_format(info));

        auto format = convert_pixel_format(current_pixels->format());

        assert_jxl(JxlDecoderGetFrameHeader(jxl.get(), &frame_header),
          "unable to obtain frame header");

        assert_jxl(JxlDecoderSetImageOutCallback(jxl.get(),
          &format,
          on_pixels,
          this
        ), "unable to set image out callback");
      }



      void event_loop() {
        while (true) {
          switch (auto v = JxlDecoderProcessInput(jxl.get())) {
            case JXL_DEC_SUCCESS:
              return;
            case JXL_DEC_ERROR:
              throw decode_error{codec::jxl, "unhandled error"};

            case JXL_DEC_BASIC_INFO:
              on_basic_info();
              break;
            case JXL_DEC_FRAME:
              on_frame();
              break;
            case JXL_DEC_FULL_IMAGE:
              on_full_image();
              break;

            default:
              warn("unhandled decoding event: " + std::to_string(v));
              break;
          }
        }
      }



      [[nodiscard]] square_isometry unwrap_orientation(
          std::optional<square_isometry> iso
      ) {
        if (!iso) {
          warn("unknown image orientation");
          return square_isometry::identity;
        }
        return *iso;
      }



      [[nodiscard]] JxlPixelFormat convert_pixel_format(pixel_format format) const {
        return JxlPixelFormat {
          .num_channels = n_channels(format.channels),
          .data_type    = convert_data_format(format.format),
          .endianness   = convert_endian(endian_strategy),
          .align        = pixel_buffer::alignment
        };
      }



      [[nodiscard]] std::endian create_endian_strategy() const {
        if (format_out().endianess.has_preference()) {
          return format_out().endianess.value();
        }
        return std::endian::native;
      }



      [[nodiscard]] pixel_format select_pixel_format(const JxlBasicInfo& info) const {
        return pixel_format {
          .format   = select_data_format(info),
          .channels = select_color_channels(info)
        };
      }



      [[nodiscard]] color_channels select_color_channels(const JxlBasicInfo& info) const {
        if (info.num_color_channels == 1 &&
          !format_out().expand_gray_to_rgb.prefers(true)) {
          if (info.alpha_bits > 0) {
            return color_channels::gray_a;
          }
          return color_channels::gray;
        }
        if (info.alpha_bits > 0) {
          return color_channels::rgba;
        }
        return color_channels::rgb;
      }



      [[nodiscard]] data_format select_data_format(const JxlBasicInfo& info) const {
        if (format_out().component_type.has_preference()) {
          switch (auto val = format_out().component_type.value()) {
            case data_format::u8:
            case data_format::u16:
            case data_format::f16:
            case data_format::f32:
              return val;
            default:
              break;
          }
        }

        if (info.exponent_bits_per_sample > 0) {
          if (info.bits_per_sample > 16) {
            return data_format::f32;
          }
          return data_format::f16;
        }
        if (info.bits_per_sample > 8) {
          return data_format::u16;
        }
        return data_format::u8;
      }



      [[nodiscard]] alpha_mode get_alpha_mode(const JxlBasicInfo& info) const {
        if (info.alpha_bits > 0) {
          return alpha_strategy;
        }
        return alpha_mode::none;
      }
  };
}



bool pixglot::magic<codec::jxl>::check(std::span<const std::byte> input) {
  switch (JxlSignatureCheck(utils::byte_pointer_cast<const uint8_t>(input.data()),
                            input.size())) {

    case JXL_SIG_CODESTREAM:
    case JXL_SIG_CONTAINER:
      return true;

    default:
      return false;
  }
}



image decode_jxl(decoder&& dec) {
  jxl_decoder jdec{std::move(dec)};

  jdec.decode();

  return jdec.finalize_image();
}
