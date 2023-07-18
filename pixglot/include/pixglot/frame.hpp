// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_FRAME_HPP_INCLUDED
#define PIXGLOT_FRAME_HPP_INCLUDED

#include "pixglot/gl-texture.hpp"
#include "pixglot/pixel-buffer.hpp"
#include "pixglot/square-isometry.hpp"

#include <chrono>
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
  gl_texture   = 1,
  no_pixels    = 2,
};

[[nodiscard]] std::string_view stringify(storage_type);

[[nodiscard]] inline std::string to_string(storage_type t) {
  return std::string{stringify(t)};
};





class frame_source_info;

class frame_view {
  friend class frame;

  public:
    frame_view(const frame_view&);
    frame_view(frame_view&&) noexcept;
    frame_view& operator=(const frame_view&);
    frame_view& operator=(frame_view&&) noexcept;

    ~frame_view();



    [[nodiscard]] storage_type type() const;

    [[nodiscard]] const gl_texture&   texture() const;
    [[nodiscard]] const pixel_buffer& pixels()  const;



    [[nodiscard]] pixel_format format() const;

    [[nodiscard]] size_t width()  const;
    [[nodiscard]] size_t height() const;



    [[nodiscard]] const frame_source_info& source_info() const;



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



    [[nodiscard]] square_isometry                 orientation() const;
    [[nodiscard]] std::chrono::microseconds       duration()    const;
    [[nodiscard]] float                           gamma()       const;
    [[nodiscard]] pixglot::alpha_mode             alpha_mode()  const;

    [[nodiscard]] std::optional<std::string_view> name() const;



    [[nodiscard]] size_t id() const;



  private:
    class impl;
    std::shared_ptr<impl> impl_;

    frame_view(std::shared_ptr<impl>);
};





class frame : public frame_view  {
  public:
    frame(const frame&)            = delete;
    frame(frame&&) noexcept;
    frame& operator=(const frame&) = delete;
    frame& operator=(frame&&) noexcept;

    ~frame();

    frame(size_t, size_t, pixel_format = {});
    frame(pixel_buffer);
    frame(gl_texture);



    void reset(size_t, size_t, pixel_format = {});
    void reset(pixel_buffer);
    void reset(gl_texture);



    using frame_view::texture;
    using frame_view::pixels;

    [[nodiscard]] gl_texture&   texture();
    [[nodiscard]] pixel_buffer& pixels();



    using frame_view::source_info;

    [[nodiscard]] frame_source_info& source_info();



    using frame_view::visit_storage;

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



    using frame_view::orientation;
    using frame_view::duration;
    using frame_view::gamma;
    using frame_view::alpha_mode;

    using frame_view::name;



    void orientation(square_isometry);
    void duration   (std::chrono::microseconds);
    void gamma      (float);
    void alpha_mode (pixglot::alpha_mode);

    void name(std::string);
    void clear_name();
};

[[nodiscard]] std::string to_string(const frame&);

}

#endif // PIXGLOT_FRAME_HPP_INCLUDED
