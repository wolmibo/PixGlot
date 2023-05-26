// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_CODECS_HPP_INCLUDED
#define PIXGLOT_CODECS_HPP_INCLUDED

#include "config.hpp"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>



namespace pixglot {

enum class codec {
  jpeg,
  png,
  avif,
  exr,
  ppm,
  webp,
  gif,
  jxl,
};

[[nodiscard]] std::string_view stringify(codec);

[[nodiscard]] inline std::string to_string(codec cc) {
  return std::string{stringify(cc)};
}



[[nodiscard]] std::vector<std::string_view> mime_types(codec);

inline std::vector<codec> list_all_codecs() {
  return {
    codec::jpeg,
    codec::png,
    codec::avif,
    codec::exr,
    codec::ppm,
    codec::webp,
    codec::gif,
    codec::jxl
  };
}



inline std::vector<codec> list_codecs() {
  return {
#ifdef PIXGLOT_WITH_JPEG
    codec::jpeg,
#endif
#ifdef PIXGLOT_WITH_PNG
    codec::png,
#endif
#ifdef PIXGLOT_WITH_AVIF
    codec::avif,
#endif
#ifdef PIXGLOT_WITH_EXR
    codec::exr,
#endif
#ifdef PIXGLOT_WITH_PPM
    codec::ppm,
#endif
#ifdef PIXGLOT_WITH_WEBP
    codec::webp,
#endif
#ifdef PIXGLOT_WITH_GIF
    codec::gif,
#endif
#ifdef PIXGLOT_WITH_JXL
    codec::jxl,
#endif
  };
}



std::optional<codec> determine_codec(std::span<const std::byte>);
std::optional<codec> determine_codec(const std::filesystem::path&);

}

#endif // PIXGLOT_CODECS_HPP_INCLUDED
