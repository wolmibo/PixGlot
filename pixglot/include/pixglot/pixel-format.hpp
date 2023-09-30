// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_PIXEL_FORMAT_HPP_INCLUDED
#define PIXGLOT_PIXEL_FORMAT_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>



namespace pixglot {

enum class data_format {
  u8  = 0,
  u16 = 1,
  u32 = 2,

  f16 = 3,
  f32 = 4
};

[[nodiscard]] std::string_view stringify(data_format);

[[nodiscard]] inline std::string to_string(data_format f) {
  return std::string{stringify(f)};
}


[[nodiscard]] constexpr bool is_float(data_format dt) {
  return std::to_underlying(dt) >= std::to_underlying(data_format::f16);
}

[[nodiscard]] constexpr bool is_signed(data_format dt) {
  return is_float(dt);
}



[[nodiscard]] constexpr size_t byte_size(data_format dt) {
  switch (dt) {
    case data_format::u8:  return 1;
    case data_format::u16: return 2;
    case data_format::u32: return 4;

    case data_format::f16: return 2;
    case data_format::f32: return 4;
  }
  return 0;
}



using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

#if defined(__GNUG__) || defined(__clang__)
using f16 = _Float16;
#else
#error no half float data type found
#endif

using f32 = float;



template<typename T>
concept data_format_type_candidate =
  std::is_trivially_copyable_v<T> &&
  std::semiregular<T> &&

  (std::is_same_v<T,  u8> ||
   std::is_same_v<T, u16> ||
   std::is_same_v<T, u32> ||
   std::is_same_v<T, f16> ||
   std::is_same_v<T, f32>);



template<data_format dt> struct data_format_to { using type = void; };

template<> struct data_format_to<data_format::u8>  { using type = u8;  };
template<> struct data_format_to<data_format::u16> { using type = u16; };
template<> struct data_format_to<data_format::u32> { using type = u32; };
template<> struct data_format_to<data_format::f16> { using type = f16; };
template<> struct data_format_to<data_format::f32> { using type = f32; };



template<data_format_type_candidate T>
struct data_format_from { static constexpr data_format value {data_format::u8}; };

template<>
struct data_format_from<u16> { static constexpr data_format value {data_format::u16}; };
template<>
struct data_format_from<u32> { static constexpr data_format value {data_format::u32}; };
template<>
struct data_format_from<f16> { static constexpr data_format value {data_format::f16}; };
template<>
struct data_format_from<f32> { static constexpr data_format value {data_format::f32}; };



template<typename T>
concept data_format_type = requires {
  requires data_format_type_candidate<T>;

  byte_size(data_format_from<T>::value) == sizeof(T);
  std::is_same_v<T, typename data_format_to<data_format_from<T>::value>::type >;
};





enum class color_channels : unsigned int {
  gray   = 1,
  gray_a = 2,
  rgb    = 3,
  rgba   = 4,
};

[[nodiscard]] std::string_view stringify(color_channels);

[[nodiscard]] inline std::string to_string(color_channels c) {
  return std::string{stringify(c)};
}



[[nodiscard]] constexpr unsigned int n_channels(color_channels cc) {
  return std::to_underlying(cc);
}

[[nodiscard]] constexpr bool has_alpha(color_channels cc) {
  return n_channels(cc) % 2 == 0;
}

[[nodiscard]] constexpr bool has_color(color_channels cc) {
  return n_channels(cc) >= 3;
}



[[nodiscard]] color_channels add_alpha(color_channels);
[[nodiscard]] color_channels add_color(color_channels);



[[nodiscard]] bool color_channels_contained(color_channels, color_channels);





struct pixel_format {
  data_format    format  {data_format::u8};
  color_channels channels{color_channels::rgba};

  [[nodiscard]] auto operator<=>(const pixel_format&) const = default;



  [[nodiscard]] constexpr size_t size() const {
    return byte_size(format) * n_channels(channels);
  }
};

[[nodiscard]] std::string to_string(pixel_format);





template<data_format_type T>
struct gray {
  T v;

  using component = T;

  [[nodiscard]] auto operator<=>(const gray<T>&) const = default;

  [[nodiscard]] static constexpr pixel_format format() {
    return pixel_format {
      .format   = data_format_from<T>::value,
      .channels = color_channels::gray,
    };
  };
};



template<data_format_type T>
struct gray_a {
  T v;
  T a;

  using component = T;

  [[nodiscard]] auto operator<=>(const gray_a<T>&) const = default;

  [[nodiscard]] static constexpr pixel_format format() {
    return pixel_format {
      .format   = data_format_from<T>::value,
      .channels = color_channels::gray_a,
    };
  };
};



template<data_format_type T>
struct rgb {
  T r;
  T g;
  T b;

  using component = T;

  [[nodiscard]] auto operator<=>(const rgb<T>&) const = default;

  [[nodiscard]] static constexpr pixel_format format() {
    return pixel_format {
      .format   = data_format_from<T>::value,
      .channels = color_channels::rgb,
    };
  };
};



template<data_format_type T>
struct rgba {
  T r;
  T g;
  T b;
  T a;

  using component = T;

  [[nodiscard]] auto operator<=>(const rgba<T>&) const = default;

  [[nodiscard]] static constexpr pixel_format format() {
    return pixel_format {
      .format   = data_format_from<T>::value,
      .channels = color_channels::rgba,
    };
  };
};





template<data_format D, color_channels C>
struct data_format_color_channels_to {
  using type = gray<typename data_format_to<D>::type>;
};

template<data_format D>
struct data_format_color_channels_to<D, color_channels::gray_a> {
  using type = gray_a<typename data_format_to<D>::type>;
};

template<data_format D>
struct data_format_color_channels_to<D, color_channels::rgb> {
  using type = rgb<typename data_format_to<D>::type>;
};

template<data_format D>
struct data_format_color_channels_to<D, color_channels::rgba> {
  using type = rgba<typename data_format_to<D>::type>;
};

template<pixel_format pf>
struct pixel_format_to {
  using type = typename data_format_color_channels_to<pf.format, pf.channels>::type;
};





template<typename T>
concept pixel_type =
  std::is_trivially_copyable_v<T> &&
  std::semiregular<T> &&

  data_format_type<typename T::component> &&

  (std::is_same_v<T, gray  <typename T::component>> ||
   std::is_same_v<T, gray_a<typename T::component>> ||
   std::is_same_v<T, rgb   <typename T::component>> ||
   std::is_same_v<T, rgba  <typename T::component>>) &&

  std::is_same_v<T, typename pixel_format_to<T::format()>::type > &&

  sizeof(T)  == sizeof(typename T::component) * n_channels(T::format().channels) &&
  alignof(T) == alignof(typename T::component);

}

#endif // PIXGLOT_PIXEL_FORMAT_HPP_INCLUDED
