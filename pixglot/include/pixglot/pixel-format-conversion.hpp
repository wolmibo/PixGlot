// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_PIXEL_FORMAT_CONVERSION_HPP_INCLUDED
#define PIXGLOT_PIXEL_FORMAT_CONVERSION_HPP_INCLUDED

#include "pixglot/pixel-format.hpp"

#include <algorithm>



namespace pixglot::conversions {

template<data_format_type T>
struct info {};



template<>
struct info<u8> {
  static constexpr u8  range_max{0xff};
};



template<>
struct info<u16> {
  static constexpr u16 range_max{0xffff};
};



template<>
struct info<u32> {
  static constexpr u32 range_max{0xffffffff};
};



template<>
struct info<f16> {
  static constexpr f16 range_min{static_cast<f16>(0.f)};
  static constexpr f16 range_max{static_cast<f16>(1.f)};

  static constexpr f16 clamp(f16 value) {
    return std::clamp<f16>(value, range_min, range_max);
  }
};



template<>
struct info<f32> {
  static constexpr f32 range_min{0.f};
  static constexpr f32 range_max{1.f};

  static constexpr f32 clamp(f32 value) {
    return std::clamp<f32>(value, range_min, range_max);
  }
};





template<data_format_type SRC, data_format_type TGT>
struct data_format_converter {
  static constexpr bool bad{true};
  static constexpr TGT convert(SRC /*unused*/) { return TGT{}; }
};



template<data_format_type SRC>
struct data_format_converter<SRC, SRC> {
  static constexpr bool bad{false};
  static constexpr SRC convert(SRC value) { return value; }
};



// conversions from u8

template<>
struct data_format_converter<u8, u16> {
  static constexpr bool bad{false};
  static constexpr u16 convert(u8 value) {
    return value << 8 | value;
  }
};



template<>
struct data_format_converter<u8, u32> {
  static constexpr bool bad{false};
  static constexpr u32 convert(u8 value) {
    return value << 24 | value << 16 | value << 8 | value;
  }
};



template<>
struct data_format_converter<u8, f16> {
  static constexpr bool bad{false};
  static constexpr f16 convert(u8 value) {
    return static_cast<f16>(value) / static_cast<f16>(0xff);
  }
};



template<>
struct data_format_converter<u8, f32> {
  static constexpr bool bad{false};
  static constexpr f32 convert(u8 value) {
    return static_cast<f32>(value) / static_cast<f32>(0xff);
  }
};



// conversions from u16

template<>
struct data_format_converter<u16, u8> {
  static constexpr bool bad{false};
  static constexpr u8 convert(u16 value) {
    return (value >> 8) & 0xff;
  }
};



template<>
struct data_format_converter<u16, u32> {
  static constexpr bool bad{false};
  static constexpr u32 convert(u16 value) {
    return value << 16 | value;
  }
};



template<>
struct data_format_converter<u16, f16> {
  static constexpr bool bad{false};
  static constexpr f16 convert(u16 value) {
    return static_cast<f16>(value) / static_cast<f16>(0xffff);
  }
};



template<>
struct data_format_converter<u16, f32> {
  static constexpr bool bad{false};
  static constexpr f32 convert(u16 value) {
    return static_cast<f32>(value) / static_cast<f32>(0xffff);
  }
};



// conversions from u32

template<>
struct data_format_converter<u32, u8> {
  static constexpr bool bad{false};
  static constexpr u8 convert(u32 value) {
    return (value >> 24) & 0xff;
  }
};



template<>
struct data_format_converter<u32, u16> {
  static constexpr bool bad{false};
  static constexpr u32 convert(u32 value) {
    return (value >> 16) & 0xffff;
  }
};



template<>
struct data_format_converter<u32, f16> {
  static constexpr bool bad{false};
  static constexpr f16 convert(u32 value) {
    return static_cast<f16>(value) / static_cast<f16>(0xffffffff);
  }
};



template<>
struct data_format_converter<u32, f32> {
  static constexpr bool bad{false};
  static constexpr f32 convert(u32 value) {
    return static_cast<f32>(value) / static_cast<f32>(0xffffffff);
  }
};



// conversions from f16

template<>
struct data_format_converter<f16, u8> {
  static constexpr bool bad{false};
  static constexpr u8 convert(f16 value) {
    return info<f16>::clamp(value) * static_cast<f16>(0xff);
  }
};



template<>
struct data_format_converter<f16, u16> {
  static constexpr bool bad{false};
  static constexpr u16 convert(f16 value) {
    return info<f16>::clamp(value) * static_cast<f16>(0xffff);
  }
};



template<>
struct data_format_converter<f16, u32> {
  static constexpr bool bad{false};
  static constexpr u32 convert(f16 value) {
    return info<f16>::clamp(value) * static_cast<f16>(0xffffffff);
  }
};



template<>
struct data_format_converter<f16, f32> {
  static constexpr bool bad{false};
  static constexpr f32 convert(f16 value) {
    return static_cast<f32>(value);
  }
};



// conversions from f32

template<>
struct data_format_converter<f32, u8> {
  static constexpr bool bad{false};
  static constexpr u8 convert(f32 value) {
    return info<f32>::clamp(value) * static_cast<f32>(0xff);
  }
};



template<>
struct data_format_converter<f32, u16> {
  static constexpr bool bad{false};
  static constexpr u16 convert(f32 value) {
    return info<f32>::clamp(value) * static_cast<f32>(0xffff);
  }
};



template<>
struct data_format_converter<f32, u32> {
  static constexpr bool bad{false};
  static constexpr u32 convert(f32 value) {
    return info<f32>::clamp(value) * static_cast<f32>(0xffffffff);
  }
};



template<>
struct data_format_converter<f32, f16> {
  static constexpr bool bad{false};
  static constexpr f16 convert(f32 value) {
    return static_cast<f16>(value);
  }
};

}





namespace pixglot {

template<data_format_type TGT, data_format_type SRC>
constexpr TGT data_format_cast(SRC value) {
  static_assert(!conversions::data_format_converter<SRC, TGT>::bad,
    "no explicit cast defined from SRC to TGT");

  return conversions::data_format_converter<SRC, TGT>::convert(value);
}

}





namespace pixglot::conversions {

template<pixel_type SRC, pixel_type TGT>
struct pixel_converter {
  static constexpr bool bad{true};
  static constexpr TGT convert(SRC /*unused*/) {
    return {};
  }
};



// conversions from gray

template<data_format_type S, data_format_type T>
struct pixel_converter<gray<S>, gray<T>> {
  static constexpr bool bad{false};
  static constexpr gray<T> convert(gray<S> value) {
    return {
      .v = data_format_cast<T>(value.v)
    };
  }
};



template<data_format_type S, data_format_type T>
struct pixel_converter<gray<S>, gray_a<T>> {
  static constexpr bool bad{false};
  static constexpr gray_a<T> convert(gray<S> value) {
    return {
      .v = data_format_cast<T>(value.v),
      .a = info<T>::range_max
    };
  }
};



template<data_format_type S, data_format_type T>
struct pixel_converter<gray<S>, rgb<T>> {
  static constexpr bool bad{false};
  static constexpr rgb<T> convert(gray<S> value) {
    return {
      .r = data_format_cast<T>(value.v),
      .g = data_format_cast<T>(value.v),
      .b = data_format_cast<T>(value.v)
    };
  }
};



template<data_format_type S, data_format_type T>
struct pixel_converter<gray<S>, rgba<T>> {
  static constexpr bool bad{false};
  static constexpr rgba<T> convert(gray<S> value) {
    return {
      .r = data_format_cast<T>(value.v),
      .g = data_format_cast<T>(value.v),
      .b = data_format_cast<T>(value.v),
      .a = info<T>::range_max
    };
  }
};



// conversions from gray_a

template<data_format_type S, data_format_type T>
struct pixel_converter<gray_a<S>, gray_a<T>> {
  static constexpr bool bad{false};
  static constexpr gray_a<T> convert(gray_a<S> value) {
    return {
      .v = data_format_cast<T>(value.v),
      .a = data_format_cast<T>(value.a)
    };
  }
};



template<data_format_type S, data_format_type T>
struct pixel_converter<gray_a<S>, rgba<T>> {
  static constexpr bool bad{false};
  static constexpr rgba<T> convert(gray_a<S> value) {
    return {
      .r = data_format_cast<T>(value.v),
      .g = data_format_cast<T>(value.v),
      .b = data_format_cast<T>(value.v),
      .a = data_format_cast<T>(value.a)
    };
  }
};



// conversions from rgb

template<data_format_type S, data_format_type T>
struct pixel_converter<rgb<S>, rgb<T>> {
  static constexpr bool bad{false};
  static constexpr rgb<T> convert(rgb<S> value) {
    return {
      .r = data_format_cast<T>(value.r),
      .g = data_format_cast<T>(value.g),
      .b = data_format_cast<T>(value.b),
    };
  }
};



template<data_format_type S, data_format_type T>
struct pixel_converter<rgb<S>, rgba<T>> {
  static constexpr bool bad{false};
  static constexpr rgba<T> convert(rgb<S> value) {
    return {
      .r = data_format_cast<T>(value.r),
      .g = data_format_cast<T>(value.g),
      .b = data_format_cast<T>(value.b),
      .a = info<T>::range_max
    };
  }
};



// conversions from rgba

template<data_format_type S, data_format_type T>
struct pixel_converter<rgba<S>, rgba<T>> {
  static constexpr bool bad{false};
  static constexpr rgba<T> convert(rgba<S> value) {
    return {
      .r = data_format_cast<T>(value.r),
      .g = data_format_cast<T>(value.g),
      .b = data_format_cast<T>(value.b),
      .a = data_format_cast<T>(value.a)
    };
  }
};

}




namespace pixglot {

template<pixel_type TGT, pixel_type SRC>
constexpr TGT pixel_cast(SRC pixel) {
  static_assert(!conversions::pixel_converter<SRC, TGT>::bad,
    "no explicit non-narrowing conversion from SRC to TGT");

  return conversions::pixel_converter<SRC, TGT>::convert(pixel);
}

}



#endif // PIXGLOT_PIXEL_FORMAT_CONVERSION_HPP_INCLUDED
