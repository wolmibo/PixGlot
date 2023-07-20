#include "pixglot/conversions.hpp"
#include "pixglot/exception.hpp"
#include "pixglot/utils/cast.hpp"

#include <algorithm>
#include <vector>

using namespace pixglot;



namespace {
  template<size_t ChunkSize>
  using chunk_type = std::array<std::byte, ChunkSize>;


  template<size_t ChunkSize>
  std::span<chunk_type<ChunkSize>> chunked(std::span<std::byte> range) {
    return utils::interpret_as_greedy<chunk_type<ChunkSize>>(range);
  }

  template<size_t ChunkSize>
  std::span<const chunk_type<ChunkSize>> chunked(std::span<const std::byte> range) {
    return utils::interpret_as_greedy<const chunk_type<ChunkSize>>(range);
  }





  template<size_t ChunkSize>
  void flip_x_sized(pixel_buffer& pixels) {
    for (size_t y = 0; y < pixels.height(); ++y) {
      std::ranges::reverse(chunked<ChunkSize>(pixels.row_bytes(y)));
    }
  }



  void flip_x(pixel_buffer& pixels) {
    switch (pixels.format().size()) {
      case 1:  flip_x_sized<1> (pixels); break;
      case 2:  flip_x_sized<2> (pixels); break;
      case 3:  flip_x_sized<3> (pixels); break;
      case 4:  flip_x_sized<4> (pixels); break;
      case 6:  flip_x_sized<6> (pixels); break;
      case 8:  flip_x_sized<8> (pixels); break;
      case 12: flip_x_sized<12>(pixels); break;
      case 16: flip_x_sized<16>(pixels); break;
      default:
        throw pixglot::bad_pixel_format{pixels.format()};
    }
  }





  template<size_t ChunkSize>
  void rotate_half_sized(pixel_buffer& pixels) {
    for (size_t y = 0; y < pixels.height() / 2; ++y) {
      auto source = chunked<ChunkSize>(pixels.row_bytes(y));
      auto target = chunked<ChunkSize>(pixels.row_bytes(pixels.height() - 1 - y));

      std::swap_ranges(source.begin(), source.end(), target.rbegin());
    }

    if (pixels.height() % 2 != 0) {
      std::ranges::reverse(chunked<ChunkSize>(pixels.row_bytes(pixels.height() / 2)));
    }
  }



  void rotate_half(pixel_buffer& pixels) {
    switch (pixels.format().size()) {
      case 1:  rotate_half_sized<1> (pixels); break;
      case 2:  rotate_half_sized<2> (pixels); break;
      case 3:  rotate_half_sized<3> (pixels); break;
      case 4:  rotate_half_sized<4> (pixels); break;
      case 6:  rotate_half_sized<6> (pixels); break;
      case 8:  rotate_half_sized<8> (pixels); break;
      case 12: rotate_half_sized<12>(pixels); break;
      case 16: rotate_half_sized<16>(pixels); break;
      default:
        throw pixglot::bad_pixel_format{pixels.format()};
    }
  }





  template<size_t ChunkSize>
  void transpose_sized(const pixel_buffer& source, pixel_buffer& target) {
    std::vector<std::span<chunk_type<ChunkSize>>> target_rows;
    target_rows.reserve(target.height());
    for (size_t y = 0; y < target.height(); ++y) {
      target_rows.emplace_back(chunked<ChunkSize>(target.row_bytes(y)));
    }

    for (size_t y = 0; y < source.height(); ++y) {
      auto srow = chunked<ChunkSize>(source.row_bytes(y));

      for (size_t x = 0; x < source.width(); ++x) {
        target_rows[x][y] = srow[x];
      }
    }
  }



  void transpose(const pixel_buffer& source, pixel_buffer& target) {
    switch (source.format().size()) {
      case 1:  transpose_sized<1> (source, target); break;
      case 2:  transpose_sized<2> (source, target); break;
      case 3:  transpose_sized<3> (source, target); break;
      case 4:  transpose_sized<4> (source, target); break;
      case 6:  transpose_sized<6> (source, target); break;
      case 8:  transpose_sized<8> (source, target); break;
      case 12: transpose_sized<12>(source, target); break;
      case 16: transpose_sized<16>(source, target); break;
      default:
        throw pixglot::bad_pixel_format{source.format()};
    }
  }





  void flip_y(pixel_buffer& pixels) {
    for (size_t y = 0; y < pixels.height() / 2; ++y) {
      std::ranges::swap_ranges(
          pixels.row_bytes(y),
          pixels.row_bytes(pixels.height() - 1 - y)
      );
    }
  }





  void transform_flips_xy(
      pixel_buffer&   source,
      pixel_buffer&   target,
      square_isometry orientation
  ) {
    switch (orientation) {
      case square_isometry::rotate_cw:
        flip_y(source);
        transpose(source, target);
        break;

      case square_isometry::rotate_ccw:
        transpose(source, target);
        flip_y(target);
        break;

      case square_isometry::transpose:
        transpose(source, target);
        break;

      case square_isometry::anti_transpose:
        transpose(source, target);
        rotate_half(target);
        break;

      default:
        throw orientation;
    }
  }
}



namespace pixglot::details {
  void apply_orientation(pixel_buffer& pixels, square_isometry orientation) {
    switch (orientation) {
      case square_isometry::identity:
        break;

      case square_isometry::flip_y:
        flip_y(pixels);
        break;

      case square_isometry::flip_x:
        flip_x(pixels);
        break;

      case square_isometry::rotate_half:
        rotate_half(pixels);
        break;

      default: {
        pixel_buffer source{std::move(pixels)};
        pixels = pixel_buffer{source.height(), source.width(), source.format()};

        transform_flips_xy(source, pixels, orientation);
      } break;
    }
  }
}
