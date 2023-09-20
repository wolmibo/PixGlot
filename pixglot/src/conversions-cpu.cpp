#include "pixglot/conversions.hpp"
#include "pixglot/pixel-buffer.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/square-isometry.hpp"

#include <cmath>

using namespace pixglot;




namespace {
  template<pixel_type T>
    requires std::is_same_v<typename T::component, f32>
  void apply_gamma_correction(T& pix, float exp) {
    if constexpr (has_color(T::format().channels)) {
      pix.r = std::pow(pix.r, exp);
      pix.g = std::pow(pix.g, exp);
      pix.b = std::pow(pix.b, exp);
    } else {
      pix.v = std::pow(pix.v, exp);
    }
  }



  [[nodiscard]] bool needs_gamma_correction(float exp) {
    return std::abs(exp - 1.f) > 1e-2f;
  }





  template<pixel_type T, int Pre>
    requires (std::is_same_v<typename T::component, f32> && -1 <= Pre && Pre <= 1)
  void apply_alpha_conversion(T& pix) {
    if constexpr (has_alpha(T::format().channels)) {
      if constexpr (Pre < 0) {
        if (pix.a > 0.f) {
          if constexpr (has_color(T::format().channels)) {
            pix.r /= pix.a;
            pix.g /= pix.a;
            pix.b /= pix.a;
          } else {
            pix.v /= pix.v;
          }
        }
      } else if constexpr (Pre > 0) {
        if constexpr (has_color(T::format().channels)) {
          pix.r *= pix.a;
          pix.g *= pix.a;
          pix.b *= pix.a;
        } else {
          pix.v *= pix.v;
        }
      }
    }
  }
}





namespace pixglot::details {
  void apply_orientation(pixel_buffer&, square_isometry);



  void convert(
      pixel_buffer&              pixels,
      std::optional<std::endian> target_endian,
      pixel_format               target_format,
      int                        premultiply,
      float                      gamma_exp,
      square_isometry            transform
  ) {
    if (transform != square_isometry::identity
        && pixels.format().size() < target_format.size()) {
      apply_orientation(pixels, transform);
      transform = square_isometry::identity;
    }

    bool gamma_correction = needs_gamma_correction(gamma_exp);

    if (gamma_correction || premultiply != 0) {
      convert_pixel_format(pixels, pixel_format {
          .format   = data_format::f32,
          .channels = pixels.format().channels
      }, {});
    }


    convert_pixel_format(pixels, target_format, target_endian);


    if (transform != square_isometry::identity) {
      apply_orientation(pixels, transform);
    }
  }
}
