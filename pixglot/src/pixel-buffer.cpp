#include "pixglot/pixel-buffer.hpp"





std::string pixglot::to_string(const pixel_buffer& pixels) {
  return std::to_string(pixels.width()) + "x" + std::to_string(pixels.height())
    + "@" + to_string(pixels.format());
}
