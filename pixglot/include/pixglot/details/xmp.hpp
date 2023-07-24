// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_DETAILS_XMP_HPP_INCLUDED
#define PIXGLOT_DETAILS_XMP_HPP_INCLUDED

#include <string>


namespace pixglot {
  class metadata;
}



namespace pixglot::details {

class decoder;

void fill_xmp_metadata(std::string&&, metadata&, details::decoder&);

}

#endif // PIXGLOT_DETAILS_XMP_HPP_INCLUDED
