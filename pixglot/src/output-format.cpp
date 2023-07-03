#include "pixglot/conversions.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/gl-texture.hpp"
#include "pixglot/image.hpp"
#include "pixglot/output-format.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/preference.hpp"
#include "pixglot/square-isometry.hpp"

#include <algorithm>
#include <functional>
#include <optional>

using namespace pixglot;



class output_format::impl {
  public:
    preference<pixglot::storage_type> storage_type;

    preference<bool>                  expand_gray_to_rgb;
    preference<bool>                  fill_alpha;
    preference<pixglot::data_format>  data_format;

    preference<pixglot::alpha_mode>   alpha_mode;
    preference<float>                 gamma;
    preference<std::endian>           endian;

    preference<square_isometry>       orientation;



    void make_standard() {
      storage_type       = storage_type::pixel_buffer;
      expand_gray_to_rgb = true;
      fill_alpha         = true;
      data_format        = data_format::u8;
      alpha_mode         = alpha_mode::straight;
      gamma              = gamma_s_rgb;
      endian             = std::endian::native;
      orientation        = square_isometry{};
    }



    void enforce() {
      storage_type.enforce();
      expand_gray_to_rgb.enforce();
      fill_alpha.enforce();
      data_format.enforce();
      alpha_mode.enforce();
      gamma.enforce();
      endian.enforce();
      orientation.enforce();
    }


    
    [[nodiscard]] bool satisfied_by(const image& img) const {
      return std::ranges::all_of(img.frames(), [this](const auto& f) {
          return satisfied_by(f);
      }); 
    }



    [[nodiscard]] bool satisfied_by(const frame& f) const {
      return satisfied_by(f.format()) &&
        gamma.satisfied_by(f.gamma()) &&
        orientation.satisfied_by(f.orientation()) &&
        storage_type.satisfied_by(f.type()) &&
        (f.type() != storage_type::pixel_buffer ||
         endian.satisfied_by(f.pixels().endian())) &&
        (f.alpha() == alpha_mode::none ||
         alpha_mode.satisfied_by(f.alpha()));
    }



    [[nodiscard]] bool satisfied_by(pixel_format format) const {
      return satisfied_by(format.channels) &&
        data_format.satisfied_by(format.format);
    }



    [[nodiscard]] bool satisfied_by(color_channels cc) const {
      return
        (has_alpha(cc) || !*fill_alpha         || !fill_alpha.required()        ) &&
        (has_color(cc) || !*expand_gray_to_rgb || !expand_gray_to_rgb.required());
    }



    template<typename T>
    [[nodiscard]] bool preference_satisfied_by(T&& obj) const {
      auto enforced = *this;
      enforced.enforce();

      return enforced.satisfied_by(std::forward<T>(obj));
    }
};




output_format::output_format(output_format&&) noexcept = default;

output_format::output_format(const output_format& rhs) :
  impl_{std::make_unique<impl>(*rhs.impl_)}
{}



output_format& output_format::operator=(output_format&&) noexcept = default;

output_format& output_format::operator=(const output_format& rhs) {
  impl_ = std::make_unique<impl>(*rhs.impl_);
  return *this;
}



output_format::~output_format() = default;



output_format::output_format() :
  impl_{std::make_unique<impl>()}
{}





output_format output_format::standard() {
  output_format def;
  def.impl_->make_standard();
  return def;
}





void output_format::storage_type(preference<pixglot::storage_type> pref) {
  impl_->storage_type = pref;
}

void output_format::data_format(preference<pixglot::data_format> pref) {
  impl_->data_format = pref;
}

void output_format::endian(preference<std::endian> pref) {
  impl_->endian = pref;
}

void output_format::expand_gray_to_rgb(preference<bool> pref) {
  impl_->expand_gray_to_rgb = pref;
}

void output_format::fill_alpha(preference<bool> pref) {
  impl_->fill_alpha = pref;
}

void output_format::alpha_mode(preference<pixglot::alpha_mode> pref) {
  impl_->alpha_mode = pref;
}

void output_format::gamma(preference<float> pref) {
  impl_->gamma = pref;
}

void output_format::orientation(preference<square_isometry> pref) {
  impl_->orientation = pref;
}





const preference<storage_type>& output_format::storage_type() const {
  return impl_->storage_type;
}

preference<storage_type>& output_format::storage_type() {
  return impl_->storage_type;
}



const preference<data_format>& output_format::data_format() const {
  return impl_->data_format;
}

preference<data_format>& output_format::data_format() {
  return impl_->data_format;
}



const preference<std::endian>& output_format::endian() const {
  return impl_->endian;
}

preference<std::endian>& output_format::endian() {
  return impl_->endian;
}



const preference<bool>& output_format::expand_gray_to_rgb() const {
  return impl_->expand_gray_to_rgb;
}

preference<bool>& output_format::expand_gray_to_rgb() {
  return impl_->expand_gray_to_rgb;
}



const preference<bool>& output_format::fill_alpha() const {
  return impl_->fill_alpha;
}

preference<bool>& output_format::fill_alpha() {
  return impl_->fill_alpha;
}



const preference<alpha_mode>& output_format::alpha_mode() const {
  return impl_->alpha_mode;
}

preference<alpha_mode>& output_format::alpha_mode() {
  return impl_->alpha_mode;
}



const preference<float>& output_format::gamma() const {
  return impl_->gamma;
}

preference<float>& output_format::gamma() {
  return impl_->gamma;
}



const preference<square_isometry>& output_format::orientation() const {
  return impl_->orientation;
}

preference<square_isometry>& output_format::orientation() {
  return impl_->orientation;
}






void output_format::enforce() { impl_->enforce(); }





bool output_format::satisfied_by(const image& img) const {
  return impl_->satisfied_by(img);
}

bool output_format::satisfied_by(const frame& f) const {
  return impl_->satisfied_by(f);
}

bool output_format::satisfied_by(pixel_format pf) const {
  return impl_->satisfied_by(pf);
}

bool output_format::satisfied_by(color_channels cc) const {
  return impl_->satisfied_by(cc);
}



bool output_format::preference_satisfied_by(const image& img) const {
  return impl_->preference_satisfied_by(img);
}

bool output_format::preference_satisfied_by(const frame& f) const {
  return impl_->preference_satisfied_by(f);
}

bool output_format::preference_satisfied_by(pixel_format pf) const {
  return impl_->preference_satisfied_by(pf);
}

bool output_format::preference_satisfied_by(color_channels cc) const {
  return impl_->preference_satisfied_by(cc);
}





namespace pixglot::details {
  void convert(gl_texture&, pixel_format, int, float, square_isometry);

  void convert(pixel_buffer&, std::optional<std::endian>,
                      pixel_format, int, float, square_isometry);
}




namespace {
  void apply_conversions(
      frame&               f,
      const output_format& fmt,
      pixel_format         target_format,
      int                  premultiply,
      float                gamma,
      square_isometry      transform
  ) {
    if (fmt.storage_type().required()) {
      convert_storage(f, *fmt.storage_type());
    }

    if (f.type() == storage_type::gl_texture) {
      details::convert(f.texture(), target_format, premultiply, gamma, transform);
    } else {
      auto target_endian = fmt.endian().required() ? 
        std::optional{*fmt.endian()} : std::nullopt;

      details::convert(f.pixels(), target_endian,
          target_format, premultiply, gamma, transform);
    }
  }



  void make_compatible(frame& f, const output_format& fmt) {
    square_isometry transform{};
    if (fmt.orientation().required()) {
      transform = inverse(*fmt.orientation()) * f.orientation();
      f.orientation(*fmt.orientation());
    }


    int premultiply{};
    if (fmt.alpha_mode().required()) {
      if (f.alpha() == alpha_mode::straight &&
          *fmt.alpha_mode() == alpha_mode::premultiplied) {
        premultiply = 1;
        f.alpha(alpha_mode::premultiplied);
      } else if (f.alpha() == alpha_mode::premultiplied
                 && *fmt.alpha_mode() == alpha_mode::straight) {
        premultiply = -1;
        f.alpha(alpha_mode::straight);
      }
    }


    float gamma{1.f};
    if (fmt.gamma().required()) {
      gamma = f.gamma() / *fmt.gamma();
      f.gamma(*fmt.gamma());
    }


    auto target_format = f.format();
    if (fmt.fill_alpha().required()) {
      target_format.channels = add_alpha(target_format.channels);

      if (f.alpha() == alpha_mode::none) {
        if (fmt.alpha_mode().preferred()) {
          f.alpha(*fmt.alpha_mode());
        } else {
          f.alpha(alpha_mode::straight);
        }
      }
    }
    if (fmt.expand_gray_to_rgb().required()) {
      target_format.channels = add_color(target_format.channels);
    }
    if (fmt.data_format().required()) {
      target_format.format = *fmt.data_format();
    }


    apply_conversions(f, fmt, target_format, premultiply, gamma, transform);
  }
}





void pixglot::make_format_compatible(
    frame&               f,
    const output_format& fmt,
    bool                 enforce
) {
  if (enforce) {
    auto enforced = fmt;
    enforced.enforce();
    make_compatible(f, fmt);
    return;
  }

  make_compatible(f, fmt);
}



void pixglot::make_format_compatible(
    image&               img,
    const output_format& fmt,
    bool                 enforce
) {
  if (enforce) {
    auto enforced = fmt;
    enforced.enforce();
    for (auto& f: img.frames()) {
      make_compatible(f, fmt);
    }
    return;
  }

  for (auto& f : img.frames()) {
    make_compatible(f, fmt);
  }
}
