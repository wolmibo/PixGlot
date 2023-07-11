#include "pixglot/frame.hpp"

using namespace pixglot;
using std::chrono::microseconds;





namespace {
  using pixel_storage = std::variant<
    pixel_buffer,
    gl_texture
  >;
}




class frame_view::impl {
  public:
    square_isometry     orientation{square_isometry::identity};
    pixglot::alpha_mode alpha_mode {alpha_mode::straight};
    float               gamma      {gamma_s_rgb};
    microseconds        duration   {0};

    pixel_storage       storage;


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



square_isometry     frame_view::orientation() const { return impl_->orientation; }
microseconds        frame_view::duration()    const { return impl_->duration;    }
float               frame_view::gamma()       const { return impl_->gamma;       }
pixglot::alpha_mode frame_view::alpha_mode()  const { return impl_->alpha_mode;  }



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



void frame::orientation(square_isometry     iso     ) { impl_->orientation = iso;      }
void frame::duration   (microseconds        duration) { impl_->duration    = duration; }
void frame::gamma      (float               gamma   ) { impl_->gamma       = gamma;    }
void frame::alpha_mode (pixglot::alpha_mode alpha   ) { impl_->alpha_mode  = alpha;    }



void frame::reset(pixel_buffer pixels ) { impl_->storage = std::move(pixels);  }
void frame::reset(gl_texture   texture) { impl_->storage = std::move(texture); }



gl_texture&   frame::texture() { return std::get<gl_texture  >(impl_->storage); }
pixel_buffer& frame::pixels()  { return std::get<pixel_buffer>(impl_->storage); }
