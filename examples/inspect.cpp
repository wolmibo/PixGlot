#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include <pixglot/decode.hpp>

using namespace std::chrono;



std::atomic<size_t>         frame_counter{0};
std::vector<microseconds>   frame_times;
std::optional<microseconds> header_time;
steady_clock::time_point    timestamp;



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


    std::cout << pixglot::to_string(image) << std::endl;

  } catch (std::exception& ex) {
    std::cerr << "***Error*** " << ex.what() << std::endl;
    return 1;
  }
}
