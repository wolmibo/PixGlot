#include "pixglot/pixel-format.hpp"



pixglot::color_channels pixglot::add_alpha(color_channels cc) {
  switch (cc) {
    case color_channels::rgb:  return color_channels::rgba;
    case color_channels::gray: return color_channels::gray_a;
    default: return cc;
  }
}



pixglot::color_channels pixglot::add_color(color_channels cc) {
  switch (cc) {
    case color_channels::gray:   return color_channels::rgb;
    case color_channels::gray_a: return color_channels::rgba;
    default: return cc;
  }
}
