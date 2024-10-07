// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_DETAILS_STRING_BYTES_HPP_INCLUDED
#define PIXGLOT_DETAILS_STRING_BYTES_HPP_INCLUDED

#include "pixglot/utils/int_cast.hpp"

#include <limits>
#include <string>
#include <string_view>

namespace pixglot::details {



template<typename Byte, std::integral Int> requires (sizeof(Byte) == 1)
[[nodiscard]] std::string_view string_view_from(const Byte* bytes, Int size) {
  if (size <= 0 || bytes == nullptr) {
    return {};
  }

  auto S = utils::int_cast<size_t>(size);

  size_t s = 0;
  //NOLINTNEXTLINE(*-pointer-arithmetic)
  for (const Byte* b{bytes}; s < S && *b != Byte{0}; ++b, ++s) {}

  //NOLINTNEXTLINE(*-reinterpret-cast)
  return std::string_view{reinterpret_cast<const char*>(bytes), s};
}



template<typename Byte, std::integral Int> requires (sizeof(Byte) == 1)
[[nodiscard]] std::string string_from(const Byte* bytes, Int size) {
  return std::string{string_view_from<Byte, Int>(bytes, size)};
}



template<typename Byte> requires (sizeof(Byte) == 1)
[[nodiscard]] std::string string_from(const Byte* bytes) {
  return string_from(bytes, std::numeric_limits<size_t>::max());
}


}

#endif // PIXGLOT_DETAILS_STRING_BYTES_HPP_INCLUDED
