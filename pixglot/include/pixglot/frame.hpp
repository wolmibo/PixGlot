// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_FRAME_HPP_INCLUDED
#define PIXGLOT_FRAME_HPP_INCLUDED

#include "pixglot/pixel-storage.hpp"
#include "pixglot/square-isometry.hpp"

#include <chrono>
#include <string>
#include <string_view>



namespace pixglot {

enum class alpha_mode {
  none,
  straight,
  premultiplied
};

[[nodiscard]] std::string_view stringify(alpha_mode);

[[nodiscard]] inline std::string to_string(alpha_mode am) {
  return std::string{stringify(am)};
}





static constexpr float gamma_s_rgb {2.2f};
static constexpr float gamma_linear{1.f};




struct frame {
  pixel_storage             pixels;
  square_isometry           orientation{square_isometry::identity};

  alpha_mode                alpha      {alpha_mode::straight};
  float                     gamma      {2.2};

  std::endian               endianess  {std::endian::native};

  std::chrono::microseconds duration   {0};
};

[[nodiscard]] std::string to_string(const frame&);

}

#endif // PIXGLOT_FRAME_HPP_INCLUDED
