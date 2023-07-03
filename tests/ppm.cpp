#include "common.hpp"

#include <array>

#include <pixglot/decode.hpp>

using namespace pixglot;



static constexpr std::array<unsigned char, 42> black_white {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x00, 0x00, 0x00, 0x00, 0xff,
  0xff, 0x00, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x00, 0x00, 0x00, 0xff, 0xff,
  0xff, 0x00, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x00, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};





void test_general(const image& img) {
  id_assert_eq(img.frames.size(), 1u);
  const auto& frame = img.frames[0];

  id_assert_eq(frame.width(),  6u);
  id_assert_eq(frame.height(), 7u);

  id_assert_eq(frame.type(), storage_type::pixel_buffer);
}





void test_black_white(const image& img) {
  const auto& pixels = img.frames[0].pixels();

  id_assert_eq(pixels.format(), gray<u8>::format());

  for (size_t y = 0; y < pixels.height(); ++y) {
    id_assert_eq(pixels.row_bytes(y),
        std::as_bytes(std::span{black_white}.subspan(y * 6, 6)));
  }
}





//NOLINTBEGIN(*-pointer-arithmetic)
int main(int argc, char** argv) {
  // usage: .. <file> [--gray]
  id_assert(argc == 2 || argc == 3);

  id_assert_eq(determine_codec(argv[1]), codec::ppm);

  auto image = decode(reader{argv[1]});

  test_general(image);

  if (argc > 2 && argv[2] == std::string_view{"--gray"}) {
    test_black_white(image);
  }
}
//NOLINTEND(*-pointer-arithmetic)
