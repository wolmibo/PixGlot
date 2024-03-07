// Copyright (c) 2024 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_DETAILS_INT_CAST_HPP_INCLUDED
#define PIXGLOT_DETAILS_INT_CAST_HPP_INCLUDED

#include <concepts>
#include <limits>
#include <stdexcept>



template<std::signed_integral Tgt, std::unsigned_integral Src>
constexpr Tgt int_cast_unsigned_to_signed(Src src) {
  static_assert(std::numeric_limits<Tgt>::max() >= 0);
  constexpr auto tgt_max
    = static_cast<std::make_unsigned_t<Tgt>>(std::numeric_limits<Tgt>::max());

  if constexpr (std::numeric_limits<Src>::max() <= tgt_max) {
    return static_cast<Tgt>(src);

  } else {
    if (src <= tgt_max) {
      return static_cast<Tgt>(src);
    }
  }

  throw std::out_of_range("int_cast_unsigned_to_signed");
}



template<std::unsigned_integral Tgt, std::signed_integral Src>
constexpr Tgt int_cast_signed_to_unsigned(Src src) {
  static_assert(std::numeric_limits<Src>::max() >= 0);
  constexpr auto src_max
    = static_cast<std::make_unsigned_t<Src>>(std::numeric_limits<Src>::max());

  if (src >= 0) {
    if constexpr (src_max <= std::numeric_limits<Tgt>::max()) {
      return static_cast<Tgt>(src);
    } else {
      if (static_cast<std::make_unsigned_t<Src>>(src) <=
          std::numeric_limits<Tgt>::max()) {
        return static_cast<Tgt>(src);
      }
    }
  }

  throw std::out_of_range("int_cast_signed_to_unsigned");
}



template<std::integral Tgt, std::integral Src>
  requires (std::is_signed_v<Tgt> == std::is_signed_v<Src>)
constexpr Tgt int_cast_same_signedness(Src src) {
  if constexpr (std::numeric_limits<Src>::min() >= std::numeric_limits<Tgt>::min() &&
                std::numeric_limits<Src>::max() <= std::numeric_limits<Tgt>::max()) {
    return static_cast<Tgt>(src);

  } else if constexpr (!std::is_signed_v<Src>) {
    if (src <= std::numeric_limits<Tgt>::max()) {
      return static_cast<Tgt>(src);
    }

  } else {
    if (src >= std::numeric_limits<Tgt>::min() &&
        src <= std::numeric_limits<Tgt>::max()) {
      return static_cast<Tgt>(src);
    }
  }

  throw std::out_of_range("int_cast_same_signedness");
}





template<std::integral Tgt, std::integral Src>
constexpr Tgt int_cast(Src src) {
  if constexpr (std::is_same_v<Src, Tgt>) {
    return src;

  } else if constexpr (std::is_signed_v<Src> == std::is_signed_v<Tgt>) {
    return int_cast_same_signedness<Tgt, Src>(src);

  } else if constexpr (std::is_signed_v<Tgt>) {
    return int_cast_unsigned_to_signed<Tgt, Src>(src);

  } else {
    return int_cast_signed_to_unsigned<Tgt, Src>(src);
  }
}





template<std::integral Tgt, std::floating_point Src>
constexpr Tgt int_cast(Src src) {
  constexpr auto max = static_cast<Src>(std::numeric_limits<Tgt>::max());
  constexpr auto min = static_cast<Src>(std::numeric_limits<Tgt>::min());

  if (min <= src && src <= max) {
    return static_cast<Tgt>(src);
  }

  throw std::out_of_range("int_cast from floating point");
}

#endif // PIXGLOT_DETAILS_INT_CAST_HPP_INCLUDED
