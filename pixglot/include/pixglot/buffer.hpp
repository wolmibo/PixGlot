// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_BUFFER_HPP_INCLUDED
#define PIXGLOT_BUFFER_HPP_INCLUDED

#include <algorithm>
#include <memory>
#include <span>



namespace pixglot {

template<typename T>
class buffer {
  public:
    buffer(buffer&&) noexcept = default;
    buffer& operator=(buffer&&) noexcept = default;

    buffer(const buffer& rhs) :
      buffer{rhs.count_}
    {
      std::copy(rhs.begin(), rhs.end(), begin());
    }



    buffer& operator=(const buffer& rhs) {
      if (&rhs == this) {
        return *this;
      }

      buffer_.reset(new T[rhs.count_]);
      count_ = rhs.count_;

      std::copy(rhs.begin(), rhs.end(), begin());

      return *this;
    }



    ~buffer() = default;



    buffer() = default;

    explicit buffer(size_t count) :
      buffer_{count > 0 ? new T[count] : nullptr},
      count_ {count}
    {}



    void clear() {
      buffer_.reset();
      count_ = 0;
    }



    [[nodiscard]] bool     empty()     const { return !static_cast<bool>(buffer_); }

    [[nodiscard]] size_t   size()      const { return count_; }
    [[nodiscard]] size_t   byte_size() const { return count_ * sizeof(T); }

    [[nodiscard]] const T* data()      const { return buffer_.get(); }
    [[nodiscard]]       T* data()            { return buffer_.get(); }



    [[nodiscard]] std::span<const std::byte> as_bytes() const {
      return std::as_bytes(std::span{buffer_.get(), count_});
    }

    [[nodiscard]] std::span<std::byte> as_bytes() {
      return std::as_writable_bytes(std::span{buffer_.get(), count_});
    }



    [[nodiscard]] const T* begin() const { return buffer_.get(); }
    [[nodiscard]] const T* end()   const { return buffer_.get() + count_; }

    [[nodiscard]]       T* begin()       { return buffer_.get(); }
    [[nodiscard]]       T* end()         { return buffer_.get() + count_; }





  private:
    std::unique_ptr<T[]> buffer_;   //NOLINT(*-avoid-c-arrays)
    size_t               count_{0};
};

}

#endif // PIXGLOT_BUFFER_HPP_INCLUDED
