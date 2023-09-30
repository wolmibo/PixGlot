#include "pixglot/exception.hpp"
#include "pixglot/pixel-buffer.hpp"
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
  [[nodiscard]] std::endian swap_endian(std::endian endian) {
    if (endian == std::endian::little) {
      return std::endian::big;
    }
    return std::endian::little;
  }



  void swap_bytes(std::span<std::byte> buffer, size_t chunk_size) {
    if (chunk_size == 1) {
      return;
    }

    if (chunk_size == 2) {
      ::swap_bytes<pixglot::u16>(buffer);
    } else if (chunk_size == 4) {
      ::swap_bytes<pixglot::u32>(buffer);
    } else {
      throw pixglot::base_exception{
        "Unable to swap bytes",
        "No byte-swapping is implemented for data_format with byte_size == " +
          std::to_string(chunk_size)
      };
    }
  }



  void apply_byte_swap(pixglot::pixel_buffer& pb) {
    pb.endian(swap_endian(pb.endian()));

    if (byte_size(pb.format().format) < 2) {
      return;
    }

    static_assert(pixglot::pixel_buffer::padding() % 4 == 0);

    swap_bytes(pb.data(), byte_size(pb.format().format));
  }
}
