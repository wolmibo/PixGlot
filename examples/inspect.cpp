#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <source_location>

#include <getopt.h>

#include <pixglot/decode.hpp>
#include <pixglot/frame.hpp>
#include <pixglot/frame-source-info.hpp>
#include <pixglot/output-format.hpp>
#include <pixglot/metadata.hpp>
#include <pixglot/pixel-format.hpp>
#include <pixglot/square-isometry.hpp>




using namespace std::string_view_literals;





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





[[nodiscard]] std::string frame_source_format_to_string(
    const pixglot::frame_source_info& fsi
) {
  std::string output{pixglot::stringify(fsi.color_model())};

  if (fsi.color_model() == pixglot::color_model::yuv) {
    output += pixglot::stringify(fsi.subsampling());
  }

  if (fsi.has_alpha()) {
    output.push_back('a');
  }

  bool uniform_color = !fsi.has_color() ||
    fsi.color_model() == pixglot::color_model::palette ||
      (fsi.color_model_format()[0] == fsi.color_model_format()[1] &&
      fsi.color_model_format()[1] == fsi.color_model_format()[2]);

  output.push_back('_');

  if (uniform_color) {
    output += pixglot::stringify(fsi.color_model_format()[0]);
  } else {
    output += pixglot::stringify(fsi.color_model_format()[0]);
    output.push_back('_');
    output += pixglot::stringify(fsi.color_model_format()[1]);
    output.push_back('_');
    output += pixglot::stringify(fsi.color_model_format()[2]);
  }

  if (fsi.has_alpha() &&
      (!uniform_color || fsi.color_model_format()[3] != fsi.color_model_format()[0])) {

    if (fsi.color_model_format()[3] == pixglot::data_source_format::index) {
      output += "_indexed α";
    } else {
      output.push_back('_');
      output += pixglot::stringify(fsi.color_model_format()[3]);
    }
  }

  return output;
}





template<typename T>
void print_meta_item(const std::string& key, T&& value, size_t width, size_t indent) {
  std::cout << std::string(indent, ' ')
    << std::left << std::setw(width + 2) << (key + ": ")
    << std::forward<T>(value) << '\n';
}





[[nodiscard]] size_t column_width(std::span<const pixglot::metadata::key_value> list) {
  size_t width = 10;

  if (!list.empty()) {
    width = std::max(width, std::ranges::max(list, {}, [](const auto& arg) {
      return arg.first.size();
    }).first.size());
  }

  return std::min<size_t>(width, 40);
}





void print_key_value(
    const pixglot::metadata::key_value& kv,
    size_t                              width,
    size_t                              indent,
    bool                                raw
) {
  if (!raw && kv.first.starts_with("pixglot.") && kv.first.ends_with(".raw")) {
    print_meta_item(kv.first, "<use --raw to include raw data>"sv, width, indent);
  } else {
    print_meta_item(kv.first, kv.second, width, indent);
  }
}





void print_frame(const pixglot::frame& f, bool raw) {
  std::cout << "  • ";

  if (auto name = f.name()) {
    std::cout << '"' << *name << "\", ";
  }

  std::cout << f.width() << "×" << f.height() << ", ";

  auto fsf = frame_source_format_to_string(f.source_info());
  auto fmt = pixglot::to_string(f.format());
  std::cout << fsf;
  if (fsf != fmt) {
    std::cout << "(→" << fmt << ")";
  }
  if (f.type() == pixglot::storage_type::pixel_buffer &&
      pixglot::byte_size(f.format().format) > 1) {
    std::cout << '(' << str(f.pixels().endian()) << ')';
  }

  std::cout << ", γ=" << f.gamma();

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

  std::cout << '\n';

  if (!f.metadata().empty()) {
    auto width = column_width(f.metadata());
    for (const auto& kv: f.metadata()) {
      print_key_value(kv, width, 4, raw);
    }
  }
}





void print_image(const pixglot::image& image, bool raw) {
  for (const auto& str: image.warnings()) {
    std::cout << "  ⚠ " << str << '\n';
  }

  auto width = column_width(image.metadata());

  print_meta_item("codec",     pixglot::to_string(image.codec()), width, 2);
  print_meta_item("mime-type", image.mime_type(), width, 2);
  print_meta_item("animated",  str(image.animated()), width, 2);
  print_meta_item("frames",    image.size(), width, 2);

  for (const auto& kv: image.metadata()) {
    print_key_value(kv, width, 2, raw);
  }

  for (const auto& f: image.frames()) {
    print_frame(f, raw);
  }


  std::cout << std::endl;
}





std::ostream& operator<<(std::ostream& out, const std::source_location& location) {
  out << location.file_name() << ':' << location.line() << ':' << location.column();
  return out;
}



void print_help(const std::filesystem::path& name) {
  std::cout <<

    "Inspect the provided file(s) using the pixglot library and return the number of\n"
    "errors encountered.\n"
    "\n"
    "Usage: " << name.filename().native() << " [options] <image1>...\n"
    "\n"
    "Available options:\n"
    "  -h, --help              show this help and exit\n"
    "  -d, --decode            fully decode the image(s)\n"
    "  -r, --raw               include raw metadata\n"

    << std::flush;
}



int main(int argc, char** argv) {
  auto args = std::span{argv, static_cast<size_t>(argc)};

  static std::array<option, 3> long_options = {
    option{"help",                 no_argument,       nullptr, 'h'},
    option{"decode",               no_argument,       nullptr, 'd'},
    option{"raw",                  no_argument,       nullptr, 'r'},
  };

  bool help  {false};
  bool decode{false};
  bool raw   {false};

  int c{-1};
  while ((c = getopt_long(args.size(), args.data(), "hdr",
                          long_options.data(), nullptr)) != -1) {
    switch (c) {
      case 'h': help   = true; break;
      case 'd': decode = true; break;
      case 'r': raw    = true; break;
    }
  }

  std::vector<std::filesystem::path> files;
  while (optind < argc) {
    files.emplace_back(args[optind++]);
  }

  if (help || files.empty()) {
    print_help(args[0]);
    return 0;
  }


  int error_count{0};

  pixglot::output_format output_format;
  if (!decode) {
    output_format.storage_type(pixglot::storage_type::no_pixels);
  }


  for (const auto& file: files) {
    std::cout << file.native() << '\n';

    try {
      auto image{pixglot::decode(pixglot::reader{file}, {}, output_format)};


      print_image(image, raw);

    } catch (const pixglot::base_exception& ex) {
      std::cerr << "  ✖ " << ex.message() << '\n';
      std::cerr << "    " << ex.location() << '\n';
      std::cerr << "    " << ex.location().function_name() << std::endl;
      error_count++;

    } catch (std::exception& ex) {
      std::cerr << "  ✖ " << ex.what() << std::endl;
      error_count++;
    }
  }

  return error_count;
}
