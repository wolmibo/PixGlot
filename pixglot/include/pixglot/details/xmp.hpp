// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_DETAILS_XMP_HPP_INCLUDED
#define PIXGLOT_DETAILS_XMP_HPP_INCLUDED

#include <string_view>


namespace pixglot::details {

class decoder;

[[nodiscard]] bool fill_xmp_metadata(std::string_view, details::decoder&);

}

#endif // PIXGLOT_DETAILS_XMP_HPP_INCLUDED
