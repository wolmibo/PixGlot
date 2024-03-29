#include <cassert>
#include <iostream>

#include <pixglot/decode.hpp>

// NOLINTBEGIN(*-avoid-endl)
int main(int argc, char** argv) {
  assert(argc == 2);
  try {
    // NOLINTNEXTLINE(*pointer-arithmetic)
    auto img = pixglot::decode(pixglot::reader{argv[1]});

    for (const auto& warning: img.warnings()) {
      std::cerr << "Warning: " << warning << std::endl;
    }

    for (size_t i = 0; i < img.size(); ++i) {
      std::cout << "Frame #" << i << ": " << pixglot::to_string(img[i]) << std::endl;
    }
  } catch (pixglot::base_exception& ex) {
    std::cerr << "*ERROR:*" << ex.what() << std::endl;
  }
}
// NOLINTEND(*-avoid-endl)
