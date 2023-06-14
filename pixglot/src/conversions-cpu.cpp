#include "pixglot/pixel-buffer.hpp"
#include "pixglot/square-isometry.hpp"


namespace pixglot::details {
  std::endian convert(
      pixel_buffer&              /*pixels*/,
      std::endian                endian,
      std::optional<std::endian> /*target_endian*/,
      pixel_format               /*target_format*/,
      int                        /*premutliply*/,
      float                      /*gamma_diff*/,
      square_isometry            /*transform*/
  ) {
    return endian;
  }
}
