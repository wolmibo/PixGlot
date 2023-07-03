#include "pixglot/details/decoder.hpp"

using namespace pixglot::details;





decoder::decoder(
    reader&                read,
    progress_access_token  token,
    pixglot::output_format format
) :
  reader_{&read},
  token_ {std::move(token)},
  format_{format}
{}





void decoder::frame_total(size_t count) {
  frame_total_ = count;
}





void decoder::frame_mark_ready_until_line(size_t y) {
  progress(y, target().height(), frame_index_, frame_total_);
}



void decoder::frame_mark_ready_from_line(size_t y) {
  auto height = target().height();

  progress(height - y, height, frame_index_, frame_total_);
}





void decoder::progress(float f) {
  if (!token_.progress(f)) {
    throw abort{};
  }
}





void decoder::progress(size_t i, size_t n, size_t j, size_t m) {
  if (i > n || j > m || n * m == 0) {
    throw std::domain_error{"progress must be in the range [0, 1]"};
  }

  progress(
    (static_cast<float>(i) / static_cast<float>(n) + static_cast<float>(j))
    / static_cast<float>(m)
  );
}





void decoder::warn(std::string msg) {
  image_.add_warning(std::move(msg));
}





void decoder::finish_frame(frame f) {
  begin_frame(std::move(f));
  finish_frame();
}





void decoder::finish_frame() {
  if (!current_frame_) {
    throw std::runtime_error{"finish_frame called without previous begin_frame"};
  }

  make_format_compatible(*current_frame_, format_);

  if (!token_.append_frame(image_.add_frame(std::move(*current_frame_)))) {
    throw abort{};
  }

  current_frame_.reset();
  frame_index_++;

  if (frame_index_ <= frame_total_) {
    progress(frame_index_, frame_total_);
  }
}





pixglot::frame& decoder::begin_frame(frame f) {
  if (current_frame_) {
    throw std::runtime_error{
      "begin_frame called but previous frame has not been finished"};
  }
  current_frame_.emplace(std::move(f));

  return *current_frame_;
}





pixglot::image decoder::finish() {
  token_.finish();
  return std::move(image_);
}
