// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_DETAILS_NO_PIXELS_HPP_INCLUDED
#define PIXGLOT_DETAILS_NO_PIXELS_HPP_INCLUDED

#include "pixglot/pixel-format.hpp"




namespace pixglot::details {

class no_pixels {
  public:
    no_pixels(size_t width, size_t height, pixel_format format = {}) :
      width_{width}, height_{height}, format_{format}
    {}



    [[nodiscard]] size_t       width()  const { return width_;  }
    [[nodiscard]] size_t       height() const { return height_; }
    [[nodiscard]] pixel_format format() const { return format_; }



  private:
    size_t       width_;
    size_t       height_;
    pixel_format format_;
};

}

#endif // PIXGLOT_DETAILS_NO_PIXELS_HPP_INCLUDED
