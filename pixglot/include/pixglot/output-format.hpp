// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_OUTPUT_FORMAT_HPP_INCLUDED
#define PIXGLOT_OUTPUT_FORMAT_HPP_INCLUDED

#include "pixglot/frame.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/preference.hpp"
#include "pixglot/square-isometry.hpp"

#include <experimental/propagate_const>
#include <memory>



namespace pixglot {

class image;

class output_format {
  public:
    [[nodiscard]] static output_format standard();

    output_format(const output_format&);
    output_format(output_format&&) noexcept;
    output_format& operator=(const output_format&);
    output_format& operator=(output_format&&) noexcept;

    ~output_format();

    output_format();



    [[nodiscard]] const preference<pixglot::storage_type>& storage_type()       const;
    [[nodiscard]]       preference<pixglot::storage_type>& storage_type();

    [[nodiscard]] const preference<pixglot::data_format >& data_format()        const;
    [[nodiscard]]       preference<pixglot::data_format >& data_format();
    [[nodiscard]] const preference<std::endian          >& endian()             const;
    [[nodiscard]]       preference<std::endian          >& endian();

    [[nodiscard]] const preference<bool                 >& expand_gray_to_rgb() const;
    [[nodiscard]]       preference<bool                 >& expand_gray_to_rgb();
    [[nodiscard]] const preference<bool                 >& fill_alpha()         const;
    [[nodiscard]]       preference<bool                 >& fill_alpha();

    [[nodiscard]] const preference<pixglot::alpha_mode  >& alpha_mode()         const;
    [[nodiscard]]       preference<pixglot::alpha_mode  >& alpha_mode();
    [[nodiscard]] const preference<float                >& gamma()              const;
    [[nodiscard]]       preference<float                >& gamma();
    [[nodiscard]] const preference<square_isometry      >& orientation()        const;
    [[nodiscard]]       preference<square_isometry      >& orientation();



    void storage_type      (preference<pixglot::storage_type>);

    void data_format       (preference<pixglot::data_format>);
    void endian            (preference<std::endian>);

    void expand_gray_to_rgb(preference<bool>);
    void fill_alpha        (preference<bool>);

    void alpha_mode        (preference<pixglot::alpha_mode>);
    void gamma             (preference<float>);
    void orientation       (preference<square_isometry>);



    void enforce();



    [[nodiscard]] bool satisfied_by(color_channels) const;
    [[nodiscard]] bool satisfied_by(pixel_format)   const;
    [[nodiscard]] bool satisfied_by(const frame&)   const;
    [[nodiscard]] bool satisfied_by(const image&)   const;

    [[nodiscard]] bool preference_satisfied_by(color_channels) const;
    [[nodiscard]] bool preference_satisfied_by(pixel_format)   const;
    [[nodiscard]] bool preference_satisfied_by(const frame&)   const;
    [[nodiscard]] bool preference_satisfied_by(const image&)   const;



  private:
    class impl;
    std::experimental::propagate_const<std::unique_ptr<impl>> impl_;
};



void make_format_compatible(frame&, const output_format&, bool = false);
void make_format_compatible(image&, const output_format&, bool = false);

}

#endif // PIXGLOT_OUTPUT_FORMAT_HPP_INCLUDED
