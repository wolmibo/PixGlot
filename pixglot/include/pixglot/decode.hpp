// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_DECODE_HPP_INCLUDED
#define PIXGLOT_DECODE_HPP_INCLUDED

#include "pixglot/codecs.hpp"
#include "pixglot/image.hpp"
#include "pixglot/output-format.hpp"
#include "pixglot/progress-token.hpp"
#include "pixglot/reader.hpp"



namespace pixglot {

image decode(reader&&, progress_access_token = {}, const output_format& = {});
image decode(reader&&, codec, progress_access_token = {}, const output_format& = {});

image decode(reader&, progress_access_token = {}, const output_format& = {});
image decode(reader&, codec, progress_access_token = {}, const output_format& = {});

}

#endif // PIXGLOT_DECODE_HPP_INCLUDED
