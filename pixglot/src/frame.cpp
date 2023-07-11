#include "pixglot/frame.hpp"

using namespace pixglot;
using std::chrono::microseconds;





namespace {
  using pixel_storage = std::variant<
    pixel_buffer,
    gl_texture
  >;
}




class frame::impl {
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





frame::~frame() = default;
frame::frame(frame&&) noexcept = default;
frame& frame::operator=(frame&&) noexcept = default;



frame::frame(pixel_buffer pixels) :
  impl_{std::make_unique<impl>(std::move(pixels))}
{}



frame::frame(gl_texture texture) :
  impl_{std::make_unique<impl>(std::move(texture))}
{}





square_isometry     frame::orientation() const { return impl_->orientation; }
microseconds        frame::duration()    const { return impl_->duration;    }
float               frame::gamma()       const { return impl_->gamma;       }
pixglot::alpha_mode frame::alpha_mode()  const { return impl_->alpha_mode;  }



void frame::orientation(square_isometry     iso     ) { impl_->orientation = iso;      }
void frame::duration   (microseconds        duration) { impl_->duration    = duration; }
void frame::gamma      (float               gamma   ) { impl_->gamma       = gamma;    }
void frame::alpha_mode (pixglot::alpha_mode alpha   ) { impl_->alpha_mode  = alpha;    }



storage_type frame::type() const {
  return static_cast<storage_type>(impl_->storage.index());
}



void frame::reset(pixel_buffer pixels ) { impl_->storage = std::move(pixels);  }
void frame::reset(gl_texture   texture) { impl_->storage = std::move(texture); }



gl_texture&   frame::texture() { return std::get<gl_texture  >(impl_->storage); }
pixel_buffer& frame::pixels()  { return std::get<pixel_buffer>(impl_->storage); }



const gl_texture& frame::texture() const {
  return std::get<gl_texture>(impl_->storage);
}

const pixel_buffer& frame::pixels() const {
  return std::get<pixel_buffer>(impl_->storage);
}



pixel_format frame::format() const {
  return std::visit([](auto&& arg) { return arg.format(); }, impl_->storage);
}

size_t frame::width() const {
  return std::visit([](auto&& arg) { return arg.width(); }, impl_->storage);
}

size_t frame::height() const {
  return std::visit([](auto&& arg) { return arg.height(); }, impl_->storage);
}
