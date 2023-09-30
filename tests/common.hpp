#ifndef PIXGLOT_TESTS_COMMON_HPP_INCLUDED
#define PIXGLOT_TESTS_COMMON_HPP_INCLUDED

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <span>
#include <sstream>
#include <string_view>



inline std::ostream& operator<<(std::ostream& out, const std::source_location& loc) {
  return out << loc.file_name() << ":" << loc.line() << ":" << loc.column() << ":"
    << " in `" << loc.function_name() << "`";
}



[[nodiscard]] inline bool operator==(
    std::span<const std::byte> lhs,
    std::span<const std::byte> rhs
) {
  return std::ranges::equal(lhs, rhs);
}



inline std::ostream& operator<<(std::ostream& out, std::span<const std::byte> list) {
  std::stringstream buffer;
  for (auto b: list) {
    buffer << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(b)
      << ' ';
  }
  return out << buffer.str();
}





inline void id_assert(
  bool                       v,
  std::string_view           err_msg  = {},
  const std::source_location location = std::source_location::current()
) {
  if (!v) {
    std::cout << location << " id_assert failed";

    if (!err_msg.empty()) {
      std::cout << ":\n  " << err_msg;
    }

    std::cout << '\n' << std::flush;

    std::exit(1);
  }
}





template<typename T>
concept can_be_written_to_ostream = requires(const T& a, std::ostream out) {
  { out << a } -> std::same_as<std::ostream&>;
};



template<typename T, typename U>
void id_assert_eq(
  T v1, U v2,
  std::string_view           err_msg  = {},
  const std::source_location location = std::source_location::current()
) {
  if (!(v1 == v2)) {
    if constexpr (can_be_written_to_ostream<T> && can_be_written_to_ostream<U>) {
      std::cout << location << " id_assert_eq failed !(" << v1 << " == " << v2 << ")";
    } else {
      std::cout << location << " id_assert_eq failed";
    }

    if (!err_msg.empty()) {
      std::cout << ":\n  " << err_msg;
    }

    std::cout << '\n' << std::flush;

    std::exit(1);
  }
}


#endif // PIXGLOT_TESTS_COMMON_HPP_INCLUDED
