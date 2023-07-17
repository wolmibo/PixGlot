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






std::string_view pixglot::stringify(square_isometry iso) {
  using enum square_isometry;

  switch (iso) {
    case identity:       return "identity";
    case rotate_cw:      return "rotate_cw";
    case rotate_half:    return "rotate_half";
    case rotate_ccw:     return "rotate_ccw";

    case flip_x:         return "flip_x";
    case transpose:      return "transpose";
    case flip_y:         return "flip_y";
    case anti_transpose: return "anti_transpose";
  }

  return "<invalid square_isometry>";
}
