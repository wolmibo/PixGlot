// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_SQUARE_ISOMETRY_HPP_INCLUDED
#define PIXGLOT_SQUARE_ISOMETRY_HPP_INCLUDED

#include <string>
#include <string_view>
#include <utility>



namespace pixglot {

enum class square_isometry {
  identity       = 0,

  flip_x         = 1,
  rotate_cw      = 2,

  rotate_half    = 2 * rotate_cw,
  rotate_ccw     = 3 * rotate_cw,

  flip_y         = rotate_half | flip_x,

  transpose      = rotate_cw   | flip_x,
  anti_transpose = rotate_ccw  | flip_x,
};



[[nodiscard]] std::string_view stringify(square_isometry);

[[nodiscard]] inline std::string to_string(square_isometry si) {
  return std::string{stringify(si)};
}



[[nodiscard]] constexpr bool orientation_reversing(square_isometry val) {
  return (std::to_underlying(val) & std::to_underlying(square_isometry::flip_x)) != 0;
}

[[nodiscard]] constexpr bool flips_xy(square_isometry val) {
  return (std::to_underlying(val) & std::to_underlying(square_isometry::rotate_cw)) != 0;
}

[[nodiscard]] constexpr unsigned int quarter_rotations(square_isometry val) {
  return (std::to_underlying(val) & 0x6) >> 1;
}



[[nodiscard]] constexpr std::pair<unsigned int, bool> split_rotations_reverse(
    square_isometry val
) {
  return {quarter_rotations(val), orientation_reversing(val)};
}



[[nodiscard]] constexpr square_isometry create_square_isometry(
    unsigned int rotations_by_90,
    bool         flip
) {
  return static_cast<square_isometry>((rotations_by_90 & 0x3) << 0b11
            | static_cast<unsigned int>(flip));
}





[[nodiscard]] constexpr square_isometry inverse(square_isometry val) {
  if (orientation_reversing(val)) {
    return val;
  }
  return static_cast<square_isometry>((8 - (static_cast<int>(val) & 0x6)) & 0x6);
}



[[nodiscard]] constexpr square_isometry operator*(
    square_isometry lhs,
    square_isometry rhs
) {
  auto left  = std::to_underlying(lhs);
  auto right = std::to_underlying(rhs);

  switch (static_cast<int>(orientation_reversing(lhs)) << 1
           | static_cast<int>(orientation_reversing(rhs))) {
    case 0b00:
      return static_cast<square_isometry>((left + right) & 0x6);
    case 0b01:
      return static_cast<square_isometry>(((8 + (right&0x6) - (left&0x6)) & 0x6) | 1);
    case 0b10:
      return static_cast<square_isometry>((((right&0x6) + (left&0x6)) & 0x6) | 1);
    case 0b11:
      return static_cast<square_isometry>((8 + (right&0x6) - (left&0x6)) & 0x6);
    default:
      return square_isometry::identity;
  }
}



[[nodiscard]] constexpr square_isometry operator/(
    square_isometry lhs,
    square_isometry rhs
) {
  return lhs * inverse(rhs);
}



constexpr square_isometry& operator*=(square_isometry& lhs, square_isometry rhs) {
  lhs = (lhs * rhs);
  return lhs;
}



constexpr square_isometry& operator/=(square_isometry& lhs, square_isometry rhs) {
  lhs = (lhs / rhs);
  return lhs;
}

}

#endif // PIXGLOT_SQUARE_ISOMETRY_HPP_INCLUDED
