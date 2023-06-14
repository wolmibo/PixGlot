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

  std::endian convert(pixel_buffer&, std::endian, std::optional<std::endian>,
                      pixel_format, int, float, square_isometry);

  void apply_orientation(pixel_buffer&, square_isometry);
  void apply_byte_swap(pixel_buffer&);
}





namespace {
  template<typename T>
  void convert_image(image& img, void (function)(frame&, T), T value) {
    for (auto& f: img.frames) {
      function(f, value);
    }
  }
}





void pixglot::convert_gamma(image& img, float target) {
  convert_image(img, convert_gamma, target);
}



void pixglot::convert_gamma(frame& f, float target) {
  if (f.pixels.storage_type() == pixel_target::gl_texture) {
    convert_gamma(f.pixels.texture(), f.gamma, target);
  } else {
    f.endianess = details::convert(f.pixels.pixels(), f.endianess, {}, f.pixels.format(),
                                   0, target / f.gamma, {});
  }
  f.gamma = target;
}



void pixglot::convert_gamma(pixel_storage& storage, float current, float target) {
  storage.visit([current, target](auto& arg) { convert_gamma(arg, current, target); });
}



void pixglot::convert_gamma(pixel_buffer& pb, float current, float target) {
  details::convert(pb, std::endian::native, {}, pb.format(), 0, target / current, {});
}



void pixglot::convert_gamma(gl_texture& tex, float current, float target) {
  details::convert(tex, tex.format(), 0, target / current, {});
}





void pixglot::convert_endian(image& img, std::endian target) {
  convert_image(img, convert_endian, target);
}



void pixglot::convert_endian(frame& f, std::endian target) {
  convert_endian(f.pixels, f.endianess, target);
  f.endianess = target;
}



void pixglot::convert_endian(pixel_storage& ps, std::endian src, std::endian tgt) {
  if (ps.storage_type() == pixel_target::pixel_buffer && src != tgt) {
    details::apply_byte_swap(ps.pixels());
  }
}



void pixglot::convert_endian(pixel_buffer& pb, std::endian src, std::endian tgt) {
  if (src != tgt) {
    details::apply_byte_swap(pb);
  }
}





void pixglot::convert_orientation(image& img, square_isometry target) {
  convert_image(img, convert_orientation, target);
}



void pixglot::convert_orientation(frame& f, square_isometry target) {
  convert_orientation(f.pixels, f.orientation, target);
  f.orientation = target;
}



void pixglot::convert_orientation(
    pixel_storage&  storage,
    square_isometry src,
    square_isometry tgt
) {
  storage.visit([src, tgt](auto& arg) { convert_orientation(arg, src, tgt); });
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





void pixglot::convert_storage(image& img, pixel_target target) {
  convert_image(img, convert_storage, target);
}



void pixglot::convert_storage(frame& frm, pixel_target target) {
  if (target == pixel_target::gl_texture && frm.endianess != std::endian::native) {
    convert_endian(frm, std::endian::native);
  }
  convert_storage(frm.pixels, target);
}



void pixglot::convert_storage(pixel_storage& storage, pixel_target target) {
  if (storage.storage_type() == target) {
    return;
  }

  if (storage.storage_type() == pixel_target::pixel_buffer) {
    if (target == pixel_target::gl_texture) {
      storage = gl_texture(storage.pixels());
    }

  } else if (storage.storage_type() == pixel_target::gl_texture) {
    if (target == pixel_target::pixel_buffer) {
      storage = storage.texture().download();
    }
  }
}
