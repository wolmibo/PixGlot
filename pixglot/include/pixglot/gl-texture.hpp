// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_GL_TEXTURE_HPP_INCLUDED
#define PIXGLOT_GL_TEXTURE_HPP_INCLUDED

#include "pixglot/pixel-format.hpp"

#include <utility>



namespace pixglot {

class pixel_buffer;

class gl_texture {
  public:
    explicit gl_texture(const pixel_buffer&);

    gl_texture(size_t, size_t, pixel_format = {});



    auto operator<=>(const gl_texture&) const = default;



    [[nodiscard]] bool         empty()  const { return id_.id == 0; }
    [[nodiscard]] operator     bool()   const { return !empty();    }

    [[nodiscard]] pixel_format format() const { return format_; }
    [[nodiscard]] size_t       width()  const { return width_;  }
    [[nodiscard]] size_t       height() const { return height_; }

    [[nodiscard]] unsigned int id()     const { return id_.id;  }

    void bind() const;



    [[nodiscard]] pixel_buffer download();

    void update();



  private:
    struct texture_id {
      texture_id(const texture_id&) = delete;
      texture_id& operator=(const texture_id&) = delete;

      texture_id(texture_id&& rhs) noexcept :
        id{std::exchange(rhs.id, 0)}
      {}

      texture_id& operator=(texture_id&& rhs) noexcept {
        cleanup();
        id = std::exchange(rhs.id, 0);
        return *this;
      }

      ~texture_id() { cleanup(); }

      explicit texture_id(unsigned int id = 0) : id{id} {}

      auto operator<=>(const texture_id&) const = default;

      void cleanup();

      unsigned int id{0};
    };



    size_t       width_ {0};
    size_t       height_{0};
    pixel_format format_{};

    texture_id   id_;
};



[[nodiscard]] std::string to_string(const gl_texture&);

}

#endif // PIXGLOT_GL_TEXTURE_HPP_INCLUDED
