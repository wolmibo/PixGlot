// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_IMAGE_HPP_INCLUDED
#define PIXGLOT_IMAGE_HPP_INCLUDED

#include "pixglot/frame.hpp"

#include <string>
#include <vector>



namespace pixglot {

struct image {
  std::vector<frame>       frames  {};
  std::vector<std::string> warnings{};
  bool                     animated{false};
};


[[nodiscard]] std::string to_string(const image&);

}

#endif // PIXGLOT_IMAGE_HPP_INCLUDED
