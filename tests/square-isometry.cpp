#include "common.hpp"

#include <iostream>
#include <set>

#include <pixglot/square-isometry.hpp>

using namespace pixglot;



[[nodiscard]] square_isometry pow_int(square_isometry val, int count) {
  auto start = square_isometry::identity;
  for (int i = 0; i < count; ++i) {
    start *= val;
  }
  return start;
}



[[nodiscard]] square_isometry lut_inverse(square_isometry val) {
  using enum square_isometry;

  switch (val) {
    case rotate_cw:   return rotate_ccw;
    case rotate_ccw:  return rotate_cw;

    default:          return val;
  }
}



[[nodiscard]] square_isometry lut_multiply(square_isometry lhs, square_isometry rhs) {
  using enum square_isometry;

  //NOLINTBEGIN(*throw-by-value-catch-by-reference)

  switch (rhs) {
    case identity:
      return lhs;

    case rotate_cw:
      switch (lhs) {
        case identity:       return rotate_cw;
        case rotate_cw:      return rotate_half;
        case rotate_half:    return rotate_ccw;
        case rotate_ccw:     return identity;
        case flip_x:         return transpose;
        case transpose:      return flip_y;
        case flip_y:         return anti_transpose;
        case anti_transpose: return flip_x;
      }
      throw rotate_cw;

    case rotate_half:
      switch (lhs) {
        case identity:       return rotate_half;
        case rotate_cw:      return rotate_ccw;
        case rotate_half:    return identity;
        case rotate_ccw:     return rotate_cw;
        case flip_x:         return flip_y;
        case transpose:      return anti_transpose;
        case flip_y:         return flip_x;
        case anti_transpose: return transpose;
      }
      throw rotate_half;

    case rotate_ccw:
      switch (lhs) {
        case identity:       return rotate_ccw;
        case rotate_cw:      return identity;
        case rotate_half:    return rotate_cw;
        case rotate_ccw:     return rotate_half;
        case flip_x:         return anti_transpose;
        case transpose:      return flip_x;
        case flip_y:         return transpose;
        case anti_transpose: return flip_y;
      }
      throw rotate_ccw;

    case flip_x:
      switch (lhs) {
        case identity:       return flip_x;
        case rotate_cw:      return anti_transpose;
        case rotate_half:    return flip_y;
        case rotate_ccw:     return transpose;
        case flip_x:         return identity;
        case transpose:      return rotate_ccw;
        case flip_y:         return rotate_half;
        case anti_transpose: return rotate_cw;
      }
      throw flip_x;

    case transpose:
      switch (lhs) {
        case identity:       return transpose;
        case rotate_cw:      return flip_x;
        case rotate_half:    return anti_transpose;
        case rotate_ccw:     return flip_y;
        case flip_x:         return rotate_cw;
        case transpose:      return identity;
        case flip_y:         return rotate_ccw;
        case anti_transpose: return rotate_half;
      }
      throw transpose;

    case flip_y:
      switch (lhs) {
        case identity:       return flip_y;
        case rotate_cw:      return transpose;
        case rotate_half:    return flip_x;
        case rotate_ccw:     return anti_transpose;
        case flip_x:         return rotate_half;
        case transpose:      return rotate_cw;
        case flip_y:         return identity;
        case anti_transpose: return rotate_ccw;
      }
      throw flip_y;

    case anti_transpose:
      switch (lhs) {
        case identity:       return anti_transpose;
        case rotate_cw:      return flip_y;
        case rotate_half:    return transpose;
        case rotate_ccw:     return flip_x;
        case flip_x:         return rotate_ccw;
        case transpose:      return rotate_half;
        case flip_y:         return rotate_cw;
        case anti_transpose: return identity;
      }
      throw anti_transpose;
  }
  throw 42;
  //NOLINTEND(*throw-by-value-catch-by-reference)
}



int main() {
  using enum square_isometry;

  std::set<square_isometry> list = {
    identity, rotate_cw, rotate_half, rotate_ccw,
    flip_x, transpose, flip_y, anti_transpose
  };

  id_assert_eq(list.size(), 8u);



  for (auto iso: list) {
    std::cout << "iso: " << to_string(iso) << '\n' << std::flush;

    id_assert_eq(inverse(iso), lut_inverse(iso),
        "inverse(iso) == static_inverse(iso)");

    id_assert_eq(iso, iso * identity, "iso == iso * identity");
    id_assert_eq(iso, identity * iso, "iso == identity * iso");

    id_assert_eq(identity, pow_int(iso, 8), "identity == pow_int(iso, 8)");

    if (orientation_reversing(iso)) {
      id_assert_eq(identity, iso * iso,
          "identity == iso * iso | iso orientation reversing");
    } else {
      id_assert_eq(identity, pow_int(iso, 4),
          "identity == pow_int(iso, 4) | iso orientation preserving");
    }

    //NOLINTNEXTLINE(*redundant-expression)
    id_assert_eq(identity, iso / iso, "identity == iso / iso");



    for (auto jso: list) {
      std::cout << "jso: " << to_string(jso) << '\n' << std::flush;

      auto iso2 = iso;
      auto iso3 = iso;

      id_assert_eq(iso * jso, iso2 *= jso,        "iso * jso == (iso *= jso)");
      id_assert_eq(iso / jso, iso3 /= jso,        "iso / jso == (iso /= jso)");

      id_assert_eq(iso / jso, iso * inverse(jso), "iso / jso == (iso * inverse(jso))");

      id_assert_eq(iso * jso, lut_multiply(iso, jso),
          "iso * jso == lut_multiply(iso, jso)");
      id_assert_eq(iso / jso, lut_multiply(iso, lut_inverse(jso)),
          "iso1 / iso2 == lut_multiply(iso1, lut_inverse(iso2))");
    }
  }
}
