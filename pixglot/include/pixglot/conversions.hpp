// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_COVERSIONS_HPP_INCLUDED
#define PIXGLOT_COVERSIONS_HPP_INCLUDED

#include "pixglot/image.hpp"



namespace pixglot {

void convert_gamma(image&, float);
void convert_gamma(frame&, float);
void convert_gamma(pixel_buffer&,  float, float);
void convert_gamma(gl_texture&,    float, float);

void convert_endian(image&,        std::endian = std::endian::native);
void convert_endian(frame&,        std::endian = std::endian::native);
void convert_endian(pixel_buffer&, std::endian = std::endian::native);

void convert_orientation(image&, square_isometry = {});
void convert_orientation(frame&, square_isometry = {});
void convert_orientation(pixel_buffer&,  square_isometry, square_isometry = {});
void convert_orientation(gl_texture&,    square_isometry, square_isometry = {});

void convert_storage(image&, storage_type);
void convert_storage(frame&, storage_type);

void convert_pixel_format(image&,        pixel_format, std::optional<std::endian> = {});
void convert_pixel_format(frame&,        pixel_format, std::optional<std::endian> = {});
void convert_pixel_format(pixel_buffer&, pixel_format, std::optional<std::endian> = {});

}

#endif // PIXGLOT_COVERSIONS_HPP_INCLUDED
