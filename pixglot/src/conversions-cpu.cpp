#include "pixglot/pixel-buffer.hpp"
#include "pixglot/square-isometry.hpp"


namespace pixglot::details {
  void apply_orientation(pixel_buffer&, square_isometry);



  void convert(
      pixel_buffer&              pixels,
      std::optional<std::endian> /*target_endian*/,
      pixel_format               target_format,
      int                        /*premultiply*/,
      float                      /*gamma_diff*/,
      square_isometry            transform
  ) {
    if (transform != square_isometry::identity
        && pixels.format().size() < target_format.size()) {
      apply_orientation(pixels, transform);
      transform = square_isometry::identity;
    }




    if (transform != square_isometry::identity) {
      apply_orientation(pixels, transform);
    }
  }
}
