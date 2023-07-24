#include "pixglot/frame.hpp"
#include "pixglot/frame-source-info.hpp"
#include "pixglot/metadata.hpp"
#include "pixglot/details/no-pixels.hpp"

using namespace pixglot;
using std::chrono::microseconds;





namespace {
  using pixel_storage = std::variant<
    pixel_buffer,
    gl_texture,
    details::no_pixels
  >;
}




class frame_view::impl {
  public:
    square_isometry            orientation{square_isometry::identity};
    pixglot::alpha_mode        alpha_mode {alpha_mode::straight};
    float                      gamma      {gamma_s_rgb};
    microseconds               duration   {0};

    pixel_storage              storage;

    frame_source_info          source_info;
    pixglot::metadata          metadata;

    std::optional<std::string> name;


    impl(pixel_storage store) :
      storage{std::move(store)}
    {}
};





frame_view::~frame_view() = default;

frame_view::frame_view(const frame_view&) = default;
frame_view::frame_view(frame_view&&) noexcept = default;
frame_view& frame_view::operator=(const frame_view&) = default;
frame_view& frame_view::operator=(frame_view&&) noexcept = default;

frame_view::frame_view(std::shared_ptr<impl> imp) :
  impl_{std::move(imp)}
{}



storage_type frame_view::type() const {
  return static_cast<storage_type>(impl_->storage.index());
}

const gl_texture& frame_view::texture() const {
  return std::get<gl_texture>(impl_->storage);
}

const pixel_buffer& frame_view::pixels() const {
  return std::get<pixel_buffer>(impl_->storage);
}



pixel_format frame_view::format() const {
  return std::visit([](auto&& arg) { return arg.format(); }, impl_->storage);
}

size_t frame_view::width() const {
  return std::visit([](auto&& arg) { return arg.width(); }, impl_->storage);
}

size_t frame_view::height() const {
  return std::visit([](auto&& arg) { return arg.height(); }, impl_->storage);
}


const frame_source_info& frame_view::source_info() const { return impl_->source_info; }
const pixglot::metadata& frame_view::metadata()    const { return impl_->metadata;    }

square_isometry          frame_view::orientation() const { return impl_->orientation; }
microseconds             frame_view::duration()    const { return impl_->duration;    }
float                    frame_view::gamma()       const { return impl_->gamma;       }
pixglot::alpha_mode      frame_view::alpha_mode()  const { return impl_->alpha_mode;  }

std::optional<std::string_view> frame_view::name() const {
  return impl_->name;
}



size_t frame_view::id() const {
  //NOLINTNEXTLINE(*-reinterpret-cast)
  return size_t{reinterpret_cast<std::uintptr_t>(impl_.get())};
}






frame::~frame() = default;
frame::frame(frame&&) noexcept = default;
frame& frame::operator=(frame&&) noexcept = default;



frame::frame(pixel_buffer pixels) :
  frame_view{std::make_shared<impl>(std::move(pixels))}
{}



frame::frame(gl_texture texture) :
  frame_view{std::make_shared<impl>(std::move(texture))}
{}



frame::frame(size_t width, size_t height, pixel_format format) :
  frame_view{std::make_shared<impl>(details::no_pixels{width, height, format})}
{}



void frame::orientation(square_isometry     iso     ) { impl_->orientation = iso;      }
void frame::duration   (microseconds        duration) { impl_->duration    = duration; }
void frame::gamma      (float               gamma   ) { impl_->gamma       = gamma;    }
void frame::alpha_mode (pixglot::alpha_mode alpha   ) { impl_->alpha_mode  = alpha;    }

void frame::name       (std::string         name    ) { impl_->name = std::move(name); }
void frame::clear_name ()                             { impl_->name.reset();           }



void frame::reset(pixel_buffer pixels ) { impl_->storage = std::move(pixels);  }
void frame::reset(gl_texture   texture) { impl_->storage = std::move(texture); }

void frame::reset(size_t width, size_t height, pixel_format format) {
  impl_->storage = details::no_pixels{width, height, format};
}



gl_texture&        frame::texture()     { return std::get<gl_texture  >(impl_->storage); }
pixel_buffer&      frame::pixels()      { return std::get<pixel_buffer>(impl_->storage); }

frame_source_info& frame::source_info() { return impl_->source_info; }
pixglot::metadata& frame::metadata()    { return impl_->metadata;    }






std::string_view pixglot::stringify(alpha_mode a) {
  switch (a) {
    case alpha_mode::none:          return "none";
    case alpha_mode::straight:      return "straight";
    case alpha_mode::premultiplied: return "premultiplied";
  }
  return "<invalid alpha_mode>";
}



std::string_view pixglot::stringify(storage_type t) {
  switch (t) {
    case storage_type::pixel_buffer: return "pixel buffer";
    case storage_type::gl_texture:   return "gl texture";
    case storage_type::no_pixels:    return "no pixels";
  }
  return "<invalid pixel_target>";
}





namespace {
  [[nodiscard]] std::string storage_type_to_string(const frame_view& f) {
    if (f.type() == storage_type::no_pixels) {
      return std::to_string(f.width()) + 'x' + std::to_string(f.height()) +
        '@' + to_string(f.format());
    }

    return f.visit_storage([](auto&& arg) { return to_string(arg); });
  }
}



std::string pixglot::to_string(const frame_view& f) {
  return storage_type_to_string(f)
    + " [trafo=" + to_string(f.orientation())
    + "; alpha=" + to_string(f.alpha_mode())
    + "; gamma=" + std::to_string(f.gamma()) + "]";
}



std::string pixglot::to_string(const frame& f) {
  return to_string(dynamic_cast<const frame_view&>(f));
}
