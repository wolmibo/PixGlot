#include <string>
#include <vector>

#include <pixglot/pixel-format.hpp>

using namespace pixglot;



struct new_type{
  auto operator<=>(const new_type&) const = default;
};

static_assert(data_format_type<u8>);
static_assert(data_format_type<u16>);
static_assert(data_format_type<u32>);
static_assert(data_format_type<f16>);
static_assert(data_format_type<f32>);

static_assert(!data_format_type<new_type>);
static_assert(!data_format_type<std::string>);
static_assert(!pixel_type<new_type>);
static_assert(!pixel_type<std::string>);

static_assert(pixel_type<gray<u8>>);

template<typename T>
struct concept_tester {
  static constexpr auto test() -> bool {
    static_assert(data_format_type<T>);

    static_assert(pixel_type<gray<T>>);
    static_assert(pixel_type<gray_a<T>>);
    static_assert(pixel_type<rgb<T>>);
    static_assert(pixel_type<rgba<T>>);
    static_assert(!pixel_type<std::vector<T>>);

    return true;
  }
};

static_assert(concept_tester<u8>::test());
static_assert(concept_tester<u16>::test());
static_assert(concept_tester<u32>::test());
static_assert(concept_tester<f16>::test());
static_assert(concept_tester<f32>::test());



int main() {}
