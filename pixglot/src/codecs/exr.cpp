#include "../decoder.hpp"
#include "ImfLineOrder.h"
#include "pixglot/frame.hpp"
#include "pixglot/utils/cast.hpp"

#include <span>

#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfRgbaFile.h>

using namespace pixglot;



namespace {
  using namespace Imf;
  using namespace Imath;



  class exr_reader : public IStream {
    public:
      exr_reader(const exr_reader&) = delete;
      exr_reader(exr_reader&&)      = delete;
      exr_reader& operator=(const exr_reader&) = delete;
      exr_reader& operator=(exr_reader&&)      = delete;

      ~exr_reader() override = default;

      explicit exr_reader(reader& input) :
        IStream{"pixglot::reader"},
        input_ {&input}
      {}



      [[nodiscard]] bool isMemoryMapped() const override {
        // FIXME: would be nice if exr_reader would support memory mapping where possible
        return false;
      }



      [[nodiscard]] bool read(char buffer[], int n) override { //NOLINT
        auto data = std::as_writable_bytes(std::span{buffer, static_cast<size_t>(n)});

        if (input_->read(data) < data.size()) {
          throw decode_error{codec::exr, "unexpected eof"};
        }
        return !input_->eof();
      }



      [[nodiscard]] uint64_t tellg() override {
        return input_->position();
      }



      void seekg(uint64_t pos) override {
        input_->seek(pos);
      }



    private:
      reader* input_;
  };




  [[nodiscard]] size_t stride_in_pixels(const pixel_buffer& pixels) {
    return pixels.stride() / pixels.format().size();
  }




  class exr_decoder : public decoder {
    public:
      explicit exr_decoder(decoder&& parent) :
        decoder(std::move(parent)),
        reader_{input()},
        input_{reader_}
      {}



      void decode() {
        Box2i data_window = input_.dataWindow();

        size_t width  = data_window.max.x - data_window.min.x + 1;
        size_t height = data_window.max.y - data_window.min.y + 1;

        constexpr pixel_format format{
          .format   = data_format::f16,
          .channels = color_channels::rgba
        };

        static_assert(format.size() == sizeof(Rgba),
          "openexr: size of Imf::Rgba does not match rgba_f16");

        static_assert(pixel_buffer::padding() % format.size() == 0,
          "openexr: padding of pixel_buffer is incompatible");

        pixel_buffer buffer{width, height, format};

        input_.setFrameBuffer(utils::interpret_as<Rgba>(buffer.data()).data(),
                                1, stride_in_pixels(buffer));



        if (input_.lineOrder() == Imf_3_1::INCREASING_Y) {
          for (auto y = data_window.min.y; y <= data_window.max.y; ++y) {
            input_.readPixels(y, y);
            progress(y - data_window.min.y + 1, height);
          }
        } else {
          for (auto y = data_window.max.y; y >= data_window.min.y; ++y) {
            input_.readPixels(y, y);
            progress(data_window.max.y - y + 1, height);
          }
        }



        append_frame(frame {
          .pixels      = std::move(buffer),
          .orientation = square_isometry::identity,
          .alpha       = alpha_mode::straight,
          .gamma       = gamma_linear,
          .endianess   = std::endian::native,
          .duration    = std::chrono::microseconds{0}
        });
      }



    private:
      exr_reader    reader_;
      RgbaInputFile input_;
  };
}



image decode_exr(decoder&& dec) {
  try {
    exr_decoder edec{std::move(dec)};

    edec.decode();

    return edec.finalize_image();

  } catch (const base_exception&) {
    throw;
  } catch (std::exception& ex) {
    throw decode_error{codec::exr, ex.what()};
  } catch (...) {
    throw decode_error{codec::exr, "unknown exception"};
  }
}
