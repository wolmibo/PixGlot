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



void pixglot::convert_endian(pixel_buffer& pb, std::endian src, std::endian tgt) {
  if (src == tgt || byte_size(pb.format().format) < 2) {
    return;
  }

  if (byte_size(pb.format().format) == 2) {
    swap_bytes<u16>(pb.data());
  } else if (byte_size(pb.format().format) == 4) {
    swap_bytes<u32>(pb.data());
  } else {
    throw base_exception{
      "Unable to swap bytes",
      "No byte-swapping is implemented for data_format with byte_size == " +
        std::to_string(byte_size(pb.format().format))
    };
  }
}



void pixglot::convert_endian(pixel_storage& ps, std::endian src, std::endian tgt) {
  if (ps.storage_type() == pixel_target::pixel_buffer) {
    convert_endian(ps.pixels(), src, tgt);
  }
}



void pixglot::convert_endian(frame& f, std::endian target) {
  convert_endian(f.pixels, f.endianess, target);
  f.endianess = target;
}



void pixglot::convert_endian(image& img, std::endian target) {
  for (auto& f: img.frames) {
    convert_endian(f, target);
  }
}
