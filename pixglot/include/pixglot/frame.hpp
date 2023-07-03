// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_FRAME_HPP_INCLUDED
#define PIXGLOT_FRAME_HPP_INCLUDED

#include "pixglot/gl-texture.hpp"
#include "pixglot/pixel-buffer.hpp"
#include "pixglot/square-isometry.hpp"

#include <chrono>
#include <experimental/propagate_const>
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



    [[nodiscard]] storage_type type() const {
      return static_cast<storage_type>(storage_.index());
    }



    void reset(pixel_buffer pixels ) { storage_ = std::move(pixels);  }
    void reset(gl_texture   texture) { storage_ = std::move(texture); }



    [[nodiscard]] gl_texture&   texture() { return std::get<gl_texture  >(storage_); }
    [[nodiscard]] pixel_buffer& pixels()  { return std::get<pixel_buffer>(storage_); }

    [[nodiscard]] const gl_texture& texture() const {
      return std::get<gl_texture>(storage_);
    }

    [[nodiscard]] const pixel_buffer& pixels() const {
      return std::get<pixel_buffer>(storage_);
    }



    [[nodiscard]] pixel_format format() const {
      return std::visit([](auto&& arg) { return arg.format(); }, storage_);
    }

    [[nodiscard]] size_t width() const {
      return std::visit([](auto&& arg) { return arg.width(); }, storage_);
    }

    [[nodiscard]] size_t height() const {
      return std::visit([](auto&& arg) { return arg.height(); }, storage_);
    }



    template<typename Fnc>
    auto visit_storage(Fnc&& function) {
      return std::visit(std::forward<Fnc>(function), storage_);
    }

    template<typename Fnc>
    auto visit_storage(Fnc&& function) const {
      return std::visit(std::forward<Fnc>(function), storage_);
    }



    [[nodiscard]] square_isometry           orientation() const;
    [[nodiscard]] std::chrono::microseconds duration()    const;
    [[nodiscard]] float                     gamma()       const;
    [[nodiscard]] alpha_mode                alpha()       const;



    void orientation(square_isometry);
    void duration   (std::chrono::microseconds);
    void gamma      (float);
    void alpha      (alpha_mode);



  private:
    using pixel_storage = std::variant<
      pixel_buffer,
      gl_texture
    >;
    pixel_storage storage_;

    class impl;
    std::experimental::propagate_const<std::unique_ptr<impl>> impl_;
};

[[nodiscard]] std::string to_string(const frame&);

}

#endif // PIXGLOT_FRAME_HPP_INCLUDED
