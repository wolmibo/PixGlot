// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_PIXEL_FORMAT_CONVERSION_HPP_INCLUDED
#define PIXGLOT_PIXEL_FORMAT_CONVERSION_HPP_INCLUDED

#include "pixglot/pixel-format.hpp"

#include <algorithm>
#include <limits>



namespace pixglot::details {
  template<std::integral Src, std::integral Tgt>
    requires (sizeof(Src) >= sizeof(Tgt))
  [[nodiscard]] constexpr Tgt upper_bits(Src value) {
    return (value >> (8 * (sizeof(Src) - sizeof(Tgt)))) & std::numeric_limits<Tgt>::max();
  }



  template<data_format_type T>
  [[nodiscard]] constexpr T full_channel() {
    if constexpr (std::integral<T>) {
      return std::numeric_limits<T>::max();
    } else {
      return static_cast<T>(1.f);
    }
  }
}





namespace pixglot {
  template<data_format_type Tgt, data_format_type Src>
  [[nodiscard]] constexpr Tgt data_format_cast(Src value) {
    static constexpr bool src_is_float = is_float(data_format_from<Src>::value);
    static constexpr bool tgt_is_float = is_float(data_format_from<Tgt>::value);

    if constexpr (std::is_same_v<Src, Tgt>) {
      return value;
    } else if constexpr (src_is_float && tgt_is_float) {
      return static_cast<Tgt>(value);
    } else if constexpr (src_is_float) {
      return static_cast<Tgt>(std::clamp<f32>(static_cast<f32>(value), 0.f, 1.f));
    } else if constexpr (tgt_is_float) {
      return static_cast<Tgt>(static_cast<f32>(value)
          / static_cast<f32>(std::numeric_limits<Src>::max()));

    } else if constexpr (sizeof(Src) > sizeof(Tgt)) {
      return details::upper_bits<Src, Tgt>(value);

    } else if constexpr (std::is_same_v<Src, u8> && std::is_same_v<Tgt, u16>) {
      return value << 8 | value;
    } else if constexpr (std::is_same_v<Src, u8> && std::is_same_v<Tgt, u32>) {
      return value << 24 | value << 16 | value << 8 | value;
    } else if constexpr (std::is_same_v<Src, u16> && std::is_same_v<Tgt, u32>) {
      return value << 16 | value;

    } else {
      static_assert(!data_format_type<Src>, "no data format conversion defined");
    }
  }





  template<data_format_type TgtDf, pixel_type Src>
  [[nodiscard]] constexpr auto channel_data_format_cast(Src value) {
    using SrcDf = typename Src::component;

    static constexpr color_channels channels = Src::format().channels;

    if constexpr (std::is_same_v<TgtDf, SrcDf>) {
      return value;
    } else if constexpr (channels == color_channels::gray) {
      return gray<TgtDf> {
        .v = data_format_cast<TgtDf>(value.v)
      };
    } else if constexpr (channels == color_channels::gray_a) {
      return gray_a<TgtDf> {
        .v = data_format_cast<TgtDf>(value.v)
        .a = data_format_cast<TgtDf>(value.a)
      };
    } else if constexpr (channels == color_channels::rgb) {
      return rgb<TgtDf> {
        .r = data_format_cast<TgtDf>(value.r)
        .g = data_format_cast<TgtDf>(value.g)
        .b = data_format_cast<TgtDf>(value.b)
      };
    } else if constexpr (channels == color_channels::rgba) {
      return rgb<TgtDf> {
        .r = data_format_cast<TgtDf>(value.r)
        .g = data_format_cast<TgtDf>(value.g)
        .b = data_format_cast<TgtDf>(value.b)
        .a = data_format_cast<TgtDf>(value.a)
      };
    } else {
      static_assert(!data_format_type<Src>, "no channel data format conversion defined");
    }
  }





  template<pixel_type Tgt, pixel_type Src>
  [[nodiscard]] constexpr Tgt pixel_cast(Src pixel) {
    static constexpr color_channels src_c = Src::format().channels;
    static constexpr color_channels tgt_c = Tgt::format().channels;

    using Df = typename Tgt::component;

    if constexpr (src_c == color_channels::gray) {
      if constexpr (tgt_c == color_channels::gray) {
        return {
          .v = data_format_cast<Df>(pixel.v)
        };
      } else if constexpr (tgt_c == color_channels::gray_a) {
        return {
          .v = data_format_cast<Df>(pixel.v),
          .a = details::full_channel<Df>()
        };
      } else if constexpr (tgt_c == color_channels::rgb) {
        return {
          .r = data_format_cast<Df>(pixel.v),
          .g = data_format_cast<Df>(pixel.v),
          .b = data_format_cast<Df>(pixel.v)
        };
      } else if constexpr (tgt_c == color_channels::rgba) {
        return {
          .r = data_format_cast<Df>(pixel.v),
          .g = data_format_cast<Df>(pixel.v),
          .b = data_format_cast<Df>(pixel.v),
          .a = details::full_channel<Df>()
        };
      } else {
        static_assert(!pixel_type<Src>, "no pixel conversion defined");
      }


    } else if constexpr (src_c == color_channels::gray_a) {
      if constexpr (tgt_c == color_channels::gray_a) {
        return {
          .v = data_format_cast<Df>(pixel.v),
          .a = data_format_cast<Df>(pixel.a),
        };
      } else if constexpr (tgt_c == color_channels::rgba) {
        return {
          .r = data_format_cast<Df>(pixel.v),
          .g = data_format_cast<Df>(pixel.v),
          .b = data_format_cast<Df>(pixel.v),
          .a = data_format_cast<Df>(pixel.a),
        };
      } else {
        static_assert(!pixel_type<Src>, "no pixel conversion defined");
      }


    } else if constexpr (src_c == color_channels::rgb) {
      if constexpr (tgt_c == color_channels::rgb) {
        return {
          .r = data_format_cast<Df>(pixel.r),
          .g = data_format_cast<Df>(pixel.g),
          .b = data_format_cast<Df>(pixel.b),
        };
      } else if constexpr (tgt_c == color_channels::rgba) {
        return {
          .r = data_format_cast<Df>(pixel.r),
          .g = data_format_cast<Df>(pixel.g),
          .b = data_format_cast<Df>(pixel.b),
          .a = details::full_channel<Df>()
        };
      } else {
        static_assert(!pixel_type<Src>, "no pixel conversion defined");
      }


    } else if constexpr (src_c == color_channels::rgba) {
      if constexpr (tgt_c == color_channels::rgba) {
        return {
          .r = data_format_cast<Df>(pixel.r),
          .g = data_format_cast<Df>(pixel.g),
          .b = data_format_cast<Df>(pixel.b),
          .a = data_format_cast<Df>(pixel.a),
        };
      } else {
        static_assert(!pixel_type<Src>, "no pixel conversion defined");
      }


    } else {
      static_assert(!pixel_type<Src>, "no pixel conversion defined");
    }
  }
}

#endif // PIXGLOT_PIXEL_FORMAT_CONVERSION_HPP_INCLUDED
