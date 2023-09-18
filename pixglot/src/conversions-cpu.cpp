#include "pixglot/pixel-buffer.hpp"
#include "pixglot/square-isometry.hpp"


namespace pixglot::details {
  void convert(
      pixel_buffer&              /*pixels*/,
      std::optional<std::endian> /*target_endian*/,
      pixel_format               /*target_format*/,
      int                        /*premultiply*/,
      float                      /*gamma_diff*/,
      square_isometry            /*transform*/
  ) {
  }
}
