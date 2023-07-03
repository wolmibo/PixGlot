// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_FRAME_HPP_INCLUDED
#define PIXGLOT_FRAME_HPP_INCLUDED

#include "pixglot/gl-texture.hpp"
#include "pixglot/pixel-buffer.hpp"
#include "pixglot/square-isometry.hpp"

#include <chrono>
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
    frame(pixel_buffer pixels ) : storage_{std::move(pixels )} {}
    frame(gl_texture   texture) : storage_{std::move(texture)} {}



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



    [[nodiscard]] square_isometry           orientation() const { return orientation_; }
    [[nodiscard]] std::chrono::microseconds duration()    const { return duration_;    }
    [[nodiscard]] float                     gamma()       const { return gamma_;       }
    [[nodiscard]] alpha_mode                alpha()       const { return alpha_;       }
    [[nodiscard]] std::endian               endian()      const { return endian_;      }



    void orientation(square_isometry           iso     ) { orientation_ = iso;      }
    void duration   (std::chrono::microseconds duration) { duration_    = duration; }
    void gamma      (float                     gamma   ) { gamma_       = gamma;    }
    void alpha      (alpha_mode                alpha   ) { alpha_       = alpha;    }
    void endian     (std::endian               endian  ) { endian_      = endian;   }



  private:
    using pixel_storage = std::variant<
      pixel_buffer,
      gl_texture
    >;
    pixel_storage             storage_;

    square_isometry           orientation_{square_isometry::identity};
    alpha_mode                alpha_      {alpha_mode::straight};
    float                     gamma_      {gamma_s_rgb};
    std::endian               endian_     {std::endian::native};
    std::chrono::microseconds duration_   {0};
};

[[nodiscard]] std::string to_string(const frame&);

}

#endif // PIXGLOT_FRAME_HPP_INCLUDED
