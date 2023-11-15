#include "pixglot/exception.hpp"
#include "pixglot/square-isometry.hpp"
#include "pixglot/pixel-buffer.hpp"



namespace pixglot::details {
  void convert(
      pixel_buffer&              /*pixels*/,
      std::optional<std::endian> /*target_endian*/,
      pixel_format               /*target_format*/,
      int                        /*premultiply*/,
      float                      /*gamma*/,
      square_isometry            /*transform*/
  ) {
    throw base_exception{"conversion function for cpu disabled"};
  }



  void apply_orientation(pixel_buffer& /*pixels*/, square_isometry /*orientation*/) {
    throw base_exception{"orientation conversion for cpu disabled"};
  }
}



namespace pixglot {
  void convert_pixel_format(
      pixel_buffer&              /*pixels*/,
      pixel_format               /*target_format*/,
      std::optional<std::endian> /*target_endian*/
  ) {
    throw base_exception{"pixel format conversion for cpu disabled"};
  }
}
