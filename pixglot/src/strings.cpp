#include "pixglot/codecs.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/image.hpp"
#include "pixglot/pixel-buffer.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/pixel-storage.hpp"

#include <string>
#include <string_view>



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



std::string_view pixglot::stringify(square_isometry iso) {
  using enum square_isometry;

  switch (iso) {
    case identity:       return "identity";
    case rotate_cw:      return "rotate_cw";
    case rotate_half:    return "rotate_half";
    case rotate_ccw:     return "rotate_ccw";

    case flip_x:         return "flip_x";
    case transpose:      return "transpose";
    case flip_y:         return "flip_y";
    case anti_transpose: return "anti_transpose";
  }

  return "<invalid square_isometry>";
}



std::string_view pixglot::stringify(codec c) {
  switch (c) {
    case codec::jpeg: return "jpeg";
    case codec::png:  return "png";
    case codec::avif: return "avif";
    case codec::exr:  return "exr";
    case codec::ppm:  return "ppm";
    case codec::webp: return "webp";
    case codec::gif:  return "gif";
    case codec::jxl:  return "jxl";
  }

  return "<invalid codec>";
}



std::vector<std::string_view> pixglot::mime_types(codec c) {
  switch (c) {
    case codec::jpeg: return {"image/jpeg"};
    case codec::png:  return {"image/png"};
    case codec::avif: return {"image/avif"};
    case codec::exr:  return {"image/x-exr"};
    case codec::ppm:  return {
      "image/x-portable-bitmap",
      "image/x-portable-graymap",
      "image/x-portable-pixmap"
    };
    case codec::webp: return {"image/webp"};
    case codec::gif:  return {"image/gif"};
    case codec::jxl:  return {"image/jxl"};
  }
  return {};
}



std::string_view pixglot::stringify(alpha_mode a) {
  switch (a) {
    case alpha_mode::none:          return "none";
    case alpha_mode::straight:      return "straight";
    case alpha_mode::premultiplied: return "premultiplied";
  }
  return "<invalid alpha_mode>";
}



std::string_view pixglot::stringify(pixel_target t) {
  switch (t) {
    case pixel_target::pixel_buffer: return "pixel buffer";
    case pixel_target::gl_texture:   return "gl texture";
  }
  return "<invalid pixel_target>";
}



std::string pixglot::to_string(const gl_texture& texture) {
  return std::to_string(texture.width()) + "x" + std::to_string(texture.height())
    + "@" + to_string(texture.format()) + "(gl=" + std::to_string(texture.id()) + ")";
}



std::string pixglot::to_string(const pixel_buffer& pixels) {
  return std::to_string(pixels.width()) + "x" + std::to_string(pixels.height())
    + "@" + to_string(pixels.format());
}



std::string pixglot::to_string(const pixel_storage& storage) {
  return to_string(storage.storage_type()) + "="
    + storage.visit([](const auto& arg) { return to_string(arg); });
}



std::string pixglot::to_string(const frame& f) {
  return to_string(f.storage()) + " [trafo=" + to_string(f.orientation())
    + "; alpha=" + to_string(f.alpha()) + "; gamma=" + std::to_string(f.gamma()) + "]";
}



std::string pixglot::to_string(const image& img) {
  std::string buffer = "{";
  for (size_t i = 0; i < img.frames.size(); ++i) {
    if (i > 0) {
      buffer += ", ";
    }
    buffer += to_string(img.frames[i]);
  }
  buffer += std::string{"} [animated="} + (img.animated ? "y" : "n") + "; warnings: {";
  for (size_t i = 0; i < img.warnings.size(); ++i) {
    if (i > 0) {
      buffer += ", ";
    }
    buffer += img.warnings[i];
  }
  buffer += "}]";

  return buffer;
}
