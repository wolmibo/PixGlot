#include "pixglot/gl-texture.hpp"
#include "pixglot/pixel-buffer.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/utils/cast.hpp"
#include "pixglot/utils/gl.hpp"

#include <stdexcept>

#include <GL/gl.h>



void pixglot::gl_texture::texture_id::cleanup() {
  if (id != 0) {
    glDeleteTextures(1, &id);
  }
}





namespace {
  [[nodiscard]] GLuint create_texture(const pixglot::pixel_format& format) {
    GLuint id{0};

    glGenTextures(1, &id);

    glBindTexture(GL_TEXTURE_2D, id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    pixglot::utils::gl_swizzle_mask(format.channels);

    return id;
  }



  void teximage(const pixglot::gl_texture& tex, const void* data) {
    glTexImage2D(
      GL_TEXTURE_2D,
      0,
      pixglot::utils::gl_internal_format(tex.format()),
      static_cast<GLsizei>(tex.width()),
      static_cast<GLsizei>(tex.height()),
      0,
      pixglot::utils::gl_format(tex.format()),
      pixglot::utils::gl_type(tex.format()),
      data
    );
  }
}



pixglot::gl_texture::gl_texture(const pixel_buffer& buffer) :
  width_ {buffer.width()},
  height_{buffer.height()},
  format_{buffer.format()},

  id_    {create_texture(format_)}
{
  glPixelStorei(GL_UNPACK_ALIGNMENT,  utils::gl_unpack_alignment(buffer.stride()));
  glPixelStorei(GL_UNPACK_ROW_LENGTH, utils::gl_pixels_per_stride(buffer));

  teximage(*this, buffer.data().data());
}





pixglot::gl_texture::gl_texture(size_t width, size_t height, pixel_format format) :
  width_ {width},
  height_{height},
  format_{format},

  id_    {create_texture(format_)}
{
  teximage(*this, nullptr);
}





void pixglot::gl_texture::update() {
  glBindTexture(GL_TEXTURE_2D, id_.id);

  GLint buffer{0};

  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &buffer);
  if (buffer < 0) {
    throw std::runtime_error{"opengl texture has negative width"};
  }
  width_ = buffer;

  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &buffer);
  if (buffer < 0) {
    throw std::runtime_error{"opengl texture has negative height"};
  }
  height_ = buffer;

  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &buffer);
  format_ = utils::pixel_format_from_gl_internal(buffer);
}





namespace {
  template<size_t ChunkSize>
  void repack_row_sized(
      pixglot::pixel_format format,
      std::span<const std::byte>  source,
      std::span<std::byte>        target
  ) {
    using chunk = std::array<std::byte, ChunkSize>;
    auto src = pixglot::utils::interpret_as<const chunk>(source);
    auto tgt = pixglot::utils::interpret_as<      chunk>(target);

    auto src_it = src.begin();
    auto tgt_it = tgt.begin();

    auto tgt_end = tgt.end();

    switch (format.channels) {
      case pixglot::color_channels::gray:
        while (tgt_it != tgt_end) {
          *tgt_it++ = *src_it;
          src_it += 4;
        }
        break;

      case pixglot::color_channels::gray_a:
        while (tgt_it != tgt_end) {
          *tgt_it++ = *src_it;
          src_it += 3;
          *tgt_it++ = *src_it++;
        }
        break;

      case pixglot::color_channels::rgb:
        while (tgt_it != tgt_end) {
          *tgt_it++ = *src_it++;
          *tgt_it++ = *src_it++;
          *tgt_it++ = *src_it;
          src_it += 2;
        }
        break;

      case pixglot::color_channels::rgba:
        break;
    }
  }



  void repack_row(
      pixglot::pixel_format format,
      std::span<const std::byte>  source,
      std::span<std::byte>        target
  ) {
    if (pixglot::n_channels(format.channels) == 4) {
      std::ranges::copy(source, target.begin());
      return;
    }

    switch (byte_size(format.format)) {
      case 1: repack_row_sized<1>(format, source, target); break;
      case 2: repack_row_sized<2>(format, source, target); break;
      case 4: repack_row_sized<4>(format, source, target); break;
    }
  }
}



pixglot::pixel_buffer pixglot::gl_texture::download() {
  update();

  pixel_buffer pixels{width(), height(), format()};



  size_t stride_bytes = width() * 4 * byte_size(format().format);
  GLint alignment{0};
  glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
  if (alignment < 1) {
    throw std::runtime_error{"opengl texture has non-positive alignment"};
  }

  if (stride_bytes % alignment != 0) {
    stride_bytes += alignment - (stride_bytes % alignment);
  }



  if (n_channels(format().channels) == 4 && stride_bytes == pixels.stride()) {
    glGetTexImage(GL_TEXTURE_2D, 0, utils::gl_format(format()), utils::gl_type(format()),
        pixels.data().data());
    return pixels;
  }



  std::vector<std::byte> buffer(height() * stride_bytes);
  glGetTexImage(GL_TEXTURE_2D, 0, utils::gl_format(format()), utils::gl_type(format()),
      buffer.data());



  for (size_t y = 0; y < height(); ++y) {
    repack_row(
        format(),
        std::span{buffer}
          .subspan(y * stride_bytes, width() * 4 * byte_size(format().format)),
        pixels.row_bytes(y)
    );
  }

  return pixels;
}





void pixglot::gl_texture::bind() const {
  glBindTexture(GL_TEXTURE_2D, id_.id);
}
