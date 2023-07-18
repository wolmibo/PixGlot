#include "pixglot/progress-token.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

#include <pixglot/decode.hpp>
#include <pixglot/frame.hpp>



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
    "  -h, --help              show this help and exit\n"

    << std::flush;
}



std::ostream& operator<<(std::ostream& out, const std::source_location& location) {
  out << location.file_name() << ':' << location.line() << ':' << location.column();
  return out;
}


int main(int argc, char** argv) {
  auto args = std::span{argv, static_cast<size_t>(argc)};

  std::optional<std::filesystem::path> file;

  bool help = (args.size() == 1);

  for (std::string_view arg: args.subspan(1)) {
    if (arg == "-h" || arg == "--help") {
      help = true;
      continue;
    }

    file.emplace(arg);
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
    auto image{pixglot::decode(reader, std::move(at))};
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
