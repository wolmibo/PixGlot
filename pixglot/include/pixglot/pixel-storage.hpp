// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_PIXEL_STOARGE_HPP_INCLUDED
#define PIXGLOT_PIXEL_STOARGE_HPP_INCLUDED

#include "pixglot/gl-texture.hpp"
#include "pixglot/pixel-buffer.hpp"

#include <variant>



namespace pixglot {

using pixel_storage_base = std::variant<
  pixel_buffer,
  gl_texture
>;



enum class pixel_target {
  pixel_buffer = 0,
  gl_texture   = 1
};

[[nodiscard]] std::string_view stringify(pixel_target);

[[nodiscard]] inline std::string to_string(pixel_target t) {
  return std::string{stringify(t)};
};



class pixel_storage {
  public:
    pixel_storage(pixel_buffer&&  buffer) : storage_{std::move(buffer)} {}
    pixel_storage(gl_texture&&   texture) : storage_{std::move(texture)} {}



    [[nodiscard]] size_t width() const {
      return std::visit([](auto&& arg) { return arg.width(); }, storage_);
    }


    [[nodiscard]] size_t height() const {
      return std::visit([](auto&& arg) { return arg.height(); }, storage_);
    }


    [[nodiscard]] pixel_format format() const {
      return std::visit([](auto&& arg) { return arg.format(); }, storage_);
    }



    template<typename Fnc>
    auto visit(Fnc&& f) {
      return std::visit(std::forward<Fnc>(f), storage_);
    }

    template<typename Fnc>
    auto visit(Fnc&& f) const {
      return std::visit(std::forward<Fnc>(f), storage_);
    }



    [[nodiscard]] pixel_target storage_type() const {
      return static_cast<pixel_target>(storage_.index());
    }



    [[nodiscard]] const gl_texture& texture() const {
      return std::get<gl_texture>(storage_);
    }

    [[nodiscard]] gl_texture& texture() {
      return std::get<gl_texture>(storage_);
    }



    [[nodiscard]] const pixel_buffer& pixels() const {
      return std::get<pixel_buffer>(storage_);
    }

    [[nodiscard]] pixel_buffer& pixels() {
      return std::get<pixel_buffer>(storage_);
    }



  private:
    pixel_storage_base storage_;
};



[[nodiscard]] std::string to_string(const pixel_storage&);

}

#endif // PIXGLOT_PIXEL_STOARGE_HPP_INCLUDED
