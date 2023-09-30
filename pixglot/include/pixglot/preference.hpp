// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_PREFERENCE_HPP_INCLUDED
#define PIXGLOT_PREFERENCE_HPP_INCLUDED

#include <utility>



namespace pixglot {

enum class preference_level {
  whatever,
  prefer,
  require
};

template<typename T>
  requires std::equality_comparable<T> && std::default_initializable<T>
class preference {
  public:
    constexpr preference() = default;

    constexpr preference(
        const T&         value,
        preference_level level = preference_level::require
    ) :
      value_{value},
      level_{level}
    {}

    constexpr preference(T&& value, preference_level level = preference_level::require) :
      value_{std::move(value)},
      level_{level}
    {}



    [[nodiscard]] T&       value()       { return value_; }
    [[nodiscard]] const T& value() const { return value_; }

    [[nodiscard]] preference_level level() const { return level_; }



    [[nodiscard]] operator bool()       const { return preferred(); }

    [[nodiscard]] bool preferred() const {
      return level_ != preference_level::whatever;
    }

    [[nodiscard]] bool required() const {
      return level_ == preference_level::require;
    }



    [[nodiscard]] T*       operator->()       { return &value_; }
    [[nodiscard]] const T* operator->() const { return &value_; }

    [[nodiscard]] T&       operator*()        { return value_; }
    [[nodiscard]] const T& operator*() const  { return value_; }



    [[nodiscard]] bool prefers(const T& other) const {
      return preferred() && value_ == other;
    }

    [[nodiscard]] bool require(const T& other) const {
      return level_ == preference_level::require && value_ == other;
    }



    [[nodiscard]] bool satisfied_by(const T& other) const {
      return level_ != preference_level::require || value_ == other;
    }

    [[nodiscard]] bool preference_satisfied_by(const T& other) const {
      return level_ == preference_level::whatever || value_ == other;
    }



    void enforce() {
      if (level_ == preference_level::prefer) {
        level_ = preference_level::require;
      }
    }



  private:
    T                value_{};
    preference_level level_{preference_level::whatever};
};

}

#endif // PIXGLOT_PREFERENCE_HPP_INCLUDED
