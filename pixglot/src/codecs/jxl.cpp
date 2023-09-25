#include "config.hpp"
#include "pixglot/codecs-magic.hpp"
#include "pixglot/details/decoder.hpp"
#include "pixglot/details/exif.hpp"
#include "pixglot/details/hermit.hpp"
#include "pixglot/details/string-bytes.hpp"
#include "pixglot/details/xmp.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/frame-source-info.hpp"
#include "pixglot/pixel-buffer.hpp"
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





  [[nodiscard]] data_source_format find_format(uint32_t bits, uint32_t exponent_bits) {
    if (exponent_bits == 0) {
      return static_cast<data_source_format>(bits);
    }

    if (bits <= 16) {
      return data_source_format::f16;
    }

    return data_source_format::f32;
  }





  class jxl_reader : details::hermit {
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





  enum class metadata_type {
    none,
    xmp,
    exif,
  };

  [[nodiscard]] metadata_type determine_metadata(std::string_view box_type) {
#ifdef PIXGLOT_WITH_XMP
    if (box_type == "xml ") { return metadata_type::xmp; }
#endif
#ifdef PIXGLOT_WITH_EXIF
    if (box_type == "exif") { return metadata_type::exif; }
#endif
    return metadata_type::none;
  }





  class jxl_decoder : details::hermit {
    public:
      explicit jxl_decoder(details::decoder& decoder) :
        decoder_   {&decoder},
        jxl_       {JxlDecoderMake(nullptr)},
        reader_    {decoder_->input()}
      {
        decoder_->image().codec(codec::jxl);

        reader_.set_input(jxl_.get());

        alpha_strategy_  = create_alpha_strategy();  // NOLINT(*initializer)
        endian_strategy_ = create_endian_strategy(); // NOLINT(*initializer)


        if (!decoder_->output_format().orientation().prefers(square_isometry::identity)) {
          assert_jxl(JxlDecoderSetKeepOrientation(jxl_.get(), JXL_TRUE),
            "unable to take responsibility for image orientation");
        }

        assert_jxl(JxlDecoderSetCoalescing(jxl_.get(), JXL_TRUE),
          "unable to request frame coalescing");
      }



      void decode() {
        decoder_->frame_total(0);

        assert_jxl(JxlDecoderSubscribeEvents(jxl_.get(),
          JXL_DEC_BASIC_INFO |
//          JXL_DEC_BOX        |
          JXL_DEC_FRAME      |
          JXL_DEC_FULL_IMAGE
        ), "unable to subscribe to events");

        assert_jxl(JxlDecoderSetDecompressBoxes(jxl_.get(), JXL_TRUE),
                    "failed to request box decompressing: "
                    "some metadata might be missing");

        event_loop();
      }



    private:
      details::decoder* decoder_;
      JxlDecoderPtr     jxl_;
      jxl_reader        reader_;

      JxlBasicInfo      info_{};
      JxlFrameHeader    frame_header_{};

      alpha_mode        alpha_strategy_ {alpha_mode::premultiplied};
      std::endian       endian_strategy_{std::endian::native};

      buffer<uint8_t>   box_buffer_     {1024ul * 1024};
      metadata_type     active_box_type_{metadata_type::none};





      [[nodiscard]] alpha_mode create_alpha_strategy() {
        auto straight = decoder_->output_format().alpha_mode()
                            .prefers(alpha_mode::straight);

        assert_jxl(JxlDecoderSetUnpremultiplyAlpha(jxl_.get(), static_cast<int>(straight)),
          "unable to request alpha mode");

        return straight ? alpha_mode::straight : alpha_mode::premultiplied;
      }



      void on_basic_info() {
        assert_jxl(JxlDecoderGetBasicInfo(jxl_.get(), &info_),
          "unable to obtain basic info");
      }



      void on_full_image() {
        if (decoder_->wants_pixel_transfer()) {
          decoder_->finish_pixel_transfer();
        }

        decoder_->finish_frame();
      }



      static void on_pixels(
          void*       opaque,
          size_t      x,
          size_t      y,
          size_t      num_pixels,
          const void* pixels
      ) {
        auto* self = static_cast<jxl_decoder*>(opaque);

        if (!self->decoder_->wants_pixel_transfer()) {
          return;
        }

        if (x + num_pixels > self->decoder_->target().width()) {
          throw decode_error{codec::jxl, "trying to write pixels out of bounds"};
        }

        std::span<const std::byte> source{
          static_cast<const std::byte*>(pixels),
          num_pixels * self->decoder_->target().format().size()
        };

        std::ranges::copy(source, self->decoder_->target().row_bytes(y).begin());
        self->decoder_->frame_mark_ready_until_line(y);
      }



      void on_frame() {
        decoder_->frame_total(decoder_->frame_total() + 1);

        assert_jxl(JxlDecoderGetFrameHeader(jxl_.get(), &frame_header_),
          "unable to obtain frame header");

        auto& frame = decoder_->begin_frame(info_.xsize, info_.ysize,
          select_pixel_format(info_), endian_strategy_);

        set_frame_source_info(frame.source_info());
        set_frame_name(frame);

        frame.orientation(unwrap_orientation(convert_orientation(info_.orientation)));
        frame.duration   (convert_duration(info_, frame_header_.duration));
        frame.alpha_mode (get_alpha_mode(info_));

        decoder_->begin_pixel_transfer();

        auto format = convert_pixel_format(frame.format());

        assert_jxl(JxlDecoderSetImageOutCallback(jxl_.get(),
          &format,
          on_pixels,
          this
        ), "unable to set image out callback");
      }



      void on_box() {
        finish_box();

        std::string box_type(sizeof(JxlBoxType), ' ');

        if (soft_assert(JxlDecoderGetBoxType(jxl_.get(), box_type.data(), JXL_TRUE),
                        "unable to get box type")) {
          return;
        }


        active_box_type_ = determine_metadata(box_type);
        if (active_box_type_ != metadata_type::none) {
          soft_assert(JxlDecoderSetBoxBuffer(jxl_.get(), box_buffer_.data(),
                                             box_buffer_.size()),
                      "unable to set box buffer");
        }
      }



      void finish_box() {
        if (active_box_type_ == metadata_type::none) {
          return;
        }

        auto size = box_buffer_.size() - JxlDecoderReleaseBoxBuffer(jxl_.get());

#ifdef PIXGLOT_WITH_XMP
        if (active_box_type_ == metadata_type::xmp) {
          details::fill_xmp_metadata(details::string_from(box_buffer_.data(), size),
                                     decoder_->image().metadata(), *decoder_);
        }
#endif

#ifdef PIXGLOT_WITH_EXIF
        if (active_box_type_ == metadata_type::exif) {
          details::fill_exif_metadata(std::as_bytes(std::span{box_buffer_.data(), size}),
                                      decoder_->image().metadata(), *decoder_);
        }
#endif

        active_box_type_ = metadata_type::none;
      }






      void event_loop() {
        while (true) {
          switch (auto v = JxlDecoderProcessInput(jxl_.get())) {
            case JXL_DEC_SUCCESS:
              finish_box();
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
            case JXL_DEC_BOX:
              on_box();
              break;

            default:
              throw decode_error{codec::jxl,
                "unhandled decoding event: " + std::to_string(v)};
              break;
          }
        }
      }



      [[nodiscard]] square_isometry unwrap_orientation(
          std::optional<square_isometry> iso
      ) {
        if (!iso) {
          decoder_->warn("unknown image orientation");
          return square_isometry::identity;
        }
        return *iso;
      }



      [[nodiscard]] JxlPixelFormat convert_pixel_format(pixel_format format) const {
        return JxlPixelFormat {
          .num_channels = n_channels(format.channels),
          .data_type    = convert_data_format(format.format),
          .endianness   = convert_endian(endian_strategy_),
          .align        = pixel_buffer::alignment
        };
      }



      [[nodiscard]] std::endian create_endian_strategy() const {
        if (auto endian = decoder_->output_format().endian()) {
          return endian.value();
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
          !decoder_->output_format().expand_gray_to_rgb().prefers(true)) {
          if (info.alpha_bits > 0 ||
              decoder_->output_format().fill_alpha().prefers(true)) {
            return color_channels::gray_a;
          }
          return color_channels::gray;
        }
        if (info.alpha_bits > 0 || decoder_->output_format().fill_alpha().prefers(true)) {
          return color_channels::rgba;
        }
        return color_channels::rgb;
      }



      [[nodiscard]] data_format select_data_format(const JxlBasicInfo& info) const {
        if (auto comp = decoder_->output_format().data_format()) {
          switch (auto val = comp.value()) {
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
          return alpha_strategy_;
        }
        return alpha_mode::none;
      }





      void set_frame_source_info(frame_source_info& fsi) const {
        auto color_depth = find_format(info_.bits_per_sample,
            info_.exponent_bits_per_sample);

        auto alpha_depth = find_format(info_.alpha_bits, info_.alpha_exponent_bits);

        if (info_.num_color_channels > 1) {
          fsi.color_model(color_model::unknown);
        } else {
          fsi.color_model(color_model::value);
        }

        fsi.color_model_format({color_depth, color_depth, color_depth, alpha_depth});
      }



      void set_frame_name(frame& f) const {
        if (frame_header_.name_length == 0) {
          return;
        }

        std::vector<char> buffer(frame_header_.name_length + 1);

        if (JxlDecoderGetFrameName(jxl_.get(), buffer.data(), buffer.size())
            != JXL_DEC_SUCCESS) {
          decoder_->warn("unable to obain frame name");
        }

        buffer.back() = '0';

        f.name(buffer.data());
      }



      bool soft_assert(JxlDecoderStatus result, std::string message) {
        if (result != JXL_DEC_SUCCESS) {
          decoder_->warn(std::move(message));
          return false;
        }
        return true;
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



void decode_jxl(details::decoder& dec) {
  jxl_decoder jdec{dec};
  jdec.decode();
}
