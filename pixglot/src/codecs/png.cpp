#include "pixglot/details/decoder.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/utils/cast.hpp"

#include <utility>

#include <png.h>

using namespace pixglot;



namespace {
  struct png_guard {
    png_structp ptr {nullptr};
    png_infop   info{nullptr};



    png_guard(const png_guard&) = delete;
    png_guard& operator=(const png_guard&) = delete;



    png_guard(png_guard&& rhs) noexcept :
      ptr {std::exchange(rhs.ptr,  nullptr)},
      info{std::exchange(rhs.info, nullptr)}
    {}



    png_guard& operator=(png_guard&& rhs) noexcept {
      cleanup();
      ptr  = std::exchange(rhs.ptr,  nullptr);
      info = std::exchange(rhs.info, nullptr);

      return *this;
    }



    ~png_guard() {
      cleanup();
    }



    png_guard() :
      ptr{png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr)}
    {
      if (ptr == nullptr) {
        throw decode_error{codec::png, "unable to create png read struct"};
      }

      info = png_create_info_struct(ptr);
      if (info == nullptr) {
        throw decode_error{codec::png, "unable to create png info struct"};
      }
    }




    private:
      void cleanup() {
        if (ptr != nullptr) {
          if (info != nullptr) {
            png_destroy_read_struct(&ptr, &info, nullptr);
          } else {
            png_destroy_read_struct(&ptr, nullptr, nullptr);
          }
        }
      }
  };





  void png_color_type_add_alpha(png_byte& in) {
    in |= PNG_COLOR_MASK_ALPHA;
  }

  void png_color_type_add_color(png_byte& in) {
    in |= PNG_COLOR_MASK_COLOR;
  }

  void png_color_type_strip_palette(png_byte& in) {
    in &= ~PNG_COLOR_MASK_PALETTE;
  }





  class png_decoder {
    public:
      explicit png_decoder(details::decoder& decoder) :
        decoder_{&decoder}
      {
        png_set_error_fn(png.ptr, this, &error_function, &warn_function);
        png_set_read_fn (png.ptr, this, &read);
      }



      void decode() {
        png_read_info(png.ptr, png.info);

        pixel_buffer buffer{
          png_get_image_width(png.ptr, png.info),
          png_get_image_height(png.ptr, png.info),
          pixel_format {
            .format   = make_format_compatible(),
            .channels = make_color_channels_compatible()
          }
        };

        if (buffer.empty()) {
          throw decode_error{codec::png,
            "unable to obtain image format (or image is empty)"};
        }

        buffer.endian(make_endian_compatible(buffer.format().format));

        auto alpha  = make_alpha_mode_compatible(buffer.format().channels);

        png_read_update_info(png.ptr, png.info);

        auto& frame = decoder_->begin_frame(std::move(buffer));
        frame.alpha(alpha);

        transfer_data(decoder_->target());

        decoder_->finish_frame();
      }



    private:
      details::decoder* decoder_;
      png_guard         png;



      void transfer_data(pixel_buffer& buffer) {
        if (png_get_rowbytes(png.ptr, png.info) > buffer.stride()) {
          throw decode_error{codec::png, "image stride does not fit into pixel buffer"};
        }

        int    passes = png_set_interlace_handling(png.ptr);
        size_t height = png_get_image_height(png.ptr, png.info);

        for (int p = 0; p < passes; ++p) {
          for (size_t y = 0; y < height; ++y) {
            png_read_row(
                png.ptr,
                utils::byte_pointer_cast<png_byte>(buffer.row_bytes(y).data()),
                nullptr
            );

            if (passes == 1) {
              decoder_->frame_mark_ready_until_line(y);
            } else {
              decoder_->progress(y, height, p, passes);
            }
          }
        }
      }



      [[nodiscard]] color_channels make_color_channels_compatible() {
        png_byte color_type = png_get_color_type(png.ptr, png.info);

        if (color_type == PNG_COLOR_TYPE_PALETTE) {
          png_set_palette_to_rgb(png.ptr);

          png_color_type_strip_palette(color_type);
        }

        if ((color_type & PNG_COLOR_MASK_COLOR) == 0 &&
          decoder_->output_format().expand_gray_to_rgb.prefers(true)) {
          png_set_gray_to_rgb(png.ptr);

          png_color_type_add_color(color_type);
        }

        if (png_get_valid(png.ptr, png.info, PNG_INFO_tRNS) != 0) {
          png_set_tRNS_to_alpha(png.ptr);

          png_color_type_add_alpha(color_type);
        }

        if (decoder_->output_format().add_alpha.prefers(true)) {
          png_set_add_alpha(png.ptr, 0xffff, PNG_FILLER_AFTER);

          png_color_type_add_alpha(color_type);
        }

        switch (color_type) {
          case PNG_COLOR_TYPE_GRAY: return color_channels::gray;
          case PNG_COLOR_TYPE_GA:   return color_channels::gray_a;
          case PNG_COLOR_TYPE_RGB:  return color_channels::rgb;
          case PNG_COLOR_TYPE_RGBA: return color_channels::rgba;
        }

        throw decode_error{codec::png, "unknown color type "
          + std::to_string(color_type)};
      }



      [[nodiscard]] alpha_mode make_alpha_mode_compatible(color_channels cc) {
        if (has_alpha(cc)) {
          if (decoder_->output_format().alpha.prefers(alpha_mode::premultiplied)) {
            png_set_alpha_mode(png.ptr, PNG_ALPHA_PREMULTIPLIED, PNG_GAMMA_LINEAR);

            return alpha_mode::premultiplied;
          }
          return alpha_mode::straight;
        }
        return alpha_mode::none;
      }



      [[nodiscard]] std::endian make_endian_compatible(data_format df) {
        if (byte_size(df) > 1 &&
            decoder_->output_format().endianess.prefers(std::endian::little)) {
          png_set_swap(png.ptr);
          return std::endian::little;
        }
        return std::endian::big;
      }



      [[nodiscard]] data_format make_format_compatible() {
        uint32_t bit_depth = png_get_bit_depth(png.ptr, png.info);
        if (bit_depth == 0) {
          throw decode_error{codec::png, "unable to obtain bit depth"};
        }

        if (bit_depth < 8) {
          png_set_expand_gray_1_2_4_to_8(png.ptr);
          bit_depth = 8;
        }

        if (bit_depth == 8) {
          if (decoder_->output_format().component_type.prefers(data_format::u16)) {
            png_set_expand_16(png.ptr);
            return data_format::u16;
          }
          return data_format::u8;
        }
        if (bit_depth == 16) {
          if (decoder_->output_format().component_type.prefers(data_format::u8)) {
            png_set_strip_16(png.ptr);
            return data_format::u8;
          }
          return data_format::u16;
        }

        throw decode_error{codec::png, "no suitable format for bit_depth "
          + std::to_string(bit_depth) + " found"};
      }





      static void read(png_structp png_ptr, png_bytep data, size_t length) {
        auto* pdec = static_cast<png_decoder*>(png_get_io_ptr(png_ptr));

        if (pdec->decoder_->input().read(
              std::as_writable_bytes(std::span{data, length})) != length) {
          throw decode_error{codec::png, "failed to read: unexpected eof"};
        }
      }



      [[noreturn]] static void error_function(png_structp /*png*/, png_const_charp msg) {
        throw decode_error{codec::png, msg};
      }



      static void warn_function(png_structp png_ptr, png_const_charp msg) {
        auto* pdec = static_cast<png_decoder*>(png_get_error_ptr(png_ptr));

        pdec->decoder_->warn(msg);
      }
  };
}





void decode_png(details::decoder& dec) {
  png_decoder pdec{dec};
  pdec.decode();
}
