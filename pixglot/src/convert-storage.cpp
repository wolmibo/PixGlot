#include "pixglot/conversions.hpp"
#include "pixglot/pixel-storage.hpp"



void pixglot::convert_storage(pixel_storage& pixels, pixel_target target) {
  if (pixels.storage_type() == target) {
    return;
  }



  if (pixels.storage_type() == pixel_target::pixel_buffer) {
    if (target == pixel_target::gl_texture) {
      pixels = gl_texture(pixels.pixels());
    }

  } else if (pixels.storage_type() == pixel_target::gl_texture) {
    if (target == pixel_target::pixel_buffer) {
      pixels = pixels.texture().download();
    }
  }
}



void pixglot::convert_storage(frame& frm, pixel_target target) {
  if (target == pixel_target::gl_texture && frm.endianess != std::endian::native) {
    convert_endian(frm, std::endian::native);
  }
  convert_storage(frm.pixels, target);
}



void pixglot::convert_storage(image& img, pixel_target target) {
  for (auto& f: img.frames) {
    convert_storage(f, target);
  }
}
