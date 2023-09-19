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

  [[nodiscard]] bool needs_gamma_correction(float exp) {
    return std::abs(exp - 1.f) > 1e-2f;
  }





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



  template<typename T, bool GC, int Pre>
    requires std::is_same_v<typename T::component, f32>
  void arithmetic_transforms(T& pix, float exp) {
    // FIXME: alpha conversion should always be done in linear space
    gamma_correction<T, GC>::apply(pix, exp);
    alpha_conversion<T, Pre>::apply(pix);
  }



  template<size_t Size>
  struct integral_from_size {};

  template<>
  struct integral_from_size<2> { using type = u16; };
  template<>
  struct integral_from_size<4> { using type = u32; };

  template<size_t Size, typename T>
    requires (sizeof(T) == Size)
  void swap_bytes(T& value) {
    using interim = typename integral_from_size<Size>::type;
    value = std::bit_cast<T>(std::byteswap(std::bit_cast<interim>(value)));
  }



  template<typename T, bool Swap>
  struct byte_swapper {};

  template<typename T, bool Swap>
    requires (Swap == false) || (sizeof(T::Component) == 1)
  struct byte_swapper<T, Swap> {
    static void apply(T& /*pix*/) {}
  };

  template<typename T>
    requires (sizeof(T::Component) == 2) || (sizeof(T::Component) == 4)
  struct byte_swapper<T, true> {
    static void apply(T& pix) {
      static constexpr size_t count = n_channels(T::format().channels);

      static_assert(count * sizeof(typename T::Component) == sizeof(T));

      //NOLINTNEXTLINE(*-reinterpret-cast)
      for (auto& comp: std::span{reinterpret_cast<typename T::Component*>(&pix), count}) {
        swap_bytes<sizeof(T::Component)>(comp);
      }
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
      float                      gamma_exp,
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

    bool needs_gc = needs_gamma_correction(gamma_exp);




    if (transform != square_isometry::identity) {
      apply_orientation(pixels, transform);
    }
  }
}
