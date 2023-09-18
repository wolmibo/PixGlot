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




  template<typename T, int Pre>
    requires (std::is_same_v<typename T::component, f32> && -1 <= Pre && Pre <= 1)
  struct alpha_conversion {};

  template<typename T>
  struct alpha_conversion<T, 0> {
    static void apply(T& /*pix*/) {}
  };

  template<typename T>
    requires (!has_color(T::format().channels) && has_alpha(T::format().channels))
  struct alpha_conversion<T, -1> {
    static void apply(T& pix) {
      if (pix.a > 0.f) {
        pix.v /= pix.a;
      }
    }
  };

  template<typename T>
    requires (!has_color(T::format().channels) && has_alpha(T::format().channels))
  struct alpha_conversion<T, 1> {
    static void apply(T& pix) {
      pix.v *= pix.a;
    }
  };

  template<typename T>
    requires (has_color(T::format().channels) && has_alpha(T::format().channels))
  struct alpha_conversion<T, -1> {
    static void apply(T& pix) {
      if (pix.a > 0.f) {
        pix.r /= pix.a;
        pix.g /= pix.a;
        pix.b /= pix.a;
      }
    }
  };

  template<typename T>
    requires (has_color(T::format().channels) && has_alpha(T::format().channels))
  struct alpha_conversion<T, 1> {
    static void apply(T& pix) {
      pix.r *= pix.a;
      pix.g *= pix.a;
      pix.b *= pix.a;
    }
  };
}





namespace pixglot::details {
  void apply_orientation(pixel_buffer&, square_isometry);



  void convert(
      pixel_buffer&              pixels,
      std::optional<std::endian> /*target_endian*/,
      pixel_format               target_format,
      int                        premultiply,
      float                      /*gamma_diff*/,
      square_isometry            transform
  ) {
    if (transform != square_isometry::identity
        && pixels.format().size() < target_format.size()) {
      apply_orientation(pixels, transform);
      transform = square_isometry::identity;
    }

    if (has_alpha(pixels.format().channels)) {
      premultiply = std::clamp(premultiply, -1, 1);
    } else {
      premultiply = 0;
    }




    if (transform != square_isometry::identity) {
      apply_orientation(pixels, transform);
    }
  }
}
