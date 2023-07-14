#include "pixglot/details/decoder.hpp"
#include "pixglot/conversions.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/pixel-buffer.hpp"

#include <utility>

#include <GL/gl.h>

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





namespace {
  enum class direction : int {
    no_upload,

    unset,
    up,
    down,
  };




  [[nodiscard]] bool wants_upload(
      const std::optional<pixglot::frame>& f,
      const pixglot::pixel_buffer&         buf
  ) {
    return f && f->type() == pixglot::storage_type::gl_texture &&
      (buf.endian() == std::endian::native || byte_size(buf.format().format) == 1);
  }



  [[nodiscard]] bool direction_compatible(direction dir, int value) {
    auto val = static_cast<direction>(value);
    return val == direction::unset || val == dir;
  }
}





void decoder::frame_mark_ready_until_line(size_t y) {
  if (direction_compatible(direction::up, upload_direction_) &&
      wants_upload(current_frame_, target()) &&
      token_.upload_requested()) {
    upload_direction_ = std::to_underlying(direction::up);

    current_frame_->texture().upload_lines(target(), uploaded_, y - uploaded_);
    if (token_.flush_uploads()) {
      glFlush();
    }

    uploaded_ = y;
  }

  progress(y, target().height(), frame_index_, frame_total_);
}



void decoder::frame_mark_ready_from_line(size_t y) {
  auto height = target().height();

  if (direction_compatible(direction::down, upload_direction_) &&
      wants_upload(current_frame_, target()) &&
      token_.upload_requested()) {

    if (upload_direction_ == std::to_underlying(direction::unset)) {
      uploaded_ = height;
      upload_direction_ = std::to_underlying(direction::down);
    }

    if (y < uploaded_) {
      current_frame_->texture().upload_lines(target(), y, uploaded_ - y);
      if (token_.flush_uploads()) {
        glFlush();
      }

      uploaded_ = y;
    }
  }

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

    upload_direction_ = std::to_underlying(direction::unset);

  } else {
    target_ = &current_frame_->pixels();

    upload_direction_ = std::to_underlying(direction::no_upload);
  }

  uploaded_ = 0;

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

  switch (static_cast<direction>(upload_direction_)) {
    case direction::up:
      current_frame_->texture().upload_lines(target(), uploaded_,
          target().height() - uploaded_);
      break;
    case direction::down:
      current_frame_->texture().upload_lines(target(), 0, uploaded_);
      break;
    default:
      current_frame_->texture().upload_lines(target(), 0, target().height());
      break;
  }
}
