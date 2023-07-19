#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

#include <getopt.h>

#include <pixglot/decode.hpp>
#include <pixglot/frame.hpp>
#include <pixglot/output-format.hpp>
#include <pixglot/preference.hpp>
#include <pixglot/progress-token.hpp>



enum class event {
  image_begin,
  frame_begin,
  frame_finish,
  image_finish
};


using time_point = std::chrono::high_resolution_clock::time_point;



std::vector<std::pair<event, time_point>> times;

void emit_event(event ev) {
  times.emplace_back(ev, std::chrono::high_resolution_clock::now());
}





[[nodiscard]] std::chrono::microseconds diff(time_point first, time_point second) {
  return std::chrono::duration_cast<std::chrono::microseconds>(second - first);
}



[[nodiscard]] size_t format_width(time_point first, time_point second) {
  return std::to_string(diff(first, second).count()).size();
}



void write(std::ostream& out, const std::chrono::microseconds mus) {
  out << mus.count() << "µs";
}



void print_time_tree(std::span<const pixglot::frame> frames) {
  if (times.empty()) {
    return;
  }

  size_t col1 = format_width(times.front().second, times.back().second);
  size_t col2{};

  for (size_t i = 1; i < times.size(); ++i) {
    if (auto w = format_width(times[i-1].second, times[i].second); w > col2) {
      col2 = w;
    }
  }


  time_point last = times.front().second;
  size_t frame_index{0};
  for (const auto& [ev, tp]: times) {
    std::cout << std::setw(col1) << std::right;
    write(std::cout, diff(times.front().second, tp));
    std::cout << "  (+" << std::setw(col2) << std::right;
    write(std::cout, diff(last, tp));
    std::cout << ")  ";

    last = tp;

    switch (ev) {
      case event::image_begin:
        std::cout << "image begin\n";
        break;
      case event::image_finish:
        std::cout << "image finish\n";
        break;
      case event::frame_begin:
        std::cout << "  │  frame #" << frame_index << " begin\n";
        break;
      case event::frame_finish: {
        if (frame_index >= frames.size()) {
          throw std::runtime_error{"invalid timing info"};
        }

        const auto& f = frames[frame_index++];

        std::cout << "  ├─ frame #" << frame_index << " finish ("
          << f.width() << "×" << f.height() << ' ' << pixglot::to_string(f.format())
          << ")\n";
        }
    }
  }
}





void print_help(const std::filesystem::path& name) {
  std::cout <<

    "Measure the time of various operations on an image file\n"
    "\n"
    "Usage: " << name.filename().native() << " [options] <image>\n"
    "\n"
    "Available options:\n"
    "  -h, --help                             show this help and exit\n"
    "\n"
    "  -t, --prefer-target=<target>           set the preferred output target\n"
    "  -T, --target=<target>                  set the output target\n"
    "\n"
    "Enum values:\n"
    "  <target>: no-pixels, pixel-buffer, gl-texture\n"

    << std::flush;
}



std::ostream& operator<<(std::ostream& out, const std::source_location& location) {
  out << location.file_name() << ':' << location.line() << ':' << location.column();
  return out;
}



[[nodiscard]] pixglot::storage_type storage_type_from_string(std::string_view str) {
  if (str == "no-pixels"   ) return pixglot::storage_type::no_pixels;
  if (str == "pixel-buffer") return pixglot::storage_type::pixel_buffer;
  if (str == "gl-texture"  ) return pixglot::storage_type::gl_texture;

  throw std::runtime_error{"unknown storage type " + std::string{str}};
}



int main(int argc, char** argv) {
  auto args = std::span{argv, static_cast<size_t>(argc)};

  bool help{false};
  pixglot::output_format output_format;

  static std::array<option, 5> long_options = {
    option{"help",          no_argument,       nullptr, 'h'},
    option{"prefer-target", required_argument, nullptr, 't'},
    option{"target",        required_argument, nullptr, 'T'},
    option{nullptr,         0,                 nullptr, 0},
  };

  auto pref = pixglot::preference_level::prefer;

  int c{-1};
  while ((c = getopt_long(args.size(), args.data(), "ht:T:",
                          long_options.data(), nullptr)) != -1) {
    switch (c) {
      case 'h':
        help = true;
        break;
      case 't':
        if (optarg == nullptr) throw std::runtime_error{"missing argument"};
        output_format.storage_type(storage_type_from_string(optarg));
        break;
      case 'T':
        if (optarg == nullptr) throw std::runtime_error{"missing argument"};
        output_format.storage_type({storage_type_from_string(optarg), pref});
        break;
    }
  }

  std::optional<std::filesystem::path> file;
  if (optind < argc) {
    file.emplace(args[optind++]);
  }

  if (help || !file) {
    print_help(args[0]);
    return 0;
  }

  try {
    pixglot::progress_token tok;
    tok.frame_callback      ([](auto&&) { emit_event(event::frame_finish);   });
    tok.frame_begin_callback([](auto&&) { emit_event(event::frame_begin); });

    pixglot::reader reader{*file};
    auto at = tok.access_token();

    emit_event(event::image_begin);
    auto image{pixglot::decode(reader, std::move(at), output_format)};
    emit_event(event::image_finish);

    for (const auto& warn: image.warnings()) {
      std::cout << "⚠ " << warn << '\n';
    }

    print_time_tree(image.frames());

    std::cout << std::flush;

  } catch (const pixglot::base_exception& ex) {
    std::cout << "✖ " << ex.message() << '\n';
    std::cout << "  " << ex.location() << '\n';
    std::cout << "  " << ex.location().function_name() << std::endl;
    return 1;

  } catch (std::exception& ex) {
    std::cout << "✖ " << ex.what() << std::endl;
    return 1;
  }
}
