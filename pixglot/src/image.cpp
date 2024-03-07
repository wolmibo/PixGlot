#include "pixglot/image.hpp"

#include "pixglot/frame-source-info.hpp"
#include "pixglot/metadata.hpp"

#include <chrono>
#include <vector>

using namespace pixglot;





class image::impl {
  public:
    std::vector<pixglot::frame> frames;
    std::vector<std::string>    warnings;

    pixglot::metadata           metadata;

    pixglot::codec              codec{codec::ppm};
    std::string                 mime_type;

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

const pixglot::metadata& image::metadata() const { return impl_->metadata; }
      pixglot::metadata& image::metadata()       { return impl_->metadata; }



std::string_view image::mime_type() const { return impl_->mime_type; }
pixglot::codec   image::codec()     const { return impl_->codec;     }





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





void image::codec(pixglot::codec c, std::string mime) {
  impl_->codec = c;

  if (!mime.empty()) {
    impl_->mime_type = std::move(mime);

  } else if (auto res = mime_types(c); !res.empty()) {
    impl_->mime_type = std::string{res.front()};
  }
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





namespace {
  [[nodiscard]] char to_lower(char c) {
    if ('A' <= c && c <= 'Z') {
      return static_cast<char>(c - 'A' + 'a');
    }
    return c;
  }



  constexpr std::array<std::string_view, 13> extensions {
    "jpg", "jpeg", "jfif",
    "png",
    "avif",
    "exr",
    "webp",
    "gif",
    "jxl",
    "pbm",
    "pgm",
    "ppm",
    "pfm",
  };



  [[nodiscard]] std::span<const std::string_view> potential_extensions(const image& img) {
    std::span all{extensions};

    switch (img.codec()) {
      case codec::jpeg: return all.subspan(0, 3);
      case codec::png:  return all.subspan(3, 1);
      case codec::avif: return all.subspan(4, 1);
      case codec::exr:  return all.subspan(5, 1);
      case codec::webp: return all.subspan(6, 1);
      case codec::gif:  return all.subspan(7, 1);
      case codec::jxl:  return all.subspan(8, 1);
      case codec::ppm:
        if (img.size() != 1) {
          return {};
        } else {
          if (img.frame().source_info().color_model_format()[0] ==
              pixglot::data_source_format::f32) {
            return all.subspan(12, 1);
          }

          auto mime = img.mime_type();
          if (mime == "image/x-portable-bitmap") {
            return all.subspan(9, 1);
          }

          if (mime == "image/x-portable-graymap") {
            return all.subspan(10, 1);
          }

          if (mime == "image/x-portable-pixmap") {
            return all.subspan(11, 1);
          }
        }
    }

    return {};
  }



  [[nodiscard]] std::string format_expected(std::span<const std::string_view> list) {
    if (list.empty()) {
      return "cannot suggest extension";
    }

    if (list.size() == 1) {
      return "expected ." + std::string{list.front()};
    }

    std::string output{"expected ."};
    for (size_t i = 0; i < list.size(); ++i) {
      if (i > 0) {
        if (i < list.size() - 1) {
          output += ", .";
        } else {
          output += ", or .";
        }
      }
      output += list[i];
    }

    return output;
  }
}





bool pixglot::validate_file_extension(image& img, std::string_view extension) {
  while (!extension.empty() && extension.front() == '.') {
    extension.remove_prefix(1);
  }

  std::string ext;
  ext.reserve(extension.size());
  for (auto c: extension) {
    ext.push_back(to_lower(c));
  }



  auto candidates = potential_extensions(img);

  if (ext.empty()) {
    img.add_warning("file extension is missing; " + format_expected(candidates));
    return false;
  }

  if (std::ranges::contains(candidates, ext)) {
    return true;
  }

  if (std::ranges::contains(extensions, ext)) {
    img.add_warning("file extension ." + ext + " is misleading; "
        + format_expected(candidates));
    return false;
  }

  img.add_warning("unknown file extension ." + ext + "; " + format_expected(candidates));
  return false;
}
