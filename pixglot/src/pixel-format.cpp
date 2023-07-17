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





std::string_view pixglot::stringify(data_format df) {
  switch (df) {
    case data_format::u8:  return "u8";
    case data_format::u16: return "u16";
    case data_format::u32: return "u32";
    case data_format::f16: return "f16";
    case data_format::f32: return "f32";
  }
  return "<invalid data_format>";
}



std::string_view pixglot::stringify(color_channels cc) {
  switch (cc) {
    case color_channels::gray:   return "gray";
    case color_channels::gray_a: return "gray_a";
    case color_channels::rgb:    return "rgb";
    case color_channels::rgba:   return "rgba";
  }
  return "<invalid color_channels>";
}



std::string pixglot::to_string(pixel_format pf) {
  return to_string(pf.channels) + "_" + to_string(pf.format);
}
