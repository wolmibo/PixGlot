#include "pixglot/details/decoder.hpp"
#include "pixglot/conversions.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/pixel-buffer.hpp"

using namespace pixglot::details;





decoder::decoder(
    reader&                       read,
    progress_access_token         token,
    const pixglot::output_format* format
) :
  reader_{&read},
  token_ {std::move(token)},
  format_{format}
{
  if (format_->storage_type().require(storage_type::gl_texture)) {
    format_replacement_.emplace(*format);
    format_replacement_->endian(std::endian::native);

    format_ = &(*format_replacement_);
  }
}





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
    throw decoding_aborted{};
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

  finish_upload();



  pixel_target_.reset();
  target_ = nullptr;



  make_format_compatible(*current_frame_, *format_);

  if (!token_.append_frame(image_.add_frame(std::move(*current_frame_)))) {
    throw decoding_aborted{};
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

  if (format_->storage_type().require(storage_type::gl_texture)) {
    pixel_target_ = std::move(current_frame_->pixels());
    target_ = &(*pixel_target_);

    current_frame_->reset(gl_texture{
        pixel_target_->width(),
        pixel_target_->height(),
        pixel_target_->format()
    });

  } else {
    target_ = &current_frame_->pixels();
  }

  if (!token_.begin_frame(*current_frame_)) {
    throw decoding_aborted{};
  }

  return *current_frame_;
}





pixglot::image decoder::finish() {
  token_.finish();
  return std::move(image_);
}





pixglot::pixel_buffer& decoder::target() {
  if (target_ == nullptr) {
    throw std::runtime_error{"no active target"};
  }

  return *target_;
}





void decoder::finish_upload() {
  if (current_frame_->type() != storage_type::gl_texture || !pixel_target_) {
    return;
  }

  convert_endian(*pixel_target_, std::endian::native);

  current_frame_->texture().upload_lines(*pixel_target_, 0, target().height());
}
