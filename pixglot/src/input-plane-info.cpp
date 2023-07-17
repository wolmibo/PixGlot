#include "pixglot/input-plane-info.hpp"





std::string_view pixglot::stringify(color_model cm) {
  switch (cm) {
    case color_model::unknown: return "unknown";
    case color_model::yuv:     return "yuv";
    case color_model::rgb:     return "rgb";
    case color_model::palette: return "palette";
    case color_model::value:   return "value";
  }

  return "<invalid color_model>";
}



std::string pixglot::to_string(color_model cm) {
  return std::string{stringify(cm)};
}





std::string_view pixglot::stringify(chroma_subsampling css) {
  switch (css) {
    case chroma_subsampling::cs420: return "420";
    case chroma_subsampling::cs422: return "422";
    case chroma_subsampling::cs444: return "444";
  }

  return "<invalid chroma_subsampling>";
}



std::string pixglot::to_string(chroma_subsampling css) {
  return std::string{stringify(css)};
}





std::string_view pixglot::stringify(data_source_format dsf) {
  switch (dsf) {
    case data_source_format::ascii: return "ascii";
    case data_source_format::index: return "index";
    case data_source_format::none:  return "none";
    case data_source_format::u1:    return "u1";
    case data_source_format::u2:    return "u2";
    case data_source_format::u4:    return "u4";
    case data_source_format::u8:    return "u8";
    case data_source_format::u10:   return "u10";
    case data_source_format::u12:   return "u12";
    case data_source_format::u16:   return "u16";
    case data_source_format::u32:   return "u32";

    case data_source_format::f16:   return "f16";
    case data_source_format::f32:   return "f32";
  }

  return "<invalid data_source_format>";
}



std::string pixglot::to_string(data_source_format dsf) {
  return std::string{stringify(dsf)};
}



bool pixglot::is_float(data_source_format dsf) {
  return std::to_underlying(dsf) > 0 &&
    (std::to_underlying(dsf) & data_source_format_float_mask) != 0;
}



size_t pixglot::bit_count(data_source_format dsf) {
  if (std::to_underlying(dsf) <= 0) {
    return 0;
  }

  return std::to_underlying(dsf) & (~data_source_format_float_mask);
}



pixglot::data_format pixglot::to_data_format(data_source_format dsf) {
  switch (dsf) {
    case data_source_format::index:
    case data_source_format::none:
    case data_source_format::u1:
    case data_source_format::u2:
    case data_source_format::u4:
    case data_source_format::u8:
      return data_format::u8;
    case data_source_format::ascii:
    case data_source_format::u10:
    case data_source_format::u12:
    case data_source_format::u16:
      return data_format::u16;
    case data_source_format::u32:
      return data_format::u32;

    case data_source_format::f16:
      return data_format::f16;
    case data_source_format::f32:
      return data_format::f32;
  }

  return data_format::f32;
}



pixglot::data_source_format pixglot::data_source_format_from(data_format df) {
  switch (df) {
    case data_format::u8:  return data_source_format::u8;
    case data_format::u16: return data_source_format::u16;
    case data_format::u32: return data_source_format::u32;

    case data_format::f16: return data_source_format::f16;
    case data_format::f32: return data_source_format::f32;
  }

  return data_source_format::none;
}





using ipi = pixglot::input_plane_info;

class ipi::impl {
  public:
    pixglot::color_model color_model{pixglot::color_model::value};
    chroma_subsampling   subsampling{pixglot::chroma_subsampling::cs444};

    std::array<data_source_format, 4> color_channel_format{data_source_format::none};
};





ipi::input_plane_info(const input_plane_info& rhs) :
  impl_{std::make_unique<impl>(*rhs.impl_)}
{}

ipi::input_plane_info(input_plane_info&&) noexcept = default;

ipi& ipi::operator=(const input_plane_info& rhs) {
  impl_ = std::make_unique<impl>(*rhs.impl_);

  return *this;
}

ipi& ipi::operator=(input_plane_info&&) noexcept = default;

ipi::~input_plane_info() = default;





ipi::input_plane_info() :
  impl_{std::make_unique<impl>()}
{}





bool ipi::has_color() const {
  return impl_->color_model != pixglot::color_model::value;
}



bool ipi::has_alpha() const {
  return impl_->color_channel_format[3] != data_source_format::none;
}



pixglot::color_model        ipi::color_model() const { return impl_->color_model; }
pixglot::chroma_subsampling ipi::subsampling() const { return impl_->subsampling; }

const std::array<pixglot::data_source_format, 4>& ipi::color_model_format() const {
  return impl_->color_channel_format;
}



void ipi::color_model(pixglot::color_model model) { impl_->color_model = model; }
void ipi::subsampling(chroma_subsampling     css) { impl_->subsampling = css;   }

void ipi::color_model_format(std::array<data_source_format, 4> dsf) {
  impl_->color_channel_format = dsf;
}
