#include "pixglot/conversions.hpp"
#include "pixglot/image.hpp"
#include "pixglot/output-format.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/pixel-storage.hpp"

#include <algorithm>

using namespace pixglot;



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





namespace {
  [[nodiscard]] bool must_match_preference(preference_level level, bool enforce) {
    return level == preference_level::require ||
      (enforce && level == preference_level::prefer);
  }



  template<typename T>
  void make_compatible(
      frame&               f,
      const preference<T>& pref,
      bool                 enforce,
      void                 (task)(frame&, T)
  ) {
    if (must_match_preference(pref.level(), enforce)) {
      task(f, pref.value());
    }
  }
}



void pixglot::make_format_compatible(
    frame&               f,
    const output_format& fmt,
    bool                 enforce
) {
  make_compatible(f, fmt.gamma,       enforce, convert_gamma);
  make_compatible(f, fmt.orientation, enforce, convert_orientation);

  make_compatible(f, fmt.target,      enforce, convert_storage);

  if (f.pixels.storage_type() == pixel_target::pixel_buffer) {
    // must be last since other conversions might convert to endian::native
    make_compatible(f, fmt.endianess,   enforce, convert_endian);
  }
}



void pixglot::make_format_compatible(
    image&               img,
    const output_format& fmt,
    bool                 enforce
) {
  for (auto& f : img.frames) {
    make_format_compatible(f, fmt, enforce);
  }
}
