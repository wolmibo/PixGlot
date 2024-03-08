// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_UTILS_GL_HPP_INCLUDED
#define PIXGLOT_UTILS_GL_HPP_INCLUDED

#include "pixglot/pixel-buffer.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/utils/int_cast.hpp"

#include <array>
#include <stdexcept>

#include <GL/gl.h>



namespace pixglot::utils {

[[nodiscard]] constexpr pixel_format pixel_format_from_gl_internal(GLint fmt) {
  switch (fmt) {
    case GL_R8:          return gray<u8>::format();
    case GL_R16:         return gray<u16>::format();
    case GL_R32UI:       return gray<u32>::format();
    case GL_R16F:        return gray<f16>::format();
    case GL_R32F:        return gray<f32>::format();

    case GL_RG8:         return gray_a<u8>::format();
    case GL_RG16:        return gray_a<u16>::format();
    case GL_RG32UI:      return gray_a<u32>::format();
    case GL_RG16F:       return gray_a<f16>::format();
    case GL_RG32F:       return gray_a<f32>::format();

    case GL_RGB8:        return rgb<u8>::format();
    case GL_RGB16_SNORM: return rgb<u16>::format();
    case GL_RGB32UI:     return rgb<u32>::format();
    case GL_RGB16F:      return rgb<f16>::format();
    case GL_RGB32F:      return rgb<f32>::format();

    case GL_RGBA8:       return rgba<u8>::format();
    case GL_RGBA16:      return rgba<u16>::format();
    case GL_RGBA32UI:    return rgba<u32>::format();
    case GL_RGBA16F:     return rgba<f16>::format();
    case GL_RGBA32F:     return rgba<f32>::format();

    default: break;
  }

  throw std::invalid_argument{"unsupported internal format"};
}



[[nodiscard]] constexpr GLint gl_internal_format(pixel_format pf) {
  switch (pf.channels) {
    case color_channels::gray:
      switch (pf.format) {
        case data_format::u8:  return GL_R8;
        case data_format::u16: return GL_R16;
        case data_format::u32: return GL_R32UI;
        case data_format::f16: return GL_R16F;
        case data_format::f32: return GL_R32F;
      }
      break;
    case color_channels::gray_a:
      switch (pf.format) {
        case data_format::u8:  return GL_RG8;
        case data_format::u16: return GL_RG16;
        case data_format::u32: return GL_RG32UI;
        case data_format::f16: return GL_RG16F;
        case data_format::f32: return GL_RG32F;
      }
      break;
    case color_channels::rgb:
      switch (pf.format) {
        case data_format::u8:  return GL_RGB8;
        case data_format::u16: return GL_RGB16_SNORM;
        case data_format::u32: return GL_RGB32UI;
        case data_format::f16: return GL_RGB16F;
        case data_format::f32: return GL_RGB32F;
      }
      break;
    case color_channels::rgba:
      switch (pf.format) {
        case data_format::u8:  return GL_RGBA8;
        case data_format::u16: return GL_RGBA16;
        case data_format::u32: return GL_RGBA32UI;
        case data_format::f16: return GL_RGBA16F;
        case data_format::f32: return GL_RGBA32F;
      }
      break;
  }

  throw bad_pixel_format(pf);
}



[[nodiscard]] constexpr GLenum gl_format(pixel_format pf) {
  switch (pf.channels) {
    case color_channels::gray:   return GL_RED;
    case color_channels::gray_a: return GL_RG;
    case color_channels::rgb:    return GL_RGB;
    case color_channels::rgba:   return GL_RGBA;
  }

  throw bad_pixel_format(pf);
}



[[nodiscard]] constexpr GLenum gl_type(pixel_format pf) {
  switch (pf.format) {
    case data_format::u8:  return GL_UNSIGNED_BYTE;
    case data_format::u16: return GL_UNSIGNED_SHORT;
    case data_format::u32: return GL_UNSIGNED_INT;
    case data_format::f16: return GL_HALF_FLOAT;
    case data_format::f32: return GL_FLOAT;
  }

  throw bad_pixel_format(pf);
}



inline void gl_swizzle_mask(color_channels channels) {
  static constexpr std::array<std::array<GLint, 4>, 3> swizzleMask {
    std::array<GLint, 4>{GL_ZERO, GL_ZERO, GL_ZERO, GL_ONE},
    std::array<GLint, 4>{GL_RED,  GL_RED,  GL_RED,  GL_ONE},
    std::array<GLint, 4>{GL_RED,  GL_RED,  GL_RED,  GL_ALPHA}
  };

  if (auto n = n_channels(channels); n < swizzleMask.size()) {
    //NOLINTNEXTLINE(*constant-array-index)
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask[n].data());
  }
}



[[nodiscard]] constexpr GLint gl_unpack_alignment(size_t stride) {
  GLint alignment = 8;

  while (stride % alignment != 0) {
    alignment >>= 1;
  }

  return alignment;
}



[[nodiscard]] inline GLint gl_pixels_per_stride(const pixel_buffer& buffer) {
  return int_cast<GLint>(buffer.stride() / buffer.format().size());
}


}

#endif // PIXGLOT_UTILS_GL_HPP_INCLUDED
