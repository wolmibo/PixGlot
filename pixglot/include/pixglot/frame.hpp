// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_FRAME_HPP_INCLUDED
#define PIXGLOT_FRAME_HPP_INCLUDED

#include "pixglot/gl-texture.hpp"
#include "pixglot/pixel-buffer.hpp"
#include "pixglot/square-isometry.hpp"

#include <chrono>
#include <experimental/propagate_const>
#include <functional>
#include <string>
#include <string_view>



namespace pixglot {

enum class alpha_mode {
  none,
  straight,
  premultiplied
};

[[nodiscard]] std::string_view stringify(alpha_mode);

[[nodiscard]] inline std::string to_string(alpha_mode am) {
  return std::string{stringify(am)};
}





static constexpr float gamma_s_rgb {2.2f};
static constexpr float gamma_linear{1.f};





enum class storage_type {
  pixel_buffer = 0,
  gl_texture   = 1
};

[[nodiscard]] std::string_view stringify(storage_type);

[[nodiscard]] inline std::string to_string(storage_type t) {
  return std::string{stringify(t)};
};





class frame {
  public:
    frame(const frame&)            = delete;
    frame(frame&&) noexcept;
    frame& operator=(const frame&) = delete;
    frame& operator=(frame&&) noexcept;

    ~frame();

    frame(pixel_buffer);
    frame(gl_texture);



    [[nodiscard]] storage_type type() const;



    void reset(pixel_buffer);
    void reset(gl_texture);

    [[nodiscard]] gl_texture&   texture();
    [[nodiscard]] pixel_buffer& pixels();

    [[nodiscard]] const gl_texture&   texture() const;
    [[nodiscard]] const pixel_buffer& pixels()  const;



    [[nodiscard]] pixel_format format() const;

    [[nodiscard]] size_t width()  const;
    [[nodiscard]] size_t height() const;



    template<typename Fnc>
    auto visit_storage(Fnc&& function) {
      switch (type()) {
        default:
        case storage_type::pixel_buffer:
          return std::invoke(std::forward<Fnc>(function), pixels());
        case storage_type::gl_texture:
          return std::invoke(std::forward<Fnc>(function), texture());
      }
    }

    template<typename Fnc>
    auto visit_storage(Fnc&& function) const {
      switch (type()) {
        default:
        case storage_type::pixel_buffer:
          return std::invoke(std::forward<Fnc>(function), pixels());
        case storage_type::gl_texture:
          return std::invoke(std::forward<Fnc>(function), texture());
      }
    }



    [[nodiscard]] square_isometry           orientation() const;
    [[nodiscard]] std::chrono::microseconds duration()    const;
    [[nodiscard]] float                     gamma()       const;
    [[nodiscard]] pixglot::alpha_mode       alpha_mode()  const;



    void orientation(square_isometry);
    void duration   (std::chrono::microseconds);
    void gamma      (float);
    void alpha_mode (pixglot::alpha_mode);



  private:
    class impl;
    std::experimental::propagate_const<std::shared_ptr<impl>> impl_;
};

[[nodiscard]] std::string to_string(const frame&);

}

#endif // PIXGLOT_FRAME_HPP_INCLUDED
