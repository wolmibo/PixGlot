// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_UTILS_CAST_HPP_INCLUDED
#define PIXGLOT_UTILS_CAST_HPP_INCLUDED

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <typeinfo>



namespace pixglot::utils {

template<typename T>
struct const_qualified_byte { using byte = std::byte; };

template<typename T> requires (std::is_const_v<T>)
struct const_qualified_byte<T> { using byte = const std::byte;};




template<typename T> requires (sizeof(T) == 1)
[[nodiscard]] T* byte_pointer_cast(typename const_qualified_byte<T>::byte* ptr) {
  return reinterpret_cast<T*>(ptr); // NOLINT(*reinterpret-cast)
}



template<typename T>
[[nodiscard]] bool is_aligned_for(const std::byte* ptr) {
  //NOLINTNEXTLINE(*reinterpret-cast)
  return reinterpret_cast<std::uintptr_t>(ptr) % alignof(T) == 0;
}






template<typename T>
[[nodiscard]] std::span<T> interpret_as_n_unchecked(
    std::span<typename const_qualified_byte<T>::byte> input,
    size_t                                            count
) {
  if (!is_aligned_for<T>(input.data())) {
    throw std::bad_cast{};
  }

  //NOLINTNEXTLINE(*reinterpret-cast)
  return {reinterpret_cast<T*>(input.data()), count};
}



template<typename T>
std::span<T> interpret_as(std::span<typename const_qualified_byte<T>::byte> input) {
  if (input.size() % sizeof(T) != 0) {
    throw std::bad_cast{};
  }
  return interpret_as_n_unchecked<T>(input, input.size() / sizeof(T));
}



template<typename T>
std::span<T> interpret_as_greedy(
    std::span<typename const_qualified_byte<T>::byte> input
) {
  return interpret_as_n_unchecked<T>(input, input.size() / sizeof(T));
}



template<typename T>
std::span<T> interpret_as_n(
    std::span<typename const_qualified_byte<T>::byte> input,
    size_t                                            count
) {
  if (input.size() < count * sizeof(T)) {
    throw std::out_of_range{"interpret_as_n: more elements requested than in input"};
  }
  return interpret_as_n_unchecked<T>(input, count);
}



template<typename T>
std::span<const T> interpret_as_n(std::span<const std::byte> input, size_t count) {
  if (input.size() < count * sizeof(T)) {
    throw std::out_of_range{"interpret_as_n: more elements requested than in input"};
  }

  //NOLINTNEXTLINE(*reinterpret-cast)
  return {reinterpret_cast<const T*>(input.data()), count};
}


}

#endif // PIXGLOT_UTILS_CAST_HPP_INCLUDED
