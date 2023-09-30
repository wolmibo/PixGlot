// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_DETAILS_EXIF_HPP_INCLUDED
#define PIXGLOT_DETAILS_EXIF_HPP_INCLUDED

#include "pixglot/square-isometry.hpp"

#include <span>



namespace pixglot {
  class metadata;
}



namespace pixglot::details {

class decoder;

void fill_exif_metadata(std::span<const std::byte>, metadata&, details::decoder&,
                        square_isometry* = nullptr);

[[nodiscard]] bool is_exif(std::span<const std::byte>);

}
#endif // PIXGLOT_DETAILS_EXIF_HPP_INCLUDED
