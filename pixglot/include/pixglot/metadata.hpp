// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_METADATA_HPP_INCLUDED
#define PIXGLOT_METADATA_HPP_INCLUDED

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>



namespace pixglot {

class metadata {
  public:
    using key_value = std::pair<std::string, std::string>;

    metadata(const metadata&);
    metadata(metadata&&) noexcept;
    metadata& operator=(const metadata&);
    metadata& operator=(metadata&&) noexcept;

    ~metadata();
    metadata();



    [[nodiscard]] bool contains(std::string_view) const;



    [[nodiscard]] std::optional<std::string_view> find(std::string_view) const;

    [[nodiscard]]       std::string& operator[](std::string_view);
    [[nodiscard]] const std::string& operator[](std::string_view) const;



    void emplace(std::string, std::string);



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

}

#endif // PIXGLOT_METADATA_HPP_INCLUDED
