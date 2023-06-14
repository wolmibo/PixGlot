#include "pixglot/square-isometry.hpp"



std::array<float, 16> pixglot::square_isometry_to_mat4x4(square_isometry iso) {
  switch (iso) {
    default:
    case square_isometry::identity:
      return {
         1.f,  0.f, 0.f, 0.f,
         0.f,  1.f, 0.f, 0.f,
         0.f,  0.f, 1.f, 0.f,
         0.f,  0.f, 0.f, 1.f
      };
    case square_isometry::flip_x:
      return {
        -1.f,  0.f, 0.f, 0.f,
         0.f,  1.f, 0.f, 0.f,
         0.f,  0.f, 1.f, 0.f,
         0.f,  0.f, 0.f, 1.f
      };
    case square_isometry::rotate_cw:
      return {
         0.f,  1.f, 0.f, 0.f,
        -1.f,  0.f, 0.f, 0.f,
         0.f,  0.f, 1.f, 0.f,
         0.f,  0.f, 0.f, 1.f
      };
    case square_isometry::transpose:
      return {
         0.f, -1.f, 0.f, 0.f,
        -1.f,  0.f, 0.f, 0.f,
         0.f,  0.f, 1.f, 0.f,
         0.f,  0.f, 0.f, 1.f
      };
    case square_isometry::rotate_half:
      return {
        -1.f,  0.f, 0.f, 0.f,
         0.f, -1.f, 0.f, 0.f,
         0.f,  0.f, 1.f, 0.f,
         0.f,  0.f, 0.f, 1.f
      };
    case square_isometry::flip_y:
      return {
         1.f,  0.f, 0.f, 0.f,
         0.f, -1.f, 0.f, 0.f,
         0.f,  0.f, 1.f, 0.f,
         0.f,  0.f, 0.f, 1.f
      };
    case square_isometry::rotate_ccw:
      return {
         0.f, -1.f, 0.f, 0.f,
         1.f,  0.f, 0.f, 0.f,
         0.f,  0.f, 1.f, 0.f,
         0.f,  0.f, 0.f, 1.f
      };
    case square_isometry::anti_transpose:
      return {
         0.f,  1.f, 0.f, 0.f,
         1.f,  0.f, 0.f, 0.f,
         0.f,  0.f, 1.f, 0.f,
         0.f,  0.f, 0.f, 1.f
      };
  }
}
