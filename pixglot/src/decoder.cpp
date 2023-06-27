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





void decoder::warn(std::string_view msg) {
  image_.warnings.emplace_back(msg);
}





void decoder::finish_frame(frame f) {
  make_format_compatible(f, format_);

  image_.frames.emplace_back(std::move(f));
  if (!token_.append_frame(image_.frames.back())) {
    throw abort{};
  }
}





void decoder::finish_frame() {
  if (!current_frame_) {
    throw std::runtime_error{"finish_frame called without previous begin_frame"};
  }

  finish_frame(std::move(*current_frame_));
  current_frame_.reset();
}





void decoder::begin_frame(frame f) {
  if (current_frame_) {
    throw std::runtime_error{
      "begin_frame called but previous frame has not been finished"};
  }
  current_frame_.emplace(std::move(f));
}





pixglot::image decoder::finish() {
  token_.finish();
  return std::move(image_);
}
