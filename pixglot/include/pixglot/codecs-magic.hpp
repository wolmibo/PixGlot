// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_CODECS_MAGIC_HPP_INCLUDED
#define PIXGLOT_CODECS_MAGIC_HPP_INCLUDED

#include "pixglot/codecs.hpp"

#include <algorithm>
#include <array>
#include <span>



namespace pixglot {

[[nodiscard]] constexpr bool check_magic(
    std::span<const std::byte> magic,
    std::span<const std::byte> proto
) {
  return magic.size() >= proto.size()
    && std::ranges::equal(magic.subspan(0, proto.size()), proto);
}



[[nodiscard]] constexpr bool check_magic(
    std::span<const std::byte> magic,
    std::span<const std::byte> proto,
    size_t                     offset
) {
  return magic.size() >= proto.size() + offset
    && std::ranges::equal(magic.subspan(offset, proto.size()), proto);
}





template<codec C>
struct magic {
  static constexpr size_t recommended_size {0};

  static bool check(std::span<const std::byte> /*unused*/) {
    return false;
  }
};



#ifdef PIXGLOT_WITH_JPEG
template<>
struct magic<codec::jpeg> {
  static constexpr std::array<std::byte, 3> bytes {
    std::byte{0xff}, std::byte{0xd8}, std::byte{0xff}
  };

  static constexpr size_t recommended_size{bytes.size()};

  static bool check(std::span<const std::byte> input) {
    return check_magic(input, bytes);
  }
};
#endif



#ifdef PIXGLOT_WITH_PNG
template<>
struct magic<codec::png> {
  static constexpr std::array<std::byte, 8> bytes = {
    std::byte{0x89}, std::byte{0x50}, std::byte{0x4E}, std::byte{0x47},
    std::byte{0x0d}, std::byte{0x0a}, std::byte{0x1a}, std::byte{0x0a}
  };

  static constexpr size_t recommended_size{bytes.size()};

  static bool check(std::span<const std::byte> input) {
    return check_magic(input, bytes);
  }
};
#endif



#ifdef PIXGLOT_WITH_AVIF
template<>
struct magic<codec::avif> {
  static constexpr std::array<std::byte, 8> bytes = {
    std::byte{0x66}, std::byte{0x74}, std::byte{0x79}, std::byte{0x70},
    std::byte{0x61}, std::byte{0x76}, std::byte{0x69}, std::byte{0x66}
  };

  static constexpr size_t recommended_size{bytes.size() + 4};

  static bool check(std::span<const std::byte> input) {
    return check_magic(input, bytes, 4);
  }
};
#endif



#ifdef PIXGLOT_WITH_EXR
template<>
struct magic<codec::exr> {
  static constexpr std::array<std::byte, 4> bytes = {
    std::byte{0x76}, std::byte{0x2f}, std::byte{0x31}, std::byte{0x01}
  };

  static constexpr size_t recommended_size{bytes.size()};

  static bool check(std::span<const std::byte> input) {
    return check_magic(input, bytes);
  }
};
#endif



#ifdef PIXGLOT_WITH_PPM
template<>
struct magic<codec::ppm> {
  static constexpr size_t recommended_size{2};

  static bool check(std::span<const std::byte> input) {
    if (input.size() > 2) {
      char head = static_cast<char>(input[0]);
      char var  = static_cast<char>(input[1]);
      return head == 'P' && (
        ('1' <= var && var <= '6') ||
        var == 'f' || var == 'F');
    }
    return false;
  }
};
#endif



#ifdef PIXGLOT_WITH_WEBP
template<>
struct magic<codec::webp> {
  static constexpr size_t recommended_size{12};

  static constexpr std::array<std::byte, 4> bytes_riff = {
    std::byte{0x52}, std::byte{0x49}, std::byte{0x46}, std::byte{0x46}
  };
  static constexpr std::array<std::byte, 4> bytes_webp = {
    std::byte{0x57}, std::byte{0x45}, std::byte{0x42}, std::byte{0x50}
  };


  static bool check(std::span<const std::byte> input) {
    return check_magic(input, bytes_riff) &&
      check_magic(input, bytes_webp, 8);
  }
};
#endif



#ifdef PIXGLOT_WITH_GIF
template<>
struct magic<codec::gif> {
  static constexpr size_t recommended_size{6};

  static constexpr std::array<std::byte, 6> bytes_87 = {
    std::byte{0x47}, std::byte{0x49}, std::byte{0x46},
    std::byte{0x38}, std::byte{0x37}, std::byte{0x61},
  };

  static constexpr std::array<std::byte, 6> bytes_89 = {
    std::byte{0x47}, std::byte{0x49}, std::byte{0x46},
    std::byte{0x38}, std::byte{0x39}, std::byte{0x61},
  };

  static bool check(std::span<const std::byte> input) {
    return check_magic(input, bytes_87) || check_magic(input, bytes_89);
  }
};
#endif



#ifdef PIXGLOT_WITH_JXL
template<>
struct magic<codec::jxl> {
  static constexpr size_t recommended_size{12};

  static bool check(std::span<const std::byte>);
};
#endif





static constexpr size_t recommended_magic_size = std::max<size_t>({
#ifdef PIXGLOT_WITH_JPEG
  magic<codec::jpeg>::recommended_size,
#endif
#ifdef PIXGLOT_WITH_PNG
  magic<codec::png>::recommended_size,
#endif
#ifdef PIXGLOT_WITH_AVIF
  magic<codec::avif>::recommended_size,
#endif
#ifdef PIXGLOT_WITH_EXR
  magic<codec::exr>::recommended_size,
#endif
#ifdef PIXGLOT_WITH_PPM
  magic<codec::ppm>::recommended_size,
#endif
#ifdef PIXGLOT_WITH_WEBP
  magic<codec::webp>::recommended_size,
#endif
#ifdef PIXGLOT_WITH_GIF
  magic<codec::gif>::recommended_size,
#endif
#ifdef PIXGLOT_WITH_JXL
  magic<codec::jxl>::recommended_size,
#endif
  size_t{0}
});


}

#endif // PIXGLOT_CODECS_MAGIC_HPP_INCLUDED
