// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_DETAILS_TIFF_ORIENTATION_HPP_INCLUDED
#define PIXGLOT_DETAILS_TIFF_ORIENTATION_HPP_INCLUDED

#include "pixglot/square-isometry.hpp"

namespace pixglot::details {



constexpr std::array<square_isometry, 8> tiff_orientations {
  square_isometry::identity,
  square_isometry::flip_x,
  square_isometry::rotate_half,
  square_isometry::flip_y,
  square_isometry::transpose,
  square_isometry::rotate_ccw,
  square_isometry::anti_transpose,
  square_isometry::rotate_cw
};



[[nodiscard]] constexpr square_isometry square_isometry_from_tiff(int orientation) {
  if (orientation < 1 || orientation > std::ssize(tiff_orientations)) {
    return square_isometry::identity;
  }

  //NOLINTNEXTLINE(*-bounds-constant-array-index)
  return tiff_orientations[orientation - 1];
}



[[nodiscard]] constexpr square_isometry square_isometry_from_tiff(char orientation) {
  return square_isometry_from_tiff(orientation - '0');
}


}

#endif // PIXGLOT_DETAILS_TIFF_ORIENTATION_HPP_INCLUDED
