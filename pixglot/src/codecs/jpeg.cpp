#include "config.hpp"
#include "pixglot/codecs.hpp"
#include "pixglot/details/decoder.hpp"
#include "pixglot/details/string-bytes.hpp"
#include "pixglot/details/xmp.hpp"
#include "pixglot/exception.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/frame-source-info.hpp"
#include "pixglot/metadata.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/square-isometry.hpp"
#include "pixglot/utils/cast.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <vector>

#include <jpeglib.h>

using namespace pixglot;



namespace {
  constexpr int JPEG_APP1 = JPEG_APP0 + 1;



  class exif_decoder {
    public:
      [[nodiscard]] static bool is_exif(std::span<const std::byte> buffer) {
        return buffer.size() > magic.size()
          && std::ranges::equal(magic, buffer.subspan(0, magic.size()));
      }



      explicit exif_decoder(std::span<const std::byte> buffer)
        : buffer_{buffer}
      {
        if (!is_exif(buffer_)) {
          throw decode_error{codec::jpeg, "exif: invalid file format"};
        }
        buffer_ = buffer.subspan(magic.size());

        read_tiff();
      }



      [[nodiscard]] square_isometry orientation() const {
        return orientation_;
      }



    private:
      static constexpr std::array<std::byte, 6> magic {
        std::byte{'E'}, std::byte{'x'}, std::byte{'i'}, std::byte{'f'},
        std::byte{0}, std::byte{0}
      };

      static constexpr std::array<std::byte, 2> little_e{std::byte{'I'}, std::byte{'I'}};
      static constexpr std::array<std::byte, 2> big_e   {std::byte{'M'}, std::byte{'M'}};



      std::span<const std::byte> buffer_;
      std::endian                byte_order_{std::endian::native};

      square_isometry            orientation_{};


#if 0
      enum class value_type : uint16_t {
        byte      = 1,
        ascii     = 2,
        ushort    = 3,
        ulong     = 4,
        rational  = 5,
        undefined = 7,
        slong     = 8,
        srational = 10,
      };
#endif


      enum class attribute : uint16_t {
        orientation = 0x112,
      };



#if 0
      using ifd_value = std::variant<
        std::array<uint8_t, 4>,
        std::array<uint16_t, 2>,
        uint32_t,
        int32_t,
        std::pair<value_type, size_t>
      >;

      struct ifd_entry {
        attribute tag;
        uint32_t  count;
        ifd_value value;
      };





      [[nodiscard]] ifd_entry read_ifd_entry(size_t offset) {
        return ifd_entry{
          .tag   = static_cast<attribute>(int_at<uint16_t>(offset)),
          .count = int_at<uint32_t>(offset + 4),
          .value = read_ifd_value(offset + 8, int_at<uint16_t>(offset + 2)),
        };
      }



      [[nodiscard]] ifd_value read_ifd_value(size_t offset, uint16_t type) {
        switch (static_cast<value_type>(type)) {
          case value_type::byte:
            return std::array<uint8_t, 4> {
              int_at<uint8_t>(offset + 3),
              int_at<uint8_t>(offset + 2),
              int_at<uint8_t>(offset + 1),
              int_at<uint8_t>(offset + 0)
            };

          case value_type::ushort:
            return std::array<uint16_t, 2> {
              int_at<uint16_t>(offset + 2),
              int_at<uint16_t>(offset + 0)
            };

          case value_type::ulong:
            return int_at<uint32_t>(offset);
          case value_type::slong:
            return int_at<int32_t>(offset);

          case value_type::ascii:
          case value_type::rational:
          case value_type::undefined:
          case value_type::srational:
          default: {
            auto off = int_at<uint32_t>(offset);
            if (off >= buffer_.size()) {
              off = 0;
            }
            return std::make_pair(static_cast<value_type>(type), off);
          }
        }
      }
#endif


      void load_entries(size_t offset) {
        while (offset != 0) {
          auto count = int_at<uint16_t>(offset);
          offset += 2;
          for (uint16_t i = 0; i < count; ++i) {
            if (static_cast<attribute>(int_at<uint16_t>(offset)) ==
                attribute::orientation) {
              orientation_ = convert_orientation(int_at<uint16_t>(offset + 8));
            }
            offset += 12;
          }
          offset = int_at<uint32_t>(offset);
        }
      }



      void read_tiff() {
        if (buffer_.size() < 8) {
          throw decode_error{codec::jpeg, "exif: incomplete tiff header"};
        }

        if (std::ranges::equal(buffer_.subspan(0, little_e.size()), little_e)) {
          byte_order_ = std::endian::little;
        } else if (std::ranges::equal(buffer_.subspan(0, big_e.size()), big_e)) {
          byte_order_ = std::endian::big;
        }

        if (int_at<uint16_t>(2) != 42) {
          throw decode_error{codec::jpeg, "exif: tiff: wrong byte order"};
        }

        load_entries(int_at<uint32_t>(4));
      }





      template<typename T>
      [[nodiscard]] T int_at(size_t offset) {
        if (offset > buffer_.size() - sizeof(T)) {
          throw decode_error{codec::jpeg, "exif: unexpected eof"};
        }

        if constexpr (sizeof(T) > 1) {
          T value{};
          std::ranges::copy(buffer_.subspan(offset, sizeof(T)),
              std::as_writable_bytes(std::span{&value, 1}).begin());

          if (byte_order_ == std::endian::native) {
            return value;
          }
          return std::byteswap(value);
        } else {
          return static_cast<T>(buffer_[offset]);
        }
      }



      [[nodiscard]] static square_isometry convert_orientation(uint16_t orientation) {
        using enum square_isometry;

        switch (orientation) {
          case 1: return identity;
          case 2: return flip_x;
          case 3: return rotate_half;
          case 4: return flip_y;
          case 5: return transpose;
          case 6: return rotate_ccw;
          case 7: return anti_transpose;
          case 8: return rotate_cw;

          default:
            return identity;
        }
      }
  };




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



      [[nodiscard]] static source_mgr& get(j_decompress_ptr cinfo) {
        return *reinterpret_cast<source_mgr*>(cinfo->src); //NOLINT(*reinterpret-cast)
      }



      void fill(std::span<std::byte> buffer) {
        size_t from_buffer = std::min(buffer.size(), pub.bytes_in_buffer);

        std::ranges::copy(std::as_bytes(std::span{pub.next_input_byte, from_buffer}),
            buffer.begin());
        buffer = buffer.subspan(from_buffer);

        pub.next_input_byte += from_buffer; //NOLINT(*pointer-arithmetic)
        pub.bytes_in_buffer -= from_buffer;

        if (buffer.empty()) {
          return;
        }

        if (reader_->read(buffer) != buffer.size()) {
          throw decode_error{codec::jpeg, "uexpected eof"};
        }
      }



    private:
      reader*                reader_;
      std::vector<std::byte> buffer_;



      static void init_source(j_decompress_ptr /*unused*/) {}



      [[nodiscard]] static boolean fill_input_buffer(j_decompress_ptr cinfo) {
        auto& self = get(cinfo);

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

        auto& self = get(cinfo);

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





  [[nodiscard]] color_channels convert_color_channels(J_COLOR_SPACE cspace) {
    switch (cspace) {
      case JCS_GRAYSCALE: return color_channels::gray;
      case JCS_RGB:       return color_channels::rgb;
      case JCS_EXT_RGBA:  return color_channels::rgba;
      default: break;
    }
    throw decode_error{codec::jpeg, "unsupported color space"};
  }





  [[nodiscard]] bool is_xmp(std::span<std::byte>& data) {
    std::string_view header{"http://ns.adobe.com/xap/1.0/"};

    bool xmp = data.size() > header.size() &&
      data[header.size()] == std::byte{0} &&
      std::ranges::equal(data.subspan(0, header.size()),
                         std::as_bytes(std::span{header}));

    data = data.subspan(header.size() + 1);

    return xmp;
  }





  [[nodiscard]] std::string density_unit(UINT8 du) {
    switch (du) {
      case 1:  return "dpi";
      case 2:  return "dpcm";
      default: return "u";
    }
  }


  [[nodiscard]] std::vector<metadata::key_value> metadata_from_JFIF(
      jpeg_decompress_struct* cinfo
  ) {
    if (cinfo->saw_JFIF_marker == FALSE) {
      return {};
    }

    auto du = density_unit(cinfo->density_unit);

    return {
      {"jfif.version", std::to_string(cinfo->JFIF_major_version) + "." +
                       std::to_string(cinfo->JFIF_minor_version)},
      {"jfif.par",     std::to_string(static_cast<float>(cinfo->X_density) /
                                      static_cast<float>(cinfo->Y_density))},

      {"jfif.densityX", std::to_string(cinfo->X_density) + du},
      {"jfif.densityY", std::to_string(cinfo->Y_density) + du},
    };
  }





  struct jds_destroyer {
    void operator()(jpeg_decompress_struct* cinfo) const {
      jpeg_destroy_decompress(cinfo);
      delete cinfo; //NOLINT
    }
  };





  class jpeg_decoder {
    public:
      explicit jpeg_decoder(details::decoder& decoder) :
        decoder_{&decoder},
        cinfo_  {new jpeg_decompress_struct()},
        src_mgr_{decoder_->input()}
      {
        cinfo_->client_data = this;

        init_error();
        cinfo_->err = &err_mgr_;

        jpeg_create_decompress(cinfo_.get());

        decode();
      }



    private:
      details::decoder*                                      decoder_;
      std::unique_ptr<jpeg_decompress_struct, jds_destroyer> cinfo_;
      jpeg_error_mgr                                         err_mgr_{};
      source_mgr                                             src_mgr_;
      square_isometry                                        orientation_{};



      void decode() {
        //NOLINTNEXTLINE(*reinterpret-cast)
        cinfo_->src = reinterpret_cast<jpeg_source_mgr*>(&src_mgr_);

        jpeg_set_marker_processor(cinfo_.get(), JPEG_APP1, process_marker);

        jpeg_read_header(cinfo_.get(), TRUE);

        auto pf = make_colorspace_compatible();

        decoder_->image().metadata().append_move(metadata_from_JFIF(cinfo_.get()));

        auto& frame = decoder_->begin_frame(cinfo_->image_width, cinfo_->image_height,
                                            pf);

        frame.alpha_mode (alpha_mode::none);
        frame.orientation(orientation_);

        set_frame_source_info(frame.source_info());

        if (decoder_->wants_pixel_transfer()) {
          decoder_->begin_pixel_transfer();

          jpeg_start_decompress(cinfo_.get());
          transfer_data(decoder_->target());
          jpeg_finish_decompress(cinfo_.get());

          decoder_->finish_pixel_transfer();
        }
        decoder_->finish_frame();
      }



      [[nodiscard]] pixel_format make_colorspace_compatible() {
        if (decoder_->output_format().gamma().preferred()) {
          cinfo_->output_gamma = *decoder_->output_format().gamma();
        }

        if (cinfo_->num_components != 1
            || decoder_->output_format().expand_gray_to_rgb().prefers(true)) {
          cinfo_->out_color_space = JCS_RGB;
#ifdef JCS_ALPHA_EXTENSIONS
          if (decoder_->output_format().fill_alpha().prefers(true)) {
            cinfo_->out_color_space = JCS_EXT_RGBA;
          }
#endif
        } else {
          cinfo_->out_color_space = JCS_GRAYSCALE;
        }

        return {
          .format   = data_format::u8,
          .channels = convert_color_channels(cinfo_->out_color_space)
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

          decoder_->frame_mark_ready_until_line(cinfo_->output_scanline);
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
        decoder_->warn(msg);
      }



      static void output_message(j_common_ptr cinfo) {
        static_cast<jpeg_decoder*>(cinfo->client_data)->warn(message(cinfo));
      }



      static boolean process_marker(j_decompress_ptr cinfo) {
        uint16_t length_be{0};
        source_mgr::get(cinfo).fill(std::as_writable_bytes(std::span{&length_be, 1}));

        std::vector<std::byte> buffer(std::byteswap(length_be) - 2);
        source_mgr::get(cinfo).fill(buffer);

        auto* self = static_cast<jpeg_decoder*>(cinfo->client_data);

        std::span span{buffer};

#ifdef PIXGLOT_WITH_XMP
        if (cinfo->unread_marker == JPEG_APP1 && is_xmp(span)) {
          auto str = details::string_from(span.data(), span.size());

          if (details::fill_xmp_metadata(str, self->decoder_->image().metadata(),
                                         *self->decoder_)) {
            self->decoder_->image().metadata().emplace("pixglot.xmp.raw", std::move(str));
          } else {
            self->decoder_->warn("found invalid xmp data");
          }

          return TRUE;
        }
#endif

        if (cinfo->unread_marker == JPEG_APP1 && exif_decoder::is_exif(buffer)) {
          try {
            exif_decoder exif{buffer};
            self->orientation_ = exif.orientation();
          } catch (base_exception& ex) {
            self->warn(std::string{ex.message()});
          } catch (std::exception& ex) {
            self->warn(ex.what());
          }
        }

        return TRUE;
      }



      void set_frame_source_info(frame_source_info& fsi) const {
        switch (cinfo_->jpeg_color_space) {
          case JCS_GRAYSCALE: fsi.color_model(color_model::value); break;
          case JCS_RGB:       fsi.color_model(color_model::rgb);   break;
          case JCS_YCbCr:     fsi.color_model(color_model::yuv);   break;

          default: fsi.color_model(color_model::unknown); break;
        }

        auto x = cinfo_->max_h_samp_factor;
        auto y = cinfo_->max_v_samp_factor;

        if (x == 2 && y == 1) {
          fsi.subsampling(chroma_subsampling::cs422);
        } else if (x == 2 && y == 2) {
          fsi.subsampling(chroma_subsampling::cs420);
        } else {
          fsi.subsampling(chroma_subsampling::cs444);
        }


        auto color_depth = static_cast<data_source_format>(cinfo_->data_precision);

        fsi.color_model_format({color_depth, color_depth, color_depth,
                                data_source_format::none});

      }
  };
}



void decode_jpeg(details::decoder& dec) {
  jpeg_decoder jdec{dec};
}
