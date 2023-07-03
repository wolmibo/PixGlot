#include "pixglot/frame.hpp"

using namespace pixglot;
using std::chrono::microseconds;





class frame::impl {
  public:
    square_isometry     orientation{square_isometry::identity};
    pixglot::alpha_mode alpha_mode {alpha_mode::straight};
    float               gamma      {gamma_s_rgb};
    microseconds        duration   {0};
};





frame::~frame() = default;
frame::frame(frame&&) noexcept = default;
frame& frame::operator=(frame&&) noexcept = default;



frame::frame(pixel_buffer pixels) :
  storage_{std::move(pixels)},
  impl_   {std::make_unique<impl>()}
{}



frame::frame(gl_texture texture) :
  storage_{std::move(texture)},
  impl_   {std::make_unique<impl>()}
{}





square_isometry     frame::orientation() const { return impl_->orientation; }
microseconds        frame::duration()    const { return impl_->duration;    }
float               frame::gamma()       const { return impl_->gamma;       }
pixglot::alpha_mode frame::alpha_mode()  const { return impl_->alpha_mode;  }



void frame::orientation(square_isometry     iso     ) { impl_->orientation = iso;      }
void frame::duration   (microseconds        duration) { impl_->duration    = duration; }
void frame::gamma      (float               gamma   ) { impl_->gamma       = gamma;    }
void frame::alpha_mode (pixglot::alpha_mode alpha   ) { impl_->alpha_mode  = alpha;    }
