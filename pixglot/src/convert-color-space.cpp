#include "pixglot/conversions.hpp"
#include "pixglot/pixel-format-conversion.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/utils/cast.hpp"

#include <cmath>
#include <limits>
#include <span>



using namespace pixglot;

namespace {
  [[nodiscard]] constexpr bool needs_correction(float src, float tgt) {
    return std::abs(src - tgt) >= std::numeric_limits<float>::epsilon();
  }
}



void pixglot::convert_gamma(pixel_buffer& /*pixels*/, float src, float tgt) {
  if (!needs_correction(src, tgt)) {
    return;
  }

  //auto exponent = src / tgt;
}



void pixglot::convert_gamma(gl_texture& /*texture*/, float src, float tgt) {
  if (!needs_correction(src, tgt)) {
    return;
  }
}



void pixglot::convert_gamma(pixel_storage& store, float source, float target) {
  store.visit([source, target](auto& arg) { convert_gamma(arg, source, target); });
}



void pixglot::convert_gamma(frame& f, float target) {
  if (needs_correction(f.gamma, target)) {
    convert_endian(f, std::endian::native);
  }
  convert_gamma(f.pixels, f.gamma, target);
  f.gamma = target;
}



void pixglot::convert_gamma(image& img, float target) {
  for (auto& f: img.frames) {
    convert_gamma(f, target);
  }
}
