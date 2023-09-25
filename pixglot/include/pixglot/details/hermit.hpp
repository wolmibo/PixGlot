// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_DETAILS_HERMIT_HPP_INCLUDED
#define PIXGLOT_DETAILS_HERMIT_HPP_INCLUDED

namespace pixglot::details {

class hermit {
  public:
    hermit() = default;

    hermit(const hermit&) = delete;
    hermit(hermit&&)      = delete;

    hermit& operator=(const hermit&) = delete;
    hermit& operator=(hermit&&)      = delete;

    ~hermit() = default;
};

}

#endif // PIXGLOT_DETAILS_HERMIT_HPP_INCLUDED
