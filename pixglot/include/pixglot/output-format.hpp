// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_OUTPUT_FORMAT_HPP_INCLUDED
#define PIXGLOT_OUTPUT_FORMAT_HPP_INCLUDED

#include "pixglot/frame.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/pixel-storage.hpp"
#include "pixglot/preference.hpp"
#include "pixglot/square-isometry.hpp"



namespace pixglot {

class image;

struct output_format {
  preference<pixel_target>    target;

  preference<bool>            expand_gray_to_rgb;
  preference<bool>            add_alpha;
  preference<data_format>     component_type;

  preference<alpha_mode>      alpha;
  preference<float>           gamma;
  preference<std::endian>     endianess;

  preference<square_isometry> orientation;



  [[nodiscard]] bool satisfied_by(const pixel_format&) const;
  [[nodiscard]] bool satisfied_by(const frame&)        const;
  [[nodiscard]] bool satisfied_by(const image&)        const;

  [[nodiscard]] bool preference_satisfied_by(const pixel_format&) const;
  [[nodiscard]] bool preference_satisfied_by(const frame&)        const;
  [[nodiscard]] bool preference_satisfied_by(const image&)        const;

  void enforce();
};



void make_format_compatible(frame&, const output_format&, bool = false);
void make_format_compatible(image&, const output_format&, bool = false);

}

#endif // PIXGLOT_OUTPUT_FORMAT_HPP_INCLUDED
