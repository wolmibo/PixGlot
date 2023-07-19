#include "pixglot/square-isometry.hpp"
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





std::ostream& operator<<(std::ostream& out, const std::source_location& location) {
  out << location.file_name() << ':' << location.line() << ':' << location.column();
  return out;
}





//NOLINTBEGIN(*-macro-usage)
#define STR_TO_ENTRY(x) if ( str == #x ) return x;
#define BEGIN_STR_TO_ENUM(x, y) \
  [[nodiscard]] x::y y##_from_string(std::string_view str) {\
    using enum x::y;
#define END_STR_TO_ENUM(enum) \
    throw std::runtime_error{"unknown " #enum ": " + std::string{str}}; \
  }
//NOLINTEND(*-macro-usage)



BEGIN_STR_TO_ENUM(pixglot, storage_type)
  STR_TO_ENTRY(no_pixels);
  STR_TO_ENTRY(pixel_buffer);
  STR_TO_ENTRY(gl_texture);
END_STR_TO_ENUM(storage_type)



BEGIN_STR_TO_ENUM(pixglot, square_isometry)
  STR_TO_ENTRY(identity);
  STR_TO_ENTRY(flip_x);
  STR_TO_ENTRY(rotate_cw);
  STR_TO_ENTRY(rotate_half);
  STR_TO_ENTRY(rotate_ccw);
  STR_TO_ENTRY(flip_y);
  STR_TO_ENTRY(transpose);
  STR_TO_ENTRY(anti_transpose);
END_STR_TO_ENUM(square_isometry)



BEGIN_STR_TO_ENUM(pixglot, alpha_mode)
  STR_TO_ENTRY(none);
  STR_TO_ENTRY(straight);
  STR_TO_ENTRY(premultiplied);
END_STR_TO_ENUM(alpha_mode);



BEGIN_STR_TO_ENUM(pixglot, data_format)
  STR_TO_ENTRY(u8);
  STR_TO_ENTRY(u16);
  STR_TO_ENTRY(u32);
  STR_TO_ENTRY(f16);
  STR_TO_ENTRY(f32);
END_STR_TO_ENUM(data_format)



BEGIN_STR_TO_ENUM(std, endian)
  STR_TO_ENTRY(native);
  STR_TO_ENTRY(little);
  STR_TO_ENTRY(big);
END_STR_TO_ENUM(endian)



[[nodiscard]] float float_from_string(std::string_view str) {
  if (str.empty()) {
    throw std::runtime_error{"expected float"};
  }

  float out{1.f};
  auto res = std::from_chars(str.begin(), str.end(), out);
  if (res.ec == std::errc::invalid_argument || res.ec == std::errc::result_out_of_range) {
    throw std::runtime_error{"invalid float format: " + std::string{str}};
  }
  return out;
}






void print_help(const std::filesystem::path& name) {
  std::cout <<

    "Measure the time of various operations on an image file\n"
    "\n"
    "Usage: " << name.filename().native() << " [options] <image>\n"
    "\n"
    "Available options:\n"
    "  -h, --help                         show this help and exit\n"
    "\n"
    "  --target=<target>                  set the target\n"
    "  --orientation=<ori>                set the orientation\n"
    "  --alpha-mode=<am>                  set the alpha-mode\n"
    "  --data-format=<df>                 set the data-format\n"
    "  --endian=<endian>                  set the endian\n"
    "  --gamma=<gamma>                    set the gamma\n"
    "  --fill-alpha                       fill missing alpha channel\n"
    "  --expand-gray                      expand gray to rgb\n"
    "\n"
    "  --prefer-target=<target>           set the preferred target\n"
    "  --prefer-orientation=<ori>         set the preferred orientation\n"
    "  --prefer-alpha-mode=<am>           set the preferred alpha-mode\n"
    "  --prefer-data-format=<df>          set the preferred data-format\n"
    "  --prefer-endian=<endian>           set the preferred endian\n"
    "  --prefer-gamma=<gamma>             set the preferred gamma\n"
    "  --prefer-fill-alpha                prefer to fill missing alpha channel\n"
    "  --prefer-expand-gray               prefer to expand gray to rgb\n"
    "\n"
    "  --enforce                          enforce the requested format\n"
    "  --standard-format                  standard format (rgba_u8, all trafos applied)\n"
    "\n"
    "Enum values:\n"
    "  <target>:   no_pixels, pixel_buffer, gl_texture\n"
    "  <ori>:      identity, flip_x, rotate_cw, rotate_half, rotate_ccw, flip_y,\n"
    "              transpose, anti_transpose\n"
    "  <am>:       none, straigh, premultiplied\n"
    "  <df>:       u8, u16, u32, f16, f32\n"
    "  <endian>:   native, little, big\n"

    << std::flush;
}





int main(int argc, char** argv) {
  auto args = std::span{argv, static_cast<size_t>(argc)};

  static std::array<option, 20> long_options = {
    option{"help",                 no_argument,       nullptr, 'h'},

    option{"target",               required_argument, nullptr, 1000},
    option{"orientation",          required_argument, nullptr, 1001},
    option{"alpha-mode",           required_argument, nullptr, 1002},
    option{"data-format",          required_argument, nullptr, 1003},
    option{"endian",               required_argument, nullptr, 1004},
    option{"gamma",                required_argument, nullptr, 1005},
    option{"fill-alpha",           no_argument,       nullptr, 1006},
    option{"expand-gray",          no_argument,       nullptr, 1007},

    option{"prefer-target",        required_argument, nullptr, 2000},
    option{"prefer-orientation",   required_argument, nullptr, 2001},
    option{"prefer-alpha-mode",    required_argument, nullptr, 2002},
    option{"prefer-data-format",   required_argument, nullptr, 2003},
    option{"prefer-endian",        required_argument, nullptr, 2004},
    option{"prefer-gamma",         required_argument, nullptr, 2005},
    option{"prefer-fill-alpha",    no_argument,       nullptr, 2006},
    option{"prefer-expand-gray",   no_argument,       nullptr, 2007},

    option{"enforce",              no_argument,       nullptr, 3001},
    option{"standard-format",      no_argument,       nullptr, 3002},
    option{nullptr,                0,                 nullptr, 0},
  };

  auto pref = pixglot::preference_level::prefer;

  pixglot::output_format of;

  bool help           {false};
  bool enforce        {false};
  bool standard_format{false};

  int c{-1};
  while ((c = getopt_long(args.size(), args.data(), "h",
                          long_options.data(), nullptr)) != -1) {
    switch (c) {
      case 'h': help = true; break;

      case 1000: of.storage_type(storage_type_from_string(optarg));    break;
      case 1001: of.orientation (square_isometry_from_string(optarg)); break;
      case 1002: of.alpha_mode  (alpha_mode_from_string(optarg));      break;
      case 1003: of.data_format (data_format_from_string(optarg));     break;
      case 1004: of.endian      (endian_from_string(optarg));          break;
      case 1005: of.gamma       (float_from_string(optarg));           break;
      case 1006: of.fill_alpha        (true); break;
      case 1007: of.expand_gray_to_rgb(true); break;

      case 2000: of.storage_type({storage_type_from_string(optarg), pref});    break;
      case 2001: of.orientation ({square_isometry_from_string(optarg), pref}); break;
      case 2002: of.alpha_mode  ({alpha_mode_from_string(optarg), pref});      break;
      case 2003: of.data_format ({data_format_from_string(optarg), pref});     break;
      case 2004: of.endian      ({endian_from_string(optarg), pref});          break;
      case 2005: of.gamma       ({float_from_string(optarg), pref});           break;
      case 2006: of.fill_alpha        ({true, pref}); break;
      case 2007: of.expand_gray_to_rgb({true, pref}); break;

      case 3001: enforce         = true; break;
      case 3002: standard_format = true; break;
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

  if (standard_format) {
    of = pixglot::output_format::standard();
  }

  if (enforce) {
    of.enforce();
  }



  try {
    pixglot::progress_token tok;
    tok.frame_callback      ([](auto&&) { emit_event(event::frame_finish);   });
    tok.frame_begin_callback([](auto&&) { emit_event(event::frame_begin); });

    pixglot::reader reader{*file};
    auto at = tok.access_token();

    emit_event(event::image_begin);
    auto image{pixglot::decode(reader, std::move(at), of)};
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
