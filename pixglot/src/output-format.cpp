#include "pixglot/conversions.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/gl-texture.hpp"
#include "pixglot/image.hpp"
#include "pixglot/output-format.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/pixel-storage.hpp"
#include "pixglot/preference.hpp"
#include "pixglot/square-isometry.hpp"

#include <algorithm>
#include <optional>

using namespace pixglot;





void output_format::enforce() {
  target.enforce();
  expand_gray_to_rgb.enforce();
  add_alpha.enforce();
  component_type.enforce();
  alpha.enforce();
  gamma.enforce();
  endianess.enforce();
  orientation.enforce();
}





bool output_format::satisfied_by(const image& img) const {
  return std::ranges::all_of(
    img.frames,
    [this](const auto& f) {
      return satisfied_by(f);
    }
  );
}



bool output_format::preference_satisfied_by(const image& img) const {
  return std::ranges::all_of(
    img.frames,
    [this](const auto& f) {
      return preference_satisfied_by(f);
    }
  );
}





namespace {
  [[nodiscard]] bool alpha_satisfied_by(preference<alpha_mode> pref, alpha_mode alpha) {
    return alpha == alpha_mode::none || pref.satisfied_by(alpha);
  }



  [[nodiscard]] bool alpha_preference_satisfied_by(
      preference<alpha_mode> pref,
      alpha_mode             alpha
  ) {
    return alpha == alpha_mode::none || pref.preference_satisfied_by(alpha);
  }
}



bool output_format::satisfied_by(const frame& f) const {
  return satisfied_by(f.pixels.format())
    && alpha_satisfied_by(alpha, f.alpha)
    && gamma.satisfied_by(f.gamma)
    && orientation.satisfied_by(f.orientation)
    && endianess.satisfied_by(f.endianess)
    && target.satisfied_by(f.pixels.storage_type());
}



bool output_format::preference_satisfied_by(const frame& f) const {
  return preference_satisfied_by(f.pixels.format())
    && alpha_preference_satisfied_by(alpha, f.alpha)
    && gamma.preference_satisfied_by(f.gamma)
    && orientation.preference_satisfied_by(f.orientation)
    && endianess.preference_satisfied_by(f.endianess)
    && target.preference_satisfied_by(f.pixels.storage_type());
}





namespace {
  [[nodiscard]] bool expand_gray_to_rgb_satisfied_by(
      preference<bool> expand,
      color_channels   cc
  ) {
    return has_color(cc) || !*expand || expand.level() != preference_level::require;
  }



  [[nodiscard]] bool expand_gray_to_rgb_preference_satisfied_by(
      preference<bool> expand,
      color_channels   cc
  ) {
    return has_color(cc) || !*expand || expand.level() == preference_level::whatever;
  }



  [[nodiscard]] bool add_alpha_satisfied_by(preference<bool> add, color_channels cc) {
    return has_alpha(cc) || !*add || add.level() != preference_level::require;
  }



  [[nodiscard]] bool add_alpha_preference_satisfied_by(
      preference<bool> add,
      color_channels   cc
  ) {
    return has_alpha(cc) || !*add || add.level() == preference_level::whatever;
  }
}



bool output_format::satisfied_by(const pixel_format& fmt) const {
  return component_type.satisfied_by(fmt.format)
    && expand_gray_to_rgb_satisfied_by(expand_gray_to_rgb, fmt.channels)
    && add_alpha_satisfied_by(add_alpha, fmt.channels);
}



bool output_format::preference_satisfied_by(const pixel_format& fmt) const {
  return component_type.preference_satisfied_by(fmt.format)
    && expand_gray_to_rgb_preference_satisfied_by(expand_gray_to_rgb, fmt.channels)
    && add_alpha_preference_satisfied_by(add_alpha, fmt.channels);
}





namespace pixglot::details {
  void convert(gl_texture&, pixel_format, int, float, square_isometry);

  std::endian convert(pixel_buffer&, std::endian, std::optional<std::endian>,
                      pixel_format, int, float, square_isometry);
}




namespace {
  void apply_conversions(
      frame&               f,
      const output_format& fmt,
      pixel_format         target_format,
      int                  premultiply,
      float                gamma,
      square_isometry      transform
  ) {
    if (fmt.target.level() == preference_level::require) {
      convert_storage(f.pixels, *fmt.target);
    }

    if (f.pixels.storage_type() == pixel_target::gl_texture) {
      details::convert(f.pixels.texture(), target_format, premultiply, gamma, transform);
    } else {
      auto target_endian = fmt.endianess.level() == preference_level::require ?
        std::optional{*fmt.endianess} : std::nullopt;

      auto endian = details::convert(f.pixels.pixels(), f.endianess, target_endian,
          target_format, premultiply, gamma, transform);

      f.endianess = endian;
    }
  }



  void make_compatible(frame& f, const output_format& fmt) {
    square_isometry transform{};
    if (fmt.orientation.level() == preference_level::require) {
      transform = inverse(*fmt.orientation) * f.orientation;
      f.orientation = *fmt.orientation;
    }


    int premultiply{};
    if (fmt.alpha.level() == preference_level::require) {
      if (f.alpha == alpha_mode::straight && *fmt.alpha == alpha_mode::premultiplied) {
        premultiply = 1;
        f.alpha = alpha_mode::premultiplied;
      } else if (f.alpha == alpha_mode::premultiplied
                 && *fmt.alpha == alpha_mode::straight) {
        premultiply = -1;
        f.alpha = alpha_mode::straight;
      }
    }


    float gamma{1.f};
    if (fmt.gamma.level() == preference_level::require) {
      gamma = f.gamma / *fmt.gamma;
      f.gamma = *fmt.gamma;
    }


    auto target_format = f.pixels.format();
    if (fmt.add_alpha.level() == preference_level::require) {
      target_format.channels = add_alpha(target_format.channels);

      if (f.alpha == alpha_mode::none) {
        if (fmt.alpha.has_preference()) {
          f.alpha = *fmt.alpha;
        } else {
          f.alpha = alpha_mode::straight;
        }
      }
    }
    if (fmt.expand_gray_to_rgb.level() == preference_level::require) {
      target_format.channels = add_color(target_format.channels);
    }
    if (fmt.component_type.level() == preference_level::require) {
      target_format.format = *fmt.component_type;
    }


    apply_conversions(f, fmt, target_format, premultiply, gamma, transform);
  }
}





void pixglot::make_format_compatible(
    frame&               f,
    const output_format& fmt,
    bool                 enforce
) {
  if (enforce) {
    auto enforced = fmt;
    enforced.enforce();
    make_compatible(f, fmt);
    return;
  }

  make_compatible(f, fmt);
}



void pixglot::make_format_compatible(
    image&               img,
    const output_format& fmt,
    bool                 enforce
) {
  if (enforce) {
    auto enforced = fmt;
    enforced.enforce();
    for (auto& f: img. frames) {
      make_compatible(f, fmt);
    }
    return;
  }

  for (auto& f : img.frames) {
    make_compatible(f, fmt);
  }
}
