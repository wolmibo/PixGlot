#include "pixglot/conversions.hpp"
#include "pixglot/pixel-storage.hpp"
#include "pixglot/utils/cast.hpp"

#include <bit>



namespace {
  template<typename T>
  void swap_bytes(std::span<std::byte> buffer) {
    for (T& x: pixglot::utils::interpret_as_greedy<T>(buffer)) {
      x = std::byteswap(x);
    }
  }
}



namespace pixglot::details {
  void apply_byte_swap(pixglot::pixel_buffer& pb) {
    if (byte_size(pb.format().format) < 2) {
      return;
    }

    static_assert(pixglot::pixel_buffer::padding() % 4 == 0);

    if (byte_size(pb.format().format) == 2) {
      swap_bytes<pixglot::u16>(pb.data());
    } else if (byte_size(pb.format().format) == 4) {
      swap_bytes<pixglot::u32>(pb.data());
    } else {
      throw pixglot::base_exception{
        "Unable to swap bytes",
        "No byte-swapping is implemented for data_format with byte_size == " +
          std::to_string(byte_size(pb.format().format))
      };
    }
  }
}
