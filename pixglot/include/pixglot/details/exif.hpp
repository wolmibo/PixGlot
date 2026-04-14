// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_DETAILS_EXIF_HPP_INCLUDED
#define PIXGLOT_DETAILS_EXIF_HPP_INCLUDED

#include "pixglot/square-isometry.hpp"

#include <optional>
#include <span>



namespace pixglot {
  class metadata;
}



namespace pixglot::details {

class decoder;

void fill_exif_metadata(std::span<const std::byte>, metadata&, details::decoder&,
                        std::optional<std::reference_wrapper<square_isometry>> = {});

[[nodiscard]] bool is_exif(std::span<const std::byte>);

}
#endif // PIXGLOT_DETAILS_EXIF_HPP_INCLUDED
