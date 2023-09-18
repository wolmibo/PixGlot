#include "pixglot/pixel-buffer.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/square-isometry.hpp"

#include <cmath>

using namespace pixglot;




namespace {
  template<typename T, bool GC>
    requires std::is_same_v<typename T::component, f32>
  struct gamma_correction {
    static void apply(T& /*pix*/, float /*exp*/) {}
  };

  template<typename T>
    requires (!has_color(T::format().channels))
  struct gamma_correction<T, true> {
    static void apply(T& pix, float exp) {
      pix.v = std::pow(pix.v, exp);
    }
  };

  template<typename T>
    requires (has_color(T::format().channels))
  struct gamma_correction<T, true> {
    static void apply(T& pix, float exp) {
      pix.r = std::pow(pix.r, exp);
      pix.g = std::pow(pix.g, exp);
      pix.b = std::pow(pix.b, exp);
    }
  };
}





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
