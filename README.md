# PixGlot

A C++ image decoder collection library for various formats with high-bit and multiframe
support.

Features:
* Support for ppm, png, jpeg, gif, webp, avif, jxl, openexr
* Loading progress feedback for ppm, png, jpeg, multi-frame images
* Animated images
* 8-bit / 16-bit / 32-bit / float / half-float buffer
* Loading to memory (rows aligned to 32 bytes) or OpenGL texture


## Example

```cpp
#include <iostream>

#include <pixglot/decode.hpp>

int main(int argc, char** argv) {
  try {
    auto img = pixglot::decode(pixglot::reader{argv[1]});

    for (const auto& warning: img.warnings) {
      std::cerr << "Warning: " << warning << std::endl;
    }

    for (size_t i = 0; i < img.frames.size(); ++i) {
      std::cout << "Frame #" << i << ": "
        << pixglot::to_string(img.frames[i].pixels) << std::endl;
    }
  } catch (pixglot::base_exception& ex) {
    std::cerr << "*ERROR:*" << ex.what() << std::endl;
  }
}
```



# Supported input formats

| Format    | Library    | Notes                                                         |
|-----------|------------|---------------------------------------------------------------|
| ppm       | *built-in* | Supports Pf, PF and 16bit (big endian) for P2, P3, P5, and P6 |
| png       | libpng     |                                                               |
| jpeg      | libjpeg    |                                                               |
| gif       | giflib     | Supports animation                                            |
| webp      | libwebp    | Supports animation                                            |
| avif      | libavif    | Supports animation                                            |
| jxl       | libjxl     | Supports animation                                            |
| exr       | openexr    | Only simple API (single frame, half fp) currently supported   |



# Build instructions

## Dependencies

To build PixGlot you will need:
* GCC C++ 12 or newer
* meson
* libepoxy
* libpng (Optional)
* libjpeg (Optional)
* libavif (Optional)
* OpenEXR (Optional)
* libwebp (Optional)
* libjxl (Optional)
* giflib (Optional)

To install all dependencies on Fedora 37 run:
```sh
sudo dnf install gcc-c++ meson mesa-libGL-devel libepoxy-devel \
libpng-devel libjpeg-turbo-devel giflib-devel libwebp-devel \
libavif-devel libjxl-devel openexr-devel
```


## Build and Install

To install PixGlot run the following commands in the project's root directory:
```sh
meson setup -Dbuildtype=release build
meson compile -C build
sudo meson install -C build
```
