#include "pixglot/conversions.hpp"
#include "pixglot/gl-texture.hpp"
#include "pixglot/pixel-buffer.hpp"
#include "pixglot/pixel-format-conversion.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/square-isometry.hpp"
#include "pixglot/utils/cast.hpp"

#include <cmath>
#include <limits>
#include <span>

using namespace pixglot;



namespace pixglot::details {
  void convert(gl_texture&, pixel_format, int, float, square_isometry);

  void convert(pixel_buffer&, std::optional<std::endian>,
                      pixel_format, int, float, square_isometry);

  void apply_orientation(pixel_buffer&, square_isometry);
  void apply_byte_swap(pixel_buffer&);
}





namespace {
  template<typename T>
  void convert_image(image& img, void (function)(frame&, T), T value) {
    for (auto& f: img.frames()) {
      function(f, value);
    }
  }
}





void pixglot::convert_gamma(image& img, float target) {
  convert_image(img, convert_gamma, target);
}



void pixglot::convert_gamma(frame& f, float target) {
  if (f.type() == storage_type::gl_texture) {
    convert_gamma(f.texture(), f.gamma(), target);
  } else {
    details::convert(f.pixels(), {}, f.format(), 0, target / f.gamma(), {});
  }
  f.gamma(target);
}



void pixglot::convert_gamma(pixel_buffer& pb, float current, float target) {
  details::convert(pb, {}, pb.format(), 0, target / current, {});
}



void pixglot::convert_gamma(gl_texture& tex, float current, float target) {
  details::convert(tex, tex.format(), 0, target / current, {});
}





void pixglot::convert_endian(image& img, std::endian target) {
  convert_image(img, convert_endian, target);
}



void pixglot::convert_endian(frame& f, std::endian target) {
  if (f.type() == storage_type::pixel_buffer) {
    convert_endian(f.pixels(), target);
  } 
}



void pixglot::convert_endian(pixel_buffer& pb, std::endian tgt) {
  if (pb.endian() != tgt) {
    details::apply_byte_swap(pb);
    pb.endian(tgt);
  }
}





void pixglot::convert_orientation(image& img, square_isometry target) {
  convert_image(img, convert_orientation, target);
}



void pixglot::convert_orientation(frame& f, square_isometry target) {
  f.visit_storage([src=f.orientation(), target](auto& arg) {
      convert_orientation(arg, src, target);
  });
  f.orientation(target);
}



void pixglot::convert_orientation(
    pixel_buffer&   pixels,
    square_isometry source,
    square_isometry target
) {
  if (source == target) {
    return;
  }
  details::apply_orientation(pixels, inverse(target) * source);
}



void pixglot::convert_orientation(
    gl_texture&     texture,
    square_isometry source,
    square_isometry target
) {
  if (source == target) {
    return;
  }
  details::convert(texture, texture.format(), 0, 1.f, inverse(target) * source);
}





void pixglot::convert_storage(image& img, storage_type target) {
  convert_image(img, convert_storage, target);
}



void pixglot::convert_storage(frame& frm, storage_type target) {
  if (frm.type() == target) {
    return;
  }

  if (frm.type() == storage_type::pixel_buffer) {
    if (target == storage_type::gl_texture) {
      convert_endian(frm, std::endian::native);
      frm.reset(gl_texture(frm.pixels()));
    }

  } else if (frm.type() == storage_type::gl_texture) {
    if (target == storage_type::pixel_buffer) {
      frm.reset(frm.texture().download());
    }
  }
}
