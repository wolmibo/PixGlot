// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_METADATA_HPP_INCLUDED
#define PIXGLOT_METADATA_HPP_INCLUDED

#include "pixglot/square-isometry.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>



namespace pixglot {

class metadata {
  public:
    class key_value {
      public:
        [[nodiscard]] std::string_view key()   const { return key_;   }
        [[nodiscard]] std::string_view value() const { return value_; }

        key_value(std::string key, std::string value) :
          key_  {std::move(key)},
          value_{std::move(value)}
        {}

      private:
        std::string key_;
        std::string value_;
    };



    metadata(const metadata&);
    metadata(metadata&&) noexcept;
    metadata& operator=(const metadata&);
    metadata& operator=(metadata&&) noexcept;

    ~metadata();
    metadata();



    [[nodiscard]] bool contains(std::string_view) const;



    [[nodiscard]] std::optional<std::string_view> find(std::string_view) const;



    void emplace(std::string, std::string);
    void append_move(std::span<key_value>);
    void append_move(std::vector<key_value>);



    [[nodiscard]] size_t size()  const;
    [[nodiscard]] bool   empty() const;

    [[nodiscard]] const key_value* begin() const;
    [[nodiscard]] const key_value* end()   const;

    [[nodiscard]] key_value* begin();
    [[nodiscard]] key_value* end();



  private:
    class impl;
    std::unique_ptr<impl> impl_;
};



[[nodiscard]] std::optional<square_isometry> orientation_from_metadata(const metadata&);

}

#endif // PIXGLOT_METADATA_HPP_INCLUDED
