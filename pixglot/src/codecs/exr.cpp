#include "../decoder.hpp"
#include "ImfPixelType.h"
#include "pixglot/frame.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/utils/cast.hpp"

#include <set>
#include <span>

#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfLineOrder.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <variant>

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





  struct exr_image_frame {
    const Channel* red  {nullptr};
    const Channel* green{nullptr};
    const Channel* blue {nullptr};
    const Channel* alpha{nullptr};

    void set_channel(std::string_view name, const Channel* c) {
      if      (name == "R") { red   = c; }
      else if (name == "G") { green = c; }
      else if (name == "B") { blue  = c; }
      else if (name == "A") { alpha = c; }
    }
  };

  using exr_frame = std::pair<std::string, std::variant<exr_image_frame, const Channel*>>;



  [[nodiscard]] data_format convert_data_format(PixelType ptype) {
    switch (ptype) {
      case PixelType::HALF:  return data_format::f16;
      case PixelType::UINT:  return data_format::u32;
      default:               return data_format::f32;
    }
  }



  void update_data_format(std::optional<data_format>& df, const Channel* c) {
    if (c == nullptr) {
      return;
    }
    auto new_df = convert_data_format(c->type);

    if (!df) {
      df = new_df;
    } else if (*df != new_df) {
      df = data_format::f32;
    }
  }



  [[nodiscard]] data_format determine_data_format(const exr_frame& frame) {
    if (const auto* f = std::get_if<exr_image_frame>(&frame.second)) {
      std::optional<data_format> df;
      update_data_format(df, f->red);
      update_data_format(df, f->green);
      update_data_format(df, f->blue);
      update_data_format(df, f->alpha);

      if (df) {
        return *df;
      }
    } else {
      if (const auto* channel = std::get<const Channel*>(frame.second)) {
        return convert_data_format(channel->type);
      }
    }

    return data_format::f32;
  }



  [[nodiscard]] color_channels determine_color_channels(const exr_frame& frame) {
    if (const auto* f = std::get_if<exr_image_frame>(&frame.second)) {
      if (f->alpha == nullptr) {
        return color_channels::rgb;
      }
      return color_channels::rgba;
    }
    return color_channels::gray;
  }



  [[nodiscard]] bool is_data_channel(std::string_view name) {
    return name != "R" && name != "G" && name != "B" && name != "A";
  }



  [[nodiscard]] std::vector<exr_frame> exr_determine_frames(RgbaInputFile& file) {
    const auto& channels = file.header().channels();

    std::set<std::string> layer_names;
    channels.layers(layer_names);

    std::vector<exr_frame> list;
    for (const auto& layer: layer_names) {
      ChannelList::ConstIterator layer_begin;
      ChannelList::ConstIterator layer_end;
      channels.channelsInLayer(layer, layer_begin, layer_end);

      for (auto it = layer_begin; it != layer_end; ++it) {
        if (is_data_channel(it.name())) {
          list.emplace_back(layer + it.name(), &it.channel());
        } else {
          auto c = std::ranges::find_if(list, [&layer](const auto& pair) {
              return pair.first == layer &&
                std::holds_alternative<exr_image_frame>(pair.second);
          });

          if (c == list.end()) {
            list.emplace_back(layer, exr_image_frame{});
            c = list.end() - 1;
          }

          auto& frame = std::get<exr_image_frame>(c->second);
          frame.set_channel(it.name(), &it.channel());
        }
      }
    }

    std::ranges::sort(list, {}, &exr_frame::first);

    return list;
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
