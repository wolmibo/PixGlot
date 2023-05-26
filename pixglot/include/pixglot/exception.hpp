// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_EXCEPTION_HPP_INCLUDED
#define PIXGLOT_EXCEPTION_HPP_INCLUDED

#include "pixglot/codecs.hpp"
#include "pixglot/pixel-format.hpp"

#include <optional>
#include <source_location>
#include <string>
#include <string_view>



namespace pixglot {

class base_exception : public std::exception {
  public:
    base_exception(const base_exception&) = default;
    base_exception(base_exception&&)      = default;
    base_exception& operator=(const base_exception&) = default;
    base_exception& operator=(base_exception&&)      = default;

    ~base_exception() override = default;



    explicit base_exception(
      const std::string&          what,
      const std::string&          additional = "",
      const std::source_location& location   = std::source_location::current()
    ) :
      what_    {what},
      message_ {std::string{"`"} + location.function_name() + "` "
                + what + ": " + additional},
      location_{location}
    {}



    [[nodiscard]] const char* what() const noexcept override {
      return what_.c_str();
    }

    [[nodiscard]] std::string_view            message()  const { return message_;  }
    [[nodiscard]] const std::source_location& location() const { return location_; }

    [[nodiscard]] virtual std::unique_ptr<base_exception> make_unique() {
      return std::make_unique<base_exception>(std::move(*this));
    }



   private:
     std::string          what_;
     std::string          message_;
     std::source_location location_;
};





class index_out_of_range : public base_exception {
  public:
    index_out_of_range(const index_out_of_range&) = default;
    index_out_of_range(index_out_of_range&&)      = default;
    index_out_of_range& operator=(const index_out_of_range&) = default;
    index_out_of_range& operator=(index_out_of_range&&)      = default;

    ~index_out_of_range() override = default;



    index_out_of_range(
        size_t                      index,
        size_t                      bound,
        const std::source_location& location = std::source_location::current()
    ) :
      base_exception{
        "index out of range",
        std::to_string(index) + " >= " + std::to_string(bound),
        location
      },
      index_{index},
      bound_{bound}
    {}



    [[nodiscard]] size_t index() const { return index_; }
    [[nodiscard]] size_t bound() const { return bound_; }

    [[nodiscard]] std::unique_ptr<base_exception> make_unique() override {
      return std::make_unique<index_out_of_range>(std::move(*this));
    }



  private:
    size_t index_;
    size_t bound_;
};





class bad_pixel_format : public base_exception {
  public:
    bad_pixel_format(const bad_pixel_format&) = default;
    bad_pixel_format(bad_pixel_format&&)      = default;
    bad_pixel_format& operator=(const bad_pixel_format&) = default;
    bad_pixel_format& operator=(bad_pixel_format&&)      = default;

    ~bad_pixel_format() override = default;



    explicit bad_pixel_format(
      pixel_format                got,
      std::optional<pixel_format> expected = {},
      const std::source_location& location = std::source_location::current()
    ) :
      base_exception{
        "bad pixel format",
        "got " + to_string(got) + (expected ? "; expected " + to_string(*expected) : ""),
        location
      },
      got_     {got},
      expected_{expected}
    {}



    [[nodiscard]] pixel_format                got()      const { return got_; }
    [[nodiscard]] std::optional<pixel_format> expected() const { return expected_; }

    [[nodiscard]] std::unique_ptr<base_exception> make_unique() override {
      return std::make_unique<bad_pixel_format>(std::move(*this));
    }



  private:
    pixel_format                got_;
    std::optional<pixel_format> expected_;
};





class no_stream_access : public base_exception {
  public:
    no_stream_access(const no_stream_access&) = default;
    no_stream_access(no_stream_access&&)      = default;
    no_stream_access& operator=(const no_stream_access&) = default;
    no_stream_access& operator=(no_stream_access&&)      = default;

    ~no_stream_access() override = default;



    explicit no_stream_access(
      const std::string&          stream_name,
      const std::source_location& location = std::source_location::current()
    ) :
      base_exception{"no stream access", "cannot access stream " + stream_name, location},
      stream_name_  {stream_name}
    {}



    [[nodiscard]] std::string_view stream_name() const { return stream_name_; }

    [[nodiscard]] std::unique_ptr<base_exception> make_unique() override {
      return std::make_unique<no_stream_access>(std::move(*this));
    }



  private:
    std::string stream_name_;
};





class no_decoder : public base_exception {
  public:
    no_decoder(const no_decoder&) = default;
    no_decoder(no_decoder&&)      = default;
    no_decoder& operator=(const no_decoder&) = default;
    no_decoder& operator=(no_decoder&&)      = default;

    ~no_decoder() override = default;



    explicit no_decoder(
      const std::source_location& location = std::source_location::current()
    ) :
      base_exception{
        "no decoder",
        "no decoder found for input data",
        location
      }
    {}



    [[nodiscard]] std::unique_ptr<base_exception> make_unique() override {
      return std::make_unique<no_decoder>(std::move(*this));
    }
};





class decode_error : public base_exception {
  public:
    decode_error(const decode_error&) = default;
    decode_error(decode_error&&)      = default;
    decode_error& operator=(const decode_error&) = default;
    decode_error& operator=(decode_error&&)      = default;

    ~decode_error() override = default;



    explicit decode_error(
        codec                       c,
        const std::string&          message  = {},
        const std::source_location& location = std::source_location::current()
    ) :
      base_exception{"cannot decode " + to_string(c), message, location},
      codec_        {c},
      message_      {message}
    {}



    [[nodiscard]] codec            decoder() const { return codec_; }
    [[nodiscard]] std::string_view plain()   const { return message_; }

    [[nodiscard]] std::unique_ptr<base_exception> make_unique() override {
      return std::make_unique<decode_error>(std::move(*this));
    }



  private:
    codec       codec_;
    std::string message_;
};

}

#endif // PIXGLOT_EXCEPTION_HPP_INCLUDED
