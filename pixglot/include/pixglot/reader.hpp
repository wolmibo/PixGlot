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
    explicit reader(const std::filesystem::path& p) :
      fptr_{fopen(p.string().c_str(), "rb"), &fclose}
    {
      if (!fptr_) {
        throw no_stream_access{p.string()};
      }
    }



    [[nodiscard]] size_t read(std::span<std::byte> buffer) {
      if (fptr_) {
        return fread(buffer.data(), sizeof(std::byte), buffer.size(), fptr_.get());
      }
      return 0;
    }



    [[nodiscard]] size_t read(std::span<std::byte> buffer, size_t offset) {
      if (fptr_ && fseek(fptr_.get(), offset, SEEK_SET) == 0) {
        return fread(buffer.data(), sizeof(std::byte), buffer.size(), fptr_.get());
      }
      return 0;
    }



    [[nodiscard]] size_t peek(std::span<std::byte> buffer) const {
      if (fptr_) {
        size_t current = ftell(fptr_.get());
        size_t count
                 = fread(buffer.data(), sizeof(std::byte), buffer.size(), fptr_.get());
        fseek(fptr_.get(), current, SEEK_SET);
        return count;
      }
      return 0;
    }



    void skip(size_t count) {
      if (fptr_) {
        fseek(fptr_.get(), count, SEEK_CUR);
      }
    }



    [[nodiscard]] size_t position() const {
      return ftell(fptr_.get());
    }



    [[nodiscard]] size_t size() const {
      size_t current = ftell(fptr_.get());
      fseek(fptr_.get(), 0, SEEK_END);
      size_t size = ftell(fptr_.get());
      fseek(fptr_.get(), current, SEEK_SET);
      return size;
    }



    void seek(size_t pos) {
      if (fptr_) {
        fseek(fptr_.get(), pos, SEEK_SET);
      }
    }



    [[nodiscard]] bool eof() const {
      return fptr_ &&
        feof(fptr_.get()) != 0;
    }



  private:
    std::unique_ptr<FILE, decltype(&fclose)> fptr_;
};

}


#endif // PIXGLOT_READER_HPP_INCLUDED
