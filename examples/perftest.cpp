#include <chrono>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <vector>

#include <getopt.h>

#include <pixglot/conversions.hpp>
#include <pixglot/decode.hpp>
#include <pixglot/frame.hpp>
#include <pixglot/output-format.hpp>
#include <pixglot/pixel-format.hpp>
#include <pixglot/preference.hpp>
#include <pixglot/progress-token.hpp>
#include <pixglot/square-isometry.hpp>
#include <pixglot/utils/cast.hpp>



enum class event {
  image_begin,
  frame_begin,
  frame_finish,
  image_finish,
  conversions_begin,
  conversion_storage_type,
  conversion_orientation,
  conversion_endian,
  conversion_pixel_format,
  conversions_finish,
  save_image_begin,
  save_frame_finish,
  save_image_finish,
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
        break;

      case event::conversions_begin:
        std::cout << "conversions begin\n";
        break;
      case event::conversions_finish:
        std::cout << "conversion finish\n";
        break;
      case event::conversion_endian:
        std::cout << "  ├─ convert endian\n";
        break;
      case event::conversion_orientation:
        std::cout << "  ├─ convert orientation\n";
        break;
      case event::conversion_storage_type:
        std::cout << "  ├─ convert storage_type\n";
        break;
      case event::conversion_pixel_format:
        std::cout << "  ├─ convert pixel_format\n";
        break;


      case event::save_image_begin:
        std::cout << "save image begin\n";
        break;
      case event::save_frame_finish:
        std::cout << "  ├─ save frame finish\n";
        break;
      case event::save_image_finish:
        std::cout << "save image finish\n";
        break;
    }
  }
}






void apply_conversion(pixglot::image& img, std::endian endian) {
  pixglot::convert_endian(img, endian);
  emit_event(event::conversion_endian);
}

void apply_conversion(pixglot::image& img, pixglot::square_isometry ori) {
  pixglot::convert_orientation(img, ori);
  emit_event(event::conversion_orientation);
}

void apply_conversion(pixglot::image& img, pixglot::storage_type st) {
  pixglot::convert_storage(img, st);
  emit_event(event::conversion_storage_type);
}

void apply_conversion(pixglot::image& img, pixglot::pixel_format fmt) {
  pixglot::convert_pixel_format(img, fmt);
  emit_event(event::conversion_pixel_format);
}






void save_frame(const pixglot::frame& frame, const std::filesystem::path& outpath) {
  if (frame.type() != pixglot::storage_type::pixel_buffer) {
    throw std::runtime_error{"can only output pixel_buffer"};
  }

  if (pixglot::has_alpha(frame.format().channels)) {
    throw std::runtime_error{"cannot output to ppm with alpha channel"};
  }

  if (frame.format().format == pixglot::data_format::f16 ||
      frame.format().format == pixglot::data_format::u32 ||
      (frame.format().format == pixglot::data_format::u16 &&
       frame.pixels().endian() != std::endian::big)) {
    throw std::runtime_error{"can only output data-format u8, u16(big), f32"};
  }

  char header{'4'};
  std::string max_brightness;


  switch (frame.format().format) {
    case pixglot::data_format::u8:
      header = pixglot::has_color(frame.format().channels) ? '6' : '5';
      max_brightness = "255";
      break;
    case pixglot::data_format::u16:
      header = pixglot::has_color(frame.format().channels) ? '6' : '5';
      max_brightness = "65535";
      break;
    case pixglot::data_format::f32:
      header = pixglot::has_color(frame.format().channels) ? 'F' : 'f';
      max_brightness = frame.pixels().endian() == std::endian::little ? "-1.0" : "1.0";
      break;
    default:
      throw std::runtime_error{"unsupported format for ppm output"};
  }


  std::ofstream output{outpath};
  output << 'P' << header << '\n';
  output << frame.width() << ' ' << frame.height() << '\n';
  output << max_brightness << '\n';

  for (size_t y = 0; y < frame.height(); ++y) {
    auto row = frame.pixels().row_bytes(y);

    output.write(pixglot::utils::byte_pointer_cast<const char>(row.data()), row.size());
  }


  emit_event(event::save_frame_finish);
}



[[nodiscard]] std::filesystem::path ppm_extension(pixglot::pixel_format format) {
  if (format.format == pixglot::data_format::f32) {
    return ".pfm";
  }

  if (!pixglot::has_color(format.channels)) {
    return ".pgm";
  }

  return ".ppm";
}



void save_image(const pixglot::image& img, std::filesystem::path outpath) {
  if (img.empty()) {
    return;
  }

  if (img.size() == 1) {
    save_frame(img.frame(),
        outpath.replace_extension(ppm_extension(img.frame().format())));
    return;
  }

  for (size_t i = 0; i < img.size(); ++i) {
    const auto& f = img.frame(i);
    std::stringstream output;
    output << std::setw(4) << std::setfill('0') << std::right << i <<
      ppm_extension(img.frame(i).format());

    save_frame(f, outpath.replace_extension(output.str()));
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



BEGIN_STR_TO_ENUM(pixglot, color_channels)
  STR_TO_ENTRY(gray);
  STR_TO_ENTRY(gray_a);
  STR_TO_ENTRY(rgb);
  STR_TO_ENTRY(rgba);
END_STR_TO_ENUM(color_channels)



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



[[nodiscard]] pixglot::pixel_format pixel_format_from_string(std::string_view str) {
  auto ipos = str.find('_');
  if (ipos == std::string_view::npos) {
    throw std::runtime_error{"expected pixel format"};
  }

  return pixglot::pixel_format {
    .format   = data_format_from_string(str.substr(ipos + 1)),
    .channels = color_channels_from_string(str.substr(0, ipos))
  };
}





using operations = std::variant<
  std::endian,
  pixglot::storage_type,
  pixglot::square_isometry,
  pixglot::pixel_format
>;





void print_help(const std::filesystem::path& name) {
  std::cout <<

    "Measure the time of various operations on an image file\n"
    "\n"
    "Usage: " << name.filename().native() << " [options] <image>\n"
    "\n"
    "Available options:\n"
    "  -h, --help                         show this help and exit\n"
    "\n"
    "  -o, --output=<file>                write image as ppm to <file>\n"
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
    "  --convert-target=<target>          convert to storage type\n"
    "  --convert-orientation=<ori>        convert to orientaion\n"
    "  --convert-endian=<endian>          convert to endian\n"
    "  --convert-pixel-format=<cc>_<df>   convert to pixel format\n"
    "\n"
    "Enum values:\n"
    "  <target>:   no_pixels, pixel_buffer, gl_texture\n"
    "  <ori>:      identity, flip_x, rotate_cw, rotate_half, rotate_ccw, flip_y,\n"
    "              transpose, anti_transpose\n"
    "  <am>:       none, straigh, premultiplied\n"
    "  <df>:       u8, u16, u32, f16, f32\n"
    "  <cc>:       gray, gray_a, rgb, rgba\n"
    "  <endian>:   native, little, big\n"

    << std::flush;
}





int main(int argc, char** argv) {
  auto args = std::span{argv, static_cast<size_t>(argc)};

  static std::array<option, 25> long_options = {
    option{"help",                 no_argument,       nullptr, 'h'},

    option{"output",               required_argument, nullptr, 'o'},

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

    option{"convert-target",       required_argument, nullptr, 4000},
    option{"convert-orientation",  required_argument, nullptr, 4001},
    option{"convert-endian",       required_argument, nullptr, 4002},
    option{"convert-pixel-format", required_argument, nullptr, 4003},

    option{nullptr,                0,                 nullptr, 0},
  };

  auto pref = pixglot::preference_level::prefer;

  pixglot::output_format of;

  bool help           {false};
  bool enforce        {false};
  bool standard_format{false};

  std::vector<operations> ops;

  std::optional<std::filesystem::path> output_file;

  int c{-1};
  while ((c = getopt_long(args.size(), args.data(), "ho:",
                          long_options.data(), nullptr)) != -1) {
    switch (c) {
      case 'h': help = true; break;

      case 'o': output_file.emplace(optarg); break;

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

      case 4000: ops.emplace_back(storage_type_from_string(optarg));    break;
      case 4001: ops.emplace_back(square_isometry_from_string(optarg)); break;
      case 4002: ops.emplace_back(endian_from_string(optarg));          break;
      case 4003: ops.emplace_back(pixel_format_from_string(optarg));    break;
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

    if (!ops.empty()) {
      emit_event(event::conversions_begin);
      for (const auto& op: ops) {
        std::visit([&image](auto&& arg) { apply_conversion(image, arg); }, op);
      }
      emit_event(event::conversions_finish);
    }

    if (output_file) {
      emit_event(event::save_image_begin);
      save_image(image, *output_file);
      emit_event(event::save_image_finish);
    }

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
