// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_IMAGE_HPP_INCLUDED
#define PIXGLOT_IMAGE_HPP_INCLUDED

#include "pixglot/frame.hpp"

#include <experimental/propagate_const>
#include <memory>
#include <span>
#include <string>



namespace pixglot {

class metadata;

class image {
  public:
    image(const image&) = delete;
    image(image&&) noexcept;
    image& operator=(const image&) = delete;
    image& operator=(image&&) noexcept;

    ~image();

    image();



    [[nodiscard]] std::span<const pixglot::frame> frames() const;
    [[nodiscard]] std::span<      pixglot::frame> frames();

    [[nodiscard]] operator               bool()  const;
    [[nodiscard]] bool                   empty() const;

    [[nodiscard]] size_t                 size() const;

    [[nodiscard]] const pixglot::frame&  frame(size_t = 0)  const;
    [[nodiscard]]       pixglot::frame&  frame(size_t = 0);

    [[nodiscard]] const pixglot::frame&  operator[](size_t) const;
    [[nodiscard]]       pixglot::frame&  operator[](size_t);

    [[nodiscard]] const pixglot::frame*  begin() const;
    [[nodiscard]]       pixglot::frame*  begin();

    [[nodiscard]] const pixglot::frame*  end() const;
    [[nodiscard]]       pixglot::frame*  end();




    [[nodiscard]] bool                         animated()      const;
    [[nodiscard]] std::span<const std::string> warnings()      const;
    [[nodiscard]] bool                         has_warnings()  const;

    [[nodiscard]] const pixglot::metadata&     metadata()      const;
    [[nodiscard]] pixglot::metadata&           metadata();



    [[nodiscard]] std::string_view             mime_type()     const;
    [[nodiscard]] pixglot::codec               codec()         const;

    void codec(pixglot::codec, std::string = {});



    void            add_warning(std::string);
    pixglot::frame& add_frame  (pixglot::frame);



  private:
    class impl;
    std::experimental::propagate_const<std::unique_ptr<impl>> impl_;
};


[[nodiscard]] std::string to_string(const image&);

bool validate_file_extension(image&, std::string_view);

}

#endif // PIXGLOT_IMAGE_HPP_INCLUDED
