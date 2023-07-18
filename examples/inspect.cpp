#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>

#include <pixglot/decode.hpp>
#include <pixglot/frame.hpp>
#include <pixglot/frame-source-info.hpp>
#include <pixglot/output-format.hpp>
#include <pixglot/pixel-format.hpp>
#include <pixglot/square-isometry.hpp>





std::string_view str(bool value) {
  return value ? "yes" : "no";
}





std::string_view str(std::endian endian) {
  switch (endian) {
    case std::endian::big:    return "big";
    case std::endian::little: return "little";
    default:                  return "<unknown byteorder>";
  }
}





void print_ipi(const pixglot::frame_source_info& fsi) {
  std::cout << pixglot::stringify(fsi.color_model());

  if (fsi.color_model() == pixglot::color_model::yuv) {
    std::cout << pixglot::stringify(fsi.subsampling());
  }

  if (fsi.has_alpha()) {
    std::cout << 'a';
  }

  bool uniform_color = !fsi.has_color() ||
    fsi.color_model() == pixglot::color_model::palette ||
      (fsi.color_model_format()[0] == fsi.color_model_format()[1] &&
      fsi.color_model_format()[1] == fsi.color_model_format()[2]);

  std::cout << '_';

  if (uniform_color) {
    std::cout << pixglot::stringify(fsi.color_model_format()[0]);
  } else {
    std::cout << pixglot::stringify(fsi.color_model_format()[0]) << '_';
    std::cout << pixglot::stringify(fsi.color_model_format()[1]) << '_';
    std::cout << pixglot::stringify(fsi.color_model_format()[2]);
  }

  if (fsi.has_alpha() &&
      (!uniform_color || fsi.color_model_format()[3] != fsi.color_model_format()[0])) {

    if (fsi.color_model_format()[3] == pixglot::data_source_format::index) {
      std::cout << "_indexed α";
    } else {
      std::cout << '_' << pixglot::stringify(fsi.color_model_format()[3]);
    }
  }
}






void print_image(const pixglot::image& image) {
  for (const auto& str: image.warnings()) {
    std::cout << "  ⚠ " << str << '\n';
  }

  if (image.has_warnings()) {
    std::cout << '\n';
  }

  std::cout << "  animated:    " << str(image.animated()) << '\n';
  std::cout << "  frames:      " << image.size() << '\n';

  if (!image.empty()) {
    std::cout << '\n';
  }

  for (const auto& f: image.frames()) {

    std::cout << "  • ";

    if (auto name = f.name()) {
      std::cout << '"' << *name << "\", ";
    }

    std::cout << f.width() << "×" << f.height() << ", ";

    std::cout << pixglot::to_string(f.format());
    if (pixglot::byte_size(f.format().format) > 1) {
      if (f.type() == pixglot::storage_type::pixel_buffer) {
        std::cout << '(' << str(f.pixels().endian()) << "), ";
      } else {
        std::cout << "(native), ";
      }
    } else {
      std::cout << ", ";
    }

    std::cout << "γ=" << f.gamma();

    if (f.orientation() != pixglot::square_isometry::identity) {
      std::cout << ", " << pixglot::to_string(f.orientation());
    }

    if (pixglot::has_alpha(f.format().channels)) {
      switch (f.alpha_mode()) {
        case pixglot::alpha_mode::none:          std::cout << ", no alpha"; break;
        case pixglot::alpha_mode::straight:      std::cout << ", straight"; break;
        case pixglot::alpha_mode::premultiplied: std::cout << ", premultiplied"; break;

        default: std::cout << ", unknown alpha"; break;
      }
    }

    if (f.duration() > std::chrono::microseconds{0}) {
      std::cout << ", " << f.duration().count() << "µs";
    }

    std::cout << ", ";
    print_ipi(f.source_info());

    std::cout << '\n';
  }


  std::cout << std::flush;
}





int main(int argc, char** argv) {
  if (argc != 2) {
    //NOLINTNEXTLINE(*pointer-arithmetic)
    std::cout << "Usage: " << argv[0] << " <image>" << std::endl;
  }

  try {
    pixglot::output_format output_format;
    output_format.storage_type(pixglot::storage_type::no_pixels);
    //
    //NOLINTNEXTLINE(*pointer-arithmetic)
    auto image{pixglot::decode(pixglot::reader{argv[1]}, {}, output_format)};

    //NOLINTNEXTLINE(*pointer-arithmetic)
    std::cout << std::filesystem::path{argv[1]}.filename().native()
      << '\n' << std::endl;

    print_image(image);

  } catch (std::exception& ex) {
    std::cerr << "***Error*** " << ex.what() << std::endl;
    return 1;
  }
}
