// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_PIXEL_BUFFER_HPP_INCLUDED
#define PIXGLOT_PIXEL_BUFFER_HPP_INCLUDED

#include "pixglot/buffer.hpp"
#include "pixglot/exception.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/utils/cast.hpp"

#include <string>



namespace pixglot {

class pixel_buffer {
  public:
    static constexpr size_t alignment = 32;

    [[nodiscard]] static constexpr size_t padding() {
      return std::max<size_t>(4, alignment);
    }

    [[nodiscard]] static constexpr size_t stride_for_width(
        size_t       width,
        pixel_format format
    ) {
      auto req = format.size() * width;
      if (auto diff = req % padding(); diff != 0) {
        return req + padding() - diff;
      }
      return req;
    }





    pixel_buffer(
        size_t       width,
        size_t       height,
        pixel_format format = {},
        std::endian  endian = std::endian::native
    ) :
      width_ {width},
      height_{height},
      format_{format},
      endian_{endian},

      buffer_{height_ * stride_for_width(width_, format_) / sizeof(aligner)}
    {}




    [[nodiscard]] std::span<const std::byte> data()   const { return buffer_.as_bytes(); }
    [[nodiscard]] std::span<std::byte>       data()         { return buffer_.as_bytes(); }

    [[nodiscard]] bool                       empty()  const { return buffer_.empty(); }
    [[nodiscard]] operator                   bool()   const { return !empty();        }

    [[nodiscard]] pixel_format               format() const { return format_; }
    [[nodiscard]] size_t                     width()  const { return width_;  }
    [[nodiscard]] size_t                     height() const { return height_; }
    [[nodiscard]] std::endian                endian() const { return endian_; }



    [[nodiscard]] size_t stride() const {
      if (height() != 0) {
        return buffer_.byte_size() / height();
      }
      return 0;
    }



    void endian(std::endian endian) { endian_ = endian; }





    template<pixel_type P>
    [[nodiscard]] std::span<P> row(size_t index) {
      if (index >= height()) {
        throw index_out_of_range{index, height()};
      }
      if (P::format() != format()) {
        throw bad_pixel_format{P::format(), format()};
      }
      return utils::interpret_as_n_unchecked<P>(
          data().subspan(index * stride()), width());
    }



    template<pixel_type P>
    [[nodiscard]] std::span<const P> row(size_t index) const {
      if (index >= height()) {
        throw index_out_of_range{index, height()};
      }
      if (P::format() != format()) {
        throw bad_pixel_format{P::format(), format()};
      }
      return utils::interpret_as_n_unchecked<const P>(
          data().subspan(index * stride()), width());
    }






    [[nodiscard]] std::span<std::byte> row_bytes(size_t index) {
      if (index >= height()) {
        throw index_out_of_range{index, height()};
      }
      return data().subspan(index * stride(), width() * format().size());
    }



    [[nodiscard]] std::span<const std::byte> row_bytes(size_t index) const {
      if (index >= height()) {
        throw index_out_of_range{index, height()};
      }
      return data().subspan(index * stride(), width() * format().size());
    }





    template<pixel_type P>
    class row_iterator {
      friend pixel_buffer;

      public:
        [[nodiscard]] row_iterator begin() {
          return *this;
        }

        [[nodiscard]] row_iterator end() {
          return row_iterator{std::span{data.end(), 0}, stride, width};
        }



        row_iterator& operator++() {
          if (!data.empty()) {
            data = data.subspan(stride);
          }

          return *this;
        }



        [[nodiscard]] bool operator==(const row_iterator& rhs) const {
          return data.size() == rhs.data.size()
            && data.data() == rhs.data.data()
            && stride == rhs.stride
            && width  == rhs.width;
        }



        [[nodiscard]] std::span<P> operator*() {
          return utils::interpret_as_n_unchecked<P>(data, width);
        }



      private:
        std::span<std::byte> data;
        size_t               stride;
        size_t               width;



        row_iterator(std::span<std::byte> d, size_t str, size_t wid) :
          data{d}, stride{str}, width{wid}
        {}
    };



    template<pixel_type P>
    [[nodiscard]] row_iterator<P> rows() {
      if (P::format() != format()) {
        throw bad_pixel_format{P::format(), format()};
      }
      return row_iterator<P>{data(), stride(), width()};
    }



  private:
    struct alignas(alignment) aligner{};
    size_t                    width_ {0};
    size_t                    height_{0};
    pixel_format              format_{};

    std::endian               endian_{std::endian::native};

    buffer<aligner>           buffer_;
};

[[nodiscard]] std::string to_string(const pixel_buffer&);

}

#endif // PIXGLOT_PIXEL_BUFFER_HPP_INCLUDED
