#include "pixglot/buffer.hpp"
#include "pixglot/conversions.hpp"
#include "pixglot/details/decoder.hpp"
#include "pixglot/exception.hpp"
#include "pixglot/frame-source-info.hpp"
#include "pixglot/pixel-buffer.hpp"
#include "pixglot/pixel-format-conversion.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/square-isometry.hpp"
#include "pixglot/utils/cast.hpp"

#include <cctype>
#include <variant>

using namespace pixglot;



namespace {
  [[nodiscard]] uint32_t value_of_char(char c) {
    if ('0' <= c && c <= '9') {
      return c - '0';
    }

    throw decode_error{codec::ppm,
      "illegal character with code " + std::to_string(static_cast<int>(c))
        + " in unsigned integer"};
  }



  [[nodiscard]] u32 parse_u32(std::string_view value, u32 max) {
    if (value.size() > std::string_view{"65535"}.size()) {
      throw decode_error{codec::ppm,
        "trying to decode integer which will exceed u16::max"};
    }
    u32 out = 0;

    for (char c: value) {
      out = out * 10 + value_of_char(c);
    }

    if (out > max) {
      throw decode_error{codec::ppm,
        "found value " + std::to_string(out) +
        " which exceeds range of " + std::to_string(max)};
    }

    return out;
  }



  [[nodiscard]] float parse_flp(std::string_view value) {
    try {
      std::string v{value};
      return std::stof(v);
    } catch (std::exception& ex) {
      throw decode_error{codec::ppm,
        std::string{"failed to parse string as float: "} + ex.what()};
    }
  }



  class ppm_reader {
    public:
      ppm_reader(const ppm_reader&) = delete;
      ppm_reader(ppm_reader&&)      = delete;
      ppm_reader& operator=(const ppm_reader&) = delete;
      ppm_reader& operator=(ppm_reader&&)      = delete;

      ~ppm_reader() = default;

      explicit ppm_reader(reader& input) :
        data_{input.size()},
        remainder_{data_}
      {
        if (input.read(data_.as_bytes()) != data_.size()) {
          throw decode_error{codec::ppm, "unexpected eof"};
        }
      }



      [[nodiscard]] std::optional<std::string_view> next_word() {
        skip_whitespace();

        if (remainder_.empty()) {
          return {};
        }

        while (remainder_[0] == '#') {
          consume_until(remainder_.begin()+1);
          skip_comment();
          skip_whitespace();
        }

        if (remainder_.empty()) {
          return {};
        }

        return read_until(std::ranges::find_if(remainder_, is_whitespace));
      }



      [[nodiscard]] std::span<const std::byte> read_binary_to_end() {
        auto pos = std::ranges::find_if(remainder_, [](char c) { return c == '\n'; });
        if (pos != remainder_.end()) {
          pos++;
        }
        consume_until(pos);

        return as_bytes(remainder_);
      }





    private:
      buffer<char>                  data_;
      std::span<char>               remainder_;
      std::vector<std::string_view> comments_;



      [[nodiscard]] static bool is_whitespace(char c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
      }



      [[nodiscard]] std::string_view read_until(std::span<char>::iterator end) {
        std::string_view word {
          remainder_.begin(),
          end
        };
        consume_until(end);
        return word;
      }



      void skip_comment() {
        skip_whitespace();

        auto comment = read_until(std::ranges::find_if(remainder_,
                                  [](char c) { return c == '\n'; }));
        if (!comment.empty()) {
          comments_.emplace_back(comment);
        }
      }



      void consume_until(std::span<char>::iterator end) {
        remainder_ = std::span{end, remainder_.end()};
      }



      void skip_whitespace() {
        consume_until(std::ranges::find_if_not(remainder_, is_whitespace));
      }
  };



  constexpr gray<u8> black_pixel{.v = 0x00};
  constexpr gray<u8> white_pixel{.v = 0xff};



  [[nodiscard]] gray<u8> char_to_bit_pixel(char b) {
    if (b == '0') {
      return white_pixel;
    }
    if (b == '1') {
      return black_pixel;
    }

    throw decode_error{codec::ppm, "unsupported character with code " +
      std::to_string(static_cast<int>(static_cast<unsigned char>(b))) +
      " in bitmap"};
  }



  void fill_remaining_pixel_buffer(
      pixel_buffer& target,
      size_t        row_index,
      size_t        column_index
  ) {
    if (row_index == target.height() && column_index == 0) {
      return;
    }

    if (column_index >= target.width() || row_index >= target.height()) {
      throw decode_error{codec::ppm, "decoder exceeded buffer bounds"};
    }

    std::ranges::fill(
      target.data().subspan(target.stride() * row_index
        + column_index * target.format().size()),
      std::byte{0x00}
    );
  }



  void transfer_ascii_bitmap(ppm_reader& source, pixel_buffer& target) {
    size_t row_index   {0};
    size_t column_index{0};

    auto row = target.row<gray<u8>>(row_index);

    for (auto word = source.next_word(); word; word = source.next_word()) {
      for (char b: *word) {
        row[column_index] = char_to_bit_pixel(b);

        if (++column_index >= row.size()) {
          column_index = 0;
          if (++row_index >= target.height()) {
            return;
          }
          row = target.row<gray<u8>>(row_index);
        }
      }
    }

    fill_remaining_pixel_buffer(target, row_index, column_index);
  }



  void transfer_binary_bitmap(std::span<const std::byte> source, pixel_buffer& target) {
    static constexpr std::array<const gray<u8>, 2> lookup{white_pixel, black_pixel};

    size_t row_index{0};
    size_t column_index{0};

    auto row = target.row<gray<u8>>(row_index);

    static_assert(pixel_buffer::alignment % 8 == 0);

    for (std::byte b: source) {
      for (size_t i = 0; i < 8; ++i, column_index++) {
        //NOLINTNEXTLINE(*constant-array-index)
        row[column_index] = lookup[(static_cast<size_t>(b) >> (7 - i)) & 0x1];
      }
      if (column_index >= row.size()) {
        column_index = 0;
        if (++row_index >= target.height()) {
          return;
        }
        row = target.row<gray<u8>>(row_index);
      }
    }

    fill_remaining_pixel_buffer(target, row_index, column_index);
  }



  void transfer_binary_data(std::span<const std::byte> source, pixel_buffer& target) {
    for (size_t y = 0; y < target.height(); y++) {
      auto row = target.row_bytes(y);

      if (source.size() < row.size()) {
        auto start = copy(source.begin(), source.end(), row.begin());
        fill(start, row.end(), std::byte{0x00});

        fill_remaining_pixel_buffer(target, y + 1, 0);
        return;
      }

      copy_n(source.begin(), row.size(), row.begin());

      source = source.subspan(row.size());
    }
  }



  template<data_format_type DFT>
  [[nodiscard]] std::span<DFT> components_of_row(pixel_buffer& pixels, size_t y) {
    if (pixels.format().format != data_format_from<DFT>::value) {
      throw bad_pixel_format{pixels.format()};
    }

    return utils::interpret_as_n<DFT>(pixels.row_bytes(y),
              pixels.width() * n_channels(pixels.format().channels));
  }



  template<data_format_type DFT>
  void transfer_ascii_data(ppm_reader& reader, pixel_buffer& target, u32 range) {
    size_t column_index{0};
    size_t row_index{0};

    auto row = components_of_row<DFT>(target, row_index);

    for (auto word = reader.next_word(); word; word = reader.next_word()) {
      row[column_index] = parse_u32(*word, range);

      if (++column_index >= row.size()) {
        column_index = 0;

        if (++row_index >= target.height()) {
          return;
        }

        row = components_of_row<DFT>(target, row_index);
      }
    }

    fill_remaining_pixel_buffer(target, row_index, column_index);
  }



  template<data_format_type DFT, data_format_type UP>
  void adjust_range(pixel_buffer& pixels, DFT range) {
    for (size_t y = 0; y < pixels.height(); ++y) {
      for (auto& component: components_of_row<DFT>(pixels, y)) {
        component = std::clamp<UP>(
          (static_cast<UP>(component)
            * static_cast<UP>(conversions::info<DFT>::range_max)) / range,
          0,
          static_cast<UP>(conversions::info<DFT>::range_max)
        );
      }
    }
  }



  enum class ppm_type {
    bits,
    integer,
    flp
  };



  struct ppm_header {
    ppm_type     type;
    pixel_format format;
    bool         ascii;

    size_t       width {0};
    size_t       height{0};

    std::endian              endianess{std::endian::big};
    std::variant<float, u16> range{static_cast<u16>(1)};



    explicit ppm_header(ppm_reader& reader) {
      char identifier{read_header(reader)};

      type   = type_from_header(identifier);
      format = pixel_format {
        .format   = data_format_from_header(identifier),
        .channels = color_channels_from_header(identifier)
      };

      ascii = ascii_from_header(identifier);

      if (auto w_str = reader.next_word()) {
        width = parse_u32(*w_str, 0xffff);
      } else {
        throw decode_error{codec::ppm, "ppm file contains no width information"};
      }

      if (auto h_str = reader.next_word()) {
        height = parse_u32(*h_str, 0xffff);
      } else {
        throw decode_error{codec::ppm, "ppm file contains no height information"};
      }



      if (type != ppm_type::bits) {
        if (auto range = reader.next_word()) {
          parse_range(*range);
        } else {
          throw decode_error{codec::ppm, "ppm file contains no range information"};
        }
      }
    }



    void fill_frame_source_info(frame_source_info& fsi) const {
      fsi.color_model(has_color(format.channels) ? color_model::rgb : color_model::value);

      auto dsf = source_format();
      fsi.color_model_format({dsf, dsf, dsf, data_source_format::none});
    }



    private:
      [[nodiscard]] data_source_format source_format() const {
        if (ascii) {
          return data_source_format::ascii;
        }

        if (type == ppm_type::bits) {
          return data_source_format::u1;
        }

        return data_source_format_from(format.format);
      }



      void parse_range(std::string_view range_str) {
        if (type == ppm_type::integer) {
          auto rcand = parse_u32(range_str, 0xffff);
          if (rcand > 0xff) {
            format.format = data_format::u16;
          }
          range = static_cast<u16>(rcand);
        } else if (type == ppm_type::flp) {
          auto rcand = parse_flp(range_str);
          range = std::abs(rcand);

          endianess = (rcand > 0 ? std::endian::big : std::endian::little);
        }
      }



      [[nodiscard]] static bool is_ppm_format(char c) {
        return c == 'f' || c == 'F' || ('1' <= c && c <= '6');
      }



      [[nodiscard]] static char read_header(ppm_reader& reader) {
        if (auto word = reader.next_word()) {
          if (word->size() == 2 && (*word)[0] == 'P' && is_ppm_format((*word)[1])) {
            return (*word)[1];
          }
        }

        throw decode_error{codec::ppm, "invalid ppm header"};
      }



      [[nodiscard]] static ppm_type type_from_header(char c) {
        switch (c) {
          case 'f': case 'F': return ppm_type::flp;
          case '1': case '4': return ppm_type::bits;
          default:            return ppm_type::integer;
        }
      }



      [[nodiscard]] static data_format data_format_from_header(char c) {
        if (c == 'f' || c == 'F') {
          return data_format::f32;
        }
        return data_format::u8;
      }



      [[nodiscard]] static color_channels color_channels_from_header(char c) {
        if (c == 'F' || c == '3' || c == '6') {
          return color_channels::rgb;
        }
        return color_channels::gray;
      }



      [[nodiscard]] static bool ascii_from_header(char c) {
        return '1' <= c && c <= '3';
      }
  };





  class ppm_decoder {
    public:
      explicit ppm_decoder(details::decoder& decoder) :
        decoder_{&decoder},
        reader_ {decoder_->input()},
        header_ {reader_}
      {}



      void decode() {
        frame frame_init{pixel_buffer{header_.width, header_.height, header_.format,
                         current_endianess()}};

        header_.fill_frame_source_info(frame_init.source_info());

        auto& frame = decoder_->begin_frame(std::move(frame_init));

        frame.orientation(current_orientation());
        frame.alpha_mode (alpha_mode::none);

        if (header_.ascii) {
          transfer_ascii(decoder_->target());
        } else {
          transfer_binary(decoder_->target());
        }

        decoder_->finish_frame();
      }



    private:
      details::decoder* decoder_;
      ppm_reader reader_;
      ppm_header header_;



      [[nodiscard]] float current_gamma() const {
        return is_float(header_.format.format) ? gamma_linear : gamma_s_rgb;
      }



      [[nodiscard]] std::endian current_endianess() {
        return byte_size(header_.format.format) > 1 ? header_.endianess :
          (decoder_->output_format().endian().prefers(std::endian::big)
            ? std::endian::big : std::endian::little);
      }



      [[nodiscard]] square_isometry current_orientation() const {
        return is_float(header_.format.format)
          ? square_isometry::flip_y : square_isometry::identity;
      }



      void transfer_ascii(pixel_buffer& pixels) {
        switch (header_.type) {
          case ppm_type::bits:
            transfer_ascii_bitmap(reader_, pixels);
            break;
          case ppm_type::integer:
            if (pixels.format().format == data_format::u8) {
              transfer_ascii_data<u8>(reader_, pixels, get<u16>(header_.range));
            } else {
              transfer_ascii_data<u16>(reader_, pixels, get<u16>(header_.range));
            }
            adjust_range(pixels);
            break;
          default:
            break;
        }
      }



      void transfer_binary(pixel_buffer& pixels) {
        auto binary = reader_.read_binary_to_end();

        switch (header_.type) {
          case ppm_type::bits:
            transfer_binary_bitmap(binary, pixels);
            break;
          default:
            transfer_binary_data(binary, pixels);
            adjust_range(pixels);
            break;
        }
      }



      void make_endian_native(pixel_buffer& pixels) {
        convert_endian(pixels, std::endian::native);
        header_.endianess = std::endian::native;
      }



      void adjust_range(pixel_buffer& pixels) {
        switch (pixels.format().format) {
          case data_format::u8:
            if (auto range = get<u16>(header_.range); range != 0xff) {
              ::adjust_range<u8, u32>(pixels, range);
            }
            break;
          case data_format::u16:
            if (auto range = get<u16>(header_.range); range != 0xffff) {
              make_endian_native(pixels);
              ::adjust_range<u16, u32>(pixels, range);
            }
            break;
          case data_format::f32:
            if (auto range = get<f32>(header_.range); std::abs(range - 1.f) > 1e-5) {
              make_endian_native(pixels);
              ::adjust_range<f32, f32>(pixels, range);
            }
            break;
          default:
            break;
        }
      }
  };
}



void decode_ppm(details::decoder& dec) {
  ppm_decoder pdec{dec};
  pdec.decode();
}
