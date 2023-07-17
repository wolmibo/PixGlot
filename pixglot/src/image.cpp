#include "pixglot/image.hpp"

#include <chrono>
#include <optional>
#include <vector>

using namespace pixglot;





class image::impl {
  public:
    std::vector<pixglot::frame> frames;
    std::vector<std::string>    warnings;

    bool                        animated{false};
};





image::~image() = default;
image::image(image&&) noexcept = default;
image& image::operator=(image&&) noexcept = default;

image::image() :
  impl_{std::make_unique<impl>()}
{}





std::span<frame>       image::frames()       { return impl_->frames; }
std::span<const frame> image::frames() const { return impl_->frames; }

image::operator        bool()          const { return !impl_->frames.empty(); }
bool                   image::empty()  const { return  impl_->frames.empty(); }

size_t                 image::size()   const { return impl_->frames.size(); }



const frame& image::operator[](size_t index) const { return impl_->frames[index]; }
      frame& image::operator[](size_t index)       { return impl_->frames[index]; }

const frame* image::begin() const { return impl_->frames.data(); }
      frame* image::begin()       { return impl_->frames.data(); }

//NOLINTBEGIN(*-pointer-arithmetic)
const frame* image::end() const { return impl_->frames.data() + impl_->frames.size(); }
      frame* image::end()       { return impl_->frames.data() + impl_->frames.size(); }
//NOLINTEND(*-pointer-arithmetic)



const frame& image::frame(size_t index) const { return impl_->frames.at(index); }
      frame& image::frame(size_t index)       { return impl_->frames.at(index); }








bool                         image::animated() const { return impl_->animated; }
std::span<const std::string> image::warnings() const { return impl_->warnings; }

bool image::has_warnings() const {
  return !impl_->warnings.empty();
}





void image::add_warning(std::string str) {
  impl_->warnings.emplace_back(std::move(str));
}





frame& image::add_frame(pixglot::frame frame) {
  if (frame.duration() > std::chrono::microseconds{0}) {
    impl_->animated = true;
  }

  impl_->frames.emplace_back(std::move(frame));

  return impl_->frames.back();
}





std::string pixglot::to_string(const image& img) {
  std::string buffer = "{";
  for (size_t i = 0; i < img.size(); ++i) {
    if (i > 0) {
      buffer += ", ";
    }
    buffer += to_string(img.frame(i));
  }
  buffer += std::string{"} [animated="} + (img.animated() ? "y" : "n") + "; warnings: {";
  auto warnings = img.warnings();
  for (size_t i = 0; i < warnings.size(); ++i) {
    if (i > 0) {
      buffer += ", ";
    }
    buffer += warnings[i];
  }
  buffer += "}]";

  return buffer;
}
