#include "pixglot/input-plane-info.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <thread>

#include <pixglot/decode.hpp>
#include <pixglot/frame.hpp>
#include <pixglot/pixel-format.hpp>
#include <pixglot/square-isometry.hpp>

using namespace std::chrono;



std::atomic<size_t>         frame_counter{0};
std::vector<microseconds>   frame_times;
std::optional<microseconds> header_time;
steady_clock::time_point    timestamp;



void print_times() {
  if (header_time) {
    std::cout << "  header time:    " << std::right << std::setw(12) <<
      header_time->count() << "µs\n";
  }

  if (frame_times.size() >= 10) {
    std::cout << "  avg frame time: " << std::right << std::setw(12) <<
      static_cast<uint64_t>(
          std::accumulate(frame_times.begin(), frame_times.end(),
            microseconds{0}).count() /
          static_cast<float>(frame_times.size()))
      << "µs\n";

    auto [min, max] = std::ranges::minmax(frame_times);

    std::cout << "  min frame time: " << std::setw(12) << min.count() << "µs\n";
    std::cout << "  max frame time: " << std::setw(12) << max.count() << "µs\n";

  } else {

    for (size_t i = 0; i < frame_times.size(); ++i) {
      std::cout << "  frame #" << i << " time:  " << std::setw(12) <<
        frame_times[i].count() << "µs\n";
    }
  }

  std::cout << std::flush;
}






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




void on_frame_finish(pixglot::frame& /*unused*/) {
  frame_counter++;

  frame_times.emplace_back(duration_cast<microseconds>(steady_clock::now() - timestamp));
}

void on_frame_start(const pixglot::frame_view& /*unused*/) {
  if (!header_time) {
    header_time = duration_cast<microseconds>(steady_clock::now() - timestamp);
  }

  timestamp = steady_clock::now();
}



[[nodiscard]] std::string progressbar(float value) {
  size_t discrete_progress = value * 30;

  std::string output = '[' + std::string(discrete_progress, '=');

  if (discrete_progress < 30) {
    output += '>' + std::string(29 - discrete_progress, ' ');
  }

  output.push_back(']');
  return output;
}



void progress_loop(pixglot::progress_token ptoken) {
  while (!ptoken.finished()) {
    std::this_thread::sleep_for(std::chrono::milliseconds{25});

    std::cout << '\r' << progressbar(ptoken.progress())
      << " (" << frame_counter << " frame(s))" << std::flush;
  }

  std::cout << '\r' << progressbar(1.f)
    << " (" << frame_counter << " frame(s))" << std::endl;
}



void print_ipi(const pixglot::input_plane_info& ipi) {
  std::cout << pixglot::stringify(ipi.color_model());

  if (ipi.color_model() == pixglot::color_model::yuv) {
    std::cout << pixglot::stringify(ipi.subsampling());
  }

  if (ipi.has_alpha()) {
    std::cout << "+α";
  }

  bool uniform_color = !ipi.has_color() ||
    ipi.color_model() == pixglot::color_model::palette ||
      (ipi.color_model_format()[0] == ipi.color_model_format()[1] &&
      ipi.color_model_format()[1] == ipi.color_model_format()[2]);

  std::cout << '_';

  if (uniform_color) {
    std::cout << pixglot::stringify(ipi.color_model_format()[0]);
  } else {
    std::cout << pixglot::stringify(ipi.color_model_format()[0]) << '_';
    std::cout << pixglot::stringify(ipi.color_model_format()[1]) << '_';
    std::cout << pixglot::stringify(ipi.color_model_format()[2]);
  }

  if (ipi.has_alpha() &&
      (!uniform_color || ipi.color_model_format()[3] != ipi.color_model_format()[0])) {

    if (ipi.color_model_format()[3] == pixglot::data_source_format::index) {
      std::cout << "_indexed α";
    } else {
      std::cout << '_' << pixglot::stringify(ipi.color_model_format()[3]);
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

    if (f.duration() > microseconds{0}) {
      std::cout << ", " << f.duration().count() << "µs";
    }

    std::cout << ", ";
    print_ipi(f.input_plane());

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
    pixglot::progress_token token;
    token.frame_begin_callback(on_frame_start);
    token.frame_callback(on_frame_finish);

    auto at = token.access_token();

    std::jthread pthread{progress_loop, std::move(token)};

    timestamp = steady_clock::now();

    //NOLINTNEXTLINE(*pointer-arithmetic)
    auto image{pixglot::decode(pixglot::reader{argv[1]}, std::move(at))};

    pthread.join();

    //NOLINTNEXTLINE(*pointer-arithmetic)
    std::cout << '\n' << std::filesystem::path{argv[1]}.filename().native()
      << '\n' << std::endl;

    print_times();

    std::cout << '\n';

    print_image(image);

  } catch (std::exception& ex) {
    std::cerr << "***Error*** " << ex.what() << std::endl;
    return 1;
  }
}
