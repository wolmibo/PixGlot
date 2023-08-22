// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_READER_HPP_INCLUDED
#define PIXGLOT_READER_HPP_INCLUDED

#include "pixglot/exception.hpp"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <span>



namespace pixglot {

class reader {
  public:
    explicit reader(const std::filesystem::path&);

    reader(const reader&) = delete;
    reader(reader&&) noexcept;

    reader& operator=(const reader&) = delete;
    reader& operator=(reader&&) noexcept;

    ~reader();



    [[nodiscard]] size_t peek(std::span<std::byte>) const;
    [[nodiscard]] size_t read(std::span<std::byte>);

    void skip(size_t);
    [[nodiscard]] bool seek(size_t);

    [[nodiscard]] bool   eof()      const;
    [[nodiscard]] size_t position() const;
    [[nodiscard]] size_t size()     const;





  private:
    class impl;
    std::unique_ptr<impl> impl_;
};

}


#endif // PIXGLOT_READER_HPP_INCLUDED
