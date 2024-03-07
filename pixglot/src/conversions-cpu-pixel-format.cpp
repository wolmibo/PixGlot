#include "pixglot/conversions.hpp"
#include "pixglot/exception.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/pixel-format-conversion.hpp"
#include "pixglot/utils/cast.hpp"

using namespace pixglot;



namespace pixglot::details {
  [[nodiscard]] std::endian swap_endian(std::endian);
  void swap_bytes(std::span<std::byte>, size_t);
}



namespace {
  [[nodiscard]] bool good_endian(data_format fmt, std::endian input, std::endian target) {
    if (byte_size(fmt) == 1) {
      return true;
    }

    return input == target;
  }



  [[nodiscard]] bool is_arithmetic_conversion(data_format df1, data_format df2) {
    return df1 != df2;
    /* FIXME: conversions between u8, u16, and u32
     * can also be done efficiently in non-native endian

    if (df1 == df2) {
      return false;
    }

    if (is_float(df1) || is_float(df2)) {
      return true;
    }

    return false;*/
  }





  template<data_format_type T>
  void gray_to_gray_a(std::span<const std::byte> src, std::span<std::byte> tgt, T fill) {
    auto tgt_spn = utils::interpret_as_greedy<T>(tgt);
    auto src_spn = utils::interpret_as_greedy<const T>(src);

    for (size_t i = 0, j = 0; i < src_spn.size(); ) {
      tgt_spn[j++] = src_spn[i++];
      tgt_spn[j++] = fill;
    }
  }

  template<data_format_type T>
  void gray_to_rgb(std::span<const std::byte> src, std::span<std::byte> tgt) {
    auto tgt_spn = utils::interpret_as_greedy<T>(tgt);
    auto src_spn = utils::interpret_as_greedy<const T>(src);

    for (size_t i = 0, j = 0; i < src_spn.size(); ++i) {
      tgt_spn[j++] = src_spn[i];
      tgt_spn[j++] = src_spn[i];
      tgt_spn[j++] = src_spn[i];
    }
  }

  template<data_format_type T>
  void gray_to_rgba(std::span<const std::byte> src, std::span<std::byte> tgt, T fill) {
    auto tgt_spn = utils::interpret_as_greedy<T>(tgt);
    auto src_spn = utils::interpret_as_greedy<const T>(src);

    for (size_t i = 0, j = 0; i < src_spn.size(); ++i) {
      tgt_spn[j++] = src_spn[i];
      tgt_spn[j++] = src_spn[i];
      tgt_spn[j++] = src_spn[i];
      tgt_spn[j++] = fill;
    }
  }

  template<data_format_type T>
  void gray_a_to_rgba(std::span<const std::byte> src, std::span<std::byte> tgt) {
    auto tgt_spn = utils::interpret_as_greedy<T>(tgt);
    auto src_spn = utils::interpret_as_greedy<const T>(src);

    for (size_t i = 0, j = 0; i < src_spn.size(); ) {
      tgt_spn[j++] = src_spn[i];
      tgt_spn[j++] = src_spn[i];
      tgt_spn[j++] = src_spn[i++];
      tgt_spn[j++] = src_spn[i++];
    }
  }

  template<data_format_type T>
  void rgb_to_rgba(std::span<const std::byte> src, std::span<std::byte> tgt, T fill) {
    auto tgt_spn = utils::interpret_as_greedy<T>(tgt);
    auto src_spn = utils::interpret_as_greedy<const T>(src);

    for (size_t i = 0, j = 0; i < src_spn.size(); ) {
      tgt_spn[j++] = src_spn[i++];
      tgt_spn[j++] = src_spn[i++];
      tgt_spn[j++] = src_spn[i++];
      tgt_spn[j++] = fill;
    }
  }



  template<data_format_type T>
  void convert_color_channels(
      std::span<const std::byte> source_bytes,
      color_channels             source_channels,
      std::span<std::byte>       target_bytes,
      color_channels             target_channels,
      T                          fill
  ) {
    switch (source_channels) {
      case color_channels::gray:
        switch (target_channels) {
          case color_channels::gray_a:
            gray_to_gray_a<T>(source_bytes, target_bytes, fill); break;
          case color_channels::rgb:
            gray_to_rgb<T>(source_bytes, target_bytes); break;
          case color_channels::rgba:
            gray_to_rgba<T>(source_bytes, target_bytes, fill); break;
          default:
            break;
        }
        break;
      case color_channels::gray_a:
        if (target_channels == color_channels::rgba) {
          gray_a_to_rgba<T>(source_bytes, target_bytes);
        }
        break;
      case color_channels::rgb:
        if (target_channels == color_channels::rgba) {
          rgb_to_rgba<T>(source_bytes, target_bytes, fill);
        }
        break;
      default:
        break;
    }
  }





  template<data_format_type Src, data_format_type Tgt>
    requires (sizeof(Src) == sizeof(Tgt) && std::is_integral_v<Tgt>)
  Tgt bit_convert_value(Src src, bool swapped) {
    auto out = std::bit_cast<Tgt>(src);
    if (swapped) {
      return std::byteswap(out);
    }
    return out;
  }





  void convert_color_channels(
      std::span<const std::byte> source_bytes,
      color_channels             source_channels,
      std::span<std::byte>       target_bytes,
      pixel_format               target_format,
      bool                       swapped
  ) {
    //NOLINTNEXTLINE(*macro*)
    #define CASE(x, y, v) case data_format::x:                 \
      convert_color_channels<y>(source_bytes, source_channels, \
          target_bytes, target_format.channels,                \
          bit_convert_value<x, y>(v, swapped)); break;

    switch (target_format.format) {
      CASE(u8,  u8,  0xff);
      CASE(u16, u16, 0xffff);
      CASE(u32, u32, 0xffffffff);
      CASE(f16, u16, static_cast<f16>(1.f));
      CASE(f32, u32, 1.f);
    }

    #undef CASE
  }





  template<data_format_type Src, data_format_type Tgt>
  void convert_data_format(
      std::span<const std::byte> source_bytes,
      std::span<std::byte>       target_bytes
  ) {
    auto src = utils::interpret_as_greedy<const Src>(source_bytes);
    auto tgt = utils::interpret_as_greedy<Tgt>(target_bytes);

    if (src.size() != tgt.size()) {
      throw std::bad_cast{};
    }

    for (size_t i = 0; i < src.size(); ++i) {
      tgt[i] = data_format_cast<Tgt>(src[i]);
    }
  }



  template<data_format_type Tgt>
  void convert_data_format(
      std::span<const std::byte> source_bytes,
      data_format                source_format,
      std::span<std::byte>       target_bytes
  ) {
    //NOLINTNEXTLINE(*macro*)
    #define CASE(x) case data_format::x: \
      convert_data_format<x, Tgt>(source_bytes, target_bytes); \
      return;

    switch (source_format) {
      CASE(u8);
      CASE(u16);
      CASE(u32);
      CASE(f16);
      CASE(f32)
    }

    throw base_exception{
      "Invalid data format",
      "source_format " + to_string(source_format) + " is not supported"
    };

    #undef CASE
  }



  void convert_data_format(
      std::span<const std::byte> source_bytes,
      data_format                source_format,
      std::span<std::byte>       target_bytes,
      data_format                target_format
  ) {
    //NOLINTNEXTLINE(*macro*)
    #define CASE(x) case data_format::x: \
      convert_data_format<x>(source_bytes, source_format, target_bytes); \
      return;

    switch (target_format) {
      CASE(u8);
      CASE(u16);
      CASE(u32);
      CASE(f16);
      CASE(f32);
    }

    throw base_exception{
      "Invalid data format",
      "target_format " + to_string(target_format) + " is not supported"
    };

    #undef CASE
  }





  void convert_pixel_format(
      pixel_buffer& input,
      bool          pre_swap,
      pixel_format  target_format,
      bool          post_swap
  ) {
    pixel_buffer output{input.width(), input.height(), target_format};

    for (size_t y = 0; y < input.height(); ++y) {
      auto source_bytes = input.row_bytes(y);
      auto target_bytes = output.row_bytes(y);

      if (pre_swap) {
        details::swap_bytes(source_bytes, byte_size(input.format().format));
      }

      size_t interim_size = output.width()
        * byte_size(target_format.format) * n_channels(input.format().channels);

      auto buffer_bytes = target_bytes.subspan(target_bytes.size() - interim_size);

      convert_data_format(source_bytes, input.format().format,
                          buffer_bytes, target_format.format);

      if (post_swap) {
        details::swap_bytes(buffer_bytes, byte_size(target_format.format));
      }

      convert_color_channels(buffer_bytes, input.format().channels,
          target_bytes, target_format, post_swap);
    }

    input = std::move(output);
  }
}





void pixglot::convert_pixel_format(
    pixel_buffer&              input,
    pixel_format               target_format,
    std::optional<std::endian> target_endian
) {
  if (input.format() == target_format) {
    if (target_endian && !good_endian(input.format().format, input.endian(), *target_endian)) {
      convert_endian(input, target_endian.value());
    }
    return;
  }

  if (!color_channels_contained(input.format().channels, target_format.channels)) {
    throw bad_pixel_format{target_format};
  }

  bool needs_pre_swap =
    !good_endian(input.format().format, input.endian(), std::endian::native) &&
    is_arithmetic_conversion(input.format().format, target_format.format);

  std::endian track_endian = input.endian();
  if (needs_pre_swap) {
    track_endian = details::swap_endian(track_endian);
  }

  bool needs_post_swap = target_endian &&
    !good_endian(target_format.format, track_endian, *target_endian);

  if (needs_post_swap) {
    track_endian = details::swap_endian(track_endian);
  }

  ::convert_pixel_format(input, needs_pre_swap, target_format, needs_post_swap);

  input.endian(track_endian);
}
