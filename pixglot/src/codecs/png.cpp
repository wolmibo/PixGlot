#include "config.hpp"
#include "pixglot/details/decoder.hpp"
#include "pixglot/details/string-bytes.hpp"
#include "pixglot/details/xmp.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/frame-source-info.hpp"
#include "pixglot/metadata.hpp"
#include "pixglot/square-isometry.hpp"
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





  frame_source_info create_frame_source_info(const png_guard& png) {
    frame_source_info fsi;

    auto color_type = png_get_color_type(png.ptr, png.info);

    if ((color_type & PNG_COLOR_MASK_PALETTE) != 0) {
      fsi.color_model(color_model::palette);
    } else if ((color_type & PNG_COLOR_MASK_COLOR) != 0) {
      fsi.color_model(color_model::rgb);
    } else {
      fsi.color_model(color_model::value);
    }

    auto color_depth =
      static_cast<data_source_format>(png_get_bit_depth(png.ptr, png.info));

    auto alpha_depth = data_source_format::none;

    if ((color_type & PNG_COLOR_MASK_ALPHA) != 0) {
      alpha_depth = color_depth;
    } else if (png_get_valid(png.ptr, png.info, PNG_INFO_tRNS) != 0) {
      if (fsi.color_model() == color_model::palette) {
        alpha_depth = data_source_format::u8;
      } else {
        alpha_depth = data_source_format::index;
      }
    }

    fsi.color_model_format({color_depth, color_depth, color_depth, alpha_depth});

    return fsi;
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

        auto fsi = create_frame_source_info(png);

        pixel_format format {
          .format   = make_format_compatible(),
          .channels = make_color_channels_compatible()
        };

        auto& frame = decoder_->begin_frame(
            png_get_image_width(png.ptr, png.info),
            png_get_image_height(png.ptr, png.info),
            format,
            make_endian_compatible(format.format)
        );

        frame.alpha_mode(make_alpha_mode_compatible(format.channels));
        frame.source_info() = std::move(fsi);

        fill_metadata(decoder_->image().metadata());

        frame.orientation(orientation_from_metadata(decoder_->image().metadata())
                          .value_or(square_isometry::identity));

        if (decoder_->wants_pixel_transfer()) {
          png_read_update_info(png.ptr, png.info);

          decoder_->begin_pixel_transfer();
          transfer_data(decoder_->target());
          decoder_->finish_pixel_transfer();
        }

        decoder_->finish_frame();
      }



    private:
      details::decoder* decoder_;
      png_guard         png;



      void fill_metadata(metadata& md) const {
        png_textp text{nullptr};
        int       num_text{0};

        png_get_text(png.ptr, png.info, &text, &num_text);

        if (num_text < 0) {
          decoder_->warn("negative number of text blocks");
          return;
        }

#ifdef PIXGLOT_WITH_XMP
        size_t xmp{0};
#endif

        for (const auto& png_text: std::span{text, static_cast<size_t>(num_text)}) {
          auto key   = details::string_view_from(png_text.key, 79);
          auto value = details::string_from(png_text.text);
#ifdef PIXGLOT_WITH_XMP
          if (key == "XML:com.adobe.xmp") {
            if (!fill_xmp_metadata(value, decoder_->image().metadata(), *decoder_)) {
              decoder_->warn("found invalid xmp data");
            }
            md.emplace("pixglot.xmp" + (xmp > 0 ? std::to_string(xmp) : "") + ".raw",
                       std::move(value));
              ++xmp;
            continue;
          }
#endif
          md.emplace(std::string{key}, std::move(value));
        }
      }





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
          decoder_->output_format().expand_gray_to_rgb().prefers(true)) {
          png_set_gray_to_rgb(png.ptr);

          png_color_type_add_color(color_type);
        }

        if (png_get_valid(png.ptr, png.info, PNG_INFO_tRNS) != 0) {
          png_set_tRNS_to_alpha(png.ptr);

          png_color_type_add_alpha(color_type);
        }

        if (decoder_->output_format().fill_alpha().prefers(true)) {
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
          if (decoder_->output_format().alpha_mode().prefers(alpha_mode::premultiplied)) {
            png_set_alpha_mode(png.ptr, PNG_ALPHA_PREMULTIPLIED, PNG_GAMMA_LINEAR);

            return alpha_mode::premultiplied;
          }
          return alpha_mode::straight;
        }
        return alpha_mode::none;
      }



      [[nodiscard]] std::endian make_endian_compatible(data_format df) {
        if (byte_size(df) > 1 &&
            decoder_->output_format().endian().prefers(std::endian::little)) {
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
          if (decoder_->output_format().data_format().prefers(data_format::u16)) {
            png_set_expand_16(png.ptr);
            return data_format::u16;
          }
          return data_format::u8;
        }
        if (bit_depth == 16) {
          if (decoder_->output_format().data_format().prefers(data_format::u8)) {
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
