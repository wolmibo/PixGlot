#include "../decoder.hpp"
#include "pixglot/codecs.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/square-isometry.hpp"
#include "pixglot/utils/cast.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <jpeglib.h>



namespace {
#if 0
  [[nodiscard]] constexpr optional<square_isometry> convert_orientation(uint32_t or) {
    using enum square_isometry;

    switch (or) {
      case 1: return identity;
      case 2: return flip_x;
      case 3: return rotate_half;
      case 4: return flip_y;
      case 5: return transpose;
      case 6: return rotate_ccw;
      case 7: return anti_transpose;
      case 8: return rotate_cw;

      default:
        return {};
    }
  }
#endif



  // Derived from the implementation of jpeg_stdio_src in libjpeg-turbo
  class source_mgr {
    public:
      jpeg_source_mgr pub; //NOLINT(*non-private-member-*)

      ~source_mgr() = default;
      source_mgr(const source_mgr&) = delete;
      source_mgr(source_mgr&&)      = delete;
      source_mgr& operator=(const source_mgr&) = delete;
      source_mgr& operator=(source_mgr&&)      = delete;

      explicit source_mgr(reader& read) :
        pub{.next_input_byte   = nullptr,
            .bytes_in_buffer   = 0,

            .init_source       = init_source,
            .fill_input_buffer = fill_input_buffer,
            .skip_input_data   = skip_input_data,
            .resync_to_restart = jpeg_resync_to_restart,
            .term_source       = term_source},
        reader_{&read},
        buffer_(4096) {}



    private:
      reader*                reader_;
      std::vector<std::byte> buffer_;



      [[nodiscard]] static source_mgr& myself(j_decompress_ptr cinfo) {
        return *reinterpret_cast<source_mgr*>(cinfo->src); //NOLINT(*reinterpret-cast)
      }



      static void init_source(j_decompress_ptr /*unused*/) {}



      [[nodiscard]] static boolean fill_input_buffer(j_decompress_ptr cinfo) {
        auto& self = myself(cinfo);

        self.pub.bytes_in_buffer = self.reader_->read(self.buffer_);
        self.pub.next_input_byte = utils::byte_pointer_cast<JOCTET>(self.buffer_.data());

        if (self.pub.bytes_in_buffer == 0) {
          throw decode_error{codec::jpeg, "unexpected eof"};
        }

        return TRUE;
      }



      static void skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
        if (num_bytes < 0) {
          throw decode_error{codec::jpeg, "unable to skip backwards"};
        }
        size_t nb = num_bytes;

        auto& self = myself(cinfo);

        if (nb >= self.pub.bytes_in_buffer) {
          self.reader_->skip(nb - self.pub.bytes_in_buffer);
          if (fill_input_buffer(cinfo) == FALSE) {
            throw decode_error{codec::jpeg, "unable to read from source"};
          }
        } else {
          self.pub.next_input_byte += nb; //NOLINT(*pointer-arithmetic)
          self.pub.bytes_in_buffer -= nb;
        }
      }



      static void term_source(j_decompress_ptr /*unused*/) {}
  };





  struct jds_destroyer {
    void operator()(jpeg_decompress_struct* cinfo) const {
      jpeg_destroy_decompress(cinfo);
      delete cinfo; //NOLINT
    }
  };





  class jpeg_decoder : public decoder {
    public:
      explicit jpeg_decoder(decoder&& parent) :
        decoder(std::move(parent)),
        cinfo_{new jpeg_decompress_struct()},
        src_mgr_{input()}
      {
        cinfo_->client_data = this;

        init_error();
        cinfo_->err = &err_mgr_;

        jpeg_create_decompress(cinfo_.get());

        decode();
      }



    private:
      std::unique_ptr<jpeg_decompress_struct, jds_destroyer> cinfo_;
      jpeg_error_mgr                                         err_mgr_{};
      source_mgr                                             src_mgr_;



      void decode() {
        //NOLINTNEXTLINE(*reinterpret-cast)
        cinfo_->src = reinterpret_cast<jpeg_source_mgr*>(&src_mgr_);

        jpeg_read_header(cinfo_.get(), TRUE);

        auto pf = make_colorspace_compatible();

        jpeg_start_decompress(cinfo_.get()); {
          pixel_buffer pixels{cinfo_->image_width, cinfo_->image_height, pf};
          transfer_data(pixels);

          append_frame(frame {
            .pixels      = std::move(pixels),
            .orientation = square_isometry::identity,

            .alpha       = alpha_mode::none,
            .gamma       = gamma_s_rgb,
            .endianess   = std::endian::native,

            .duration    = std::chrono::microseconds{0}
          });

        } jpeg_finish_decompress(cinfo_.get());
      }



      [[nodiscard]] pixel_format make_colorspace_compatible() {
        if (format_out().gamma.has_preference()) {
          cinfo_->output_gamma = *format_out().gamma;
        }

        if (cinfo_->num_components != 1
            || format_out().expand_gray_to_rgb.prefers(true)) {
          cinfo_->out_color_space = JCS_RGB;
          return {
            .format   = data_format::u8,
            .channels = color_channels::rgb,
          };
        }
        cinfo_->out_color_space = JCS_GRAYSCALE;
        return {
          .format   = data_format::u8,
          .channels = color_channels::gray,
        };
      }



      void transfer_data(pixel_buffer& pixbuf) {
        uint32_t row_count = cinfo_->rec_outbuf_height;
        std::vector<JSAMPROW> rows{row_count};

        while (cinfo_->output_scanline < cinfo_->output_height) {
          for (uint32_t y = 0; y < row_count; ++y) {
            rows[y] = utils::byte_pointer_cast<std::remove_pointer_t<JSAMPROW>>(
              pixbuf.row_bytes(y + cinfo_->output_scanline).data());
          }

          jpeg_read_scanlines(cinfo_.get(), rows.data(), row_count);

          progress(cinfo_->output_scanline, cinfo_->output_height);
        }
      }



      void init_error() {
        jpeg_std_error(&err_mgr_);

        err_mgr_.error_exit     = error_exit;
        err_mgr_.output_message = output_message;
      }



      static std::string message(j_common_ptr cinfo) {
        std::string message(JMSG_LENGTH_MAX, '\0');
        (*(cinfo->err->format_message))(cinfo, message.data());

        if (auto pos = message.find('\0'); pos != std::string::npos) {
          message.resize(pos);
        }

        return message;
      }



      [[noreturn]] static void error_exit(j_common_ptr cinfo) {
        throw decode_error{codec::jpeg, message(cinfo)};
      }



      void warn(const std::string& msg) {
        decoder::warn(msg);
      }



      static void output_message(j_common_ptr cinfo) {
        static_cast<jpeg_decoder*>(cinfo->client_data)->warn(message(cinfo));
      }
  };
}



image decode_jpeg(decoder&& dec) {
  jpeg_decoder jdec{std::move(dec)};

  return jdec.finalize_image();
}
