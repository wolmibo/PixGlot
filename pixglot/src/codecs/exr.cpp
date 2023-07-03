#include "pixglot/details/decoder.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/utils/cast.hpp"

#include <random>
#include <span>
#include <string_view>
#include <variant>

#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfLineOrder.h>
#include <OpenEXR/ImfPixelType.h>

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





  struct exr_image_frame {
    const Channel* red  {nullptr};
    const Channel* green{nullptr};
    const Channel* blue {nullptr};
    const Channel* alpha{nullptr};

    void set_channel(const std::string_view name, const Channel* c) {
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



  [[nodiscard]] PixelType convert_data_format(data_format df) {
    switch (df) {
      case data_format::f16: return PixelType::HALF;
      case data_format::f32: return PixelType::FLOAT;
      case data_format::u32: return PixelType::UINT;
      default: break;
    }

    throw decode_error{codec::exr, "Cannot load exr channel as " +
      to_string(df)};
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



  [[nodiscard]] color_channels expand_color_channels(
      color_channels       cc,
      const output_format& format
  ) {
    if (format.expand_gray_to_rgb.prefers(true)) {
      cc = add_color(cc);
    }
    if (format.add_alpha.prefers(true)) {
      cc = add_alpha(cc);
    }
    return cc;
  }



  [[nodiscard]] pixel_format determine_pixel_format(
      const exr_frame&     frame,
      const output_format& format
  ) {
    return pixel_format {
      .format   = determine_data_format(frame),
      .channels = expand_color_channels(determine_color_channels(frame), format)
    };
  }



  [[nodiscard]] bool is_data_channel(std::string_view name) {
    return name != "R" && name != "G" && name != "B" && name != "A";
  }



  [[nodiscard]] std::string_view channel_name(std::string_view full_name) {
    auto ipos = full_name.rfind('.');
    if (ipos == std::string_view::npos) {
      return full_name;
    }

    return full_name.substr(ipos + 1);
  }



  [[nodiscard]] std::vector<exr_frame> exr_determine_frames(InputFile& file) {
    std::vector<exr_frame> list;

    const auto& channels = file.header().channels();

    for (auto it = channels.begin(); it != channels.end(); ++it) {
      auto cname = channel_name(it.name());
      if (is_data_channel(channel_name(cname))) {
        list.emplace_back(it.name(), &it.channel());

      } else {
        std::string lut_name = it.name();
        lut_name.back() = 'R';

        auto c = std::ranges::find_if(list, [&lut_name](const auto& pair) {
            return pair.first == lut_name &&
              std::holds_alternative<exr_image_frame>(pair.second);
        });

        if (c == list.end()) {
          list.emplace_back(std::move(lut_name), exr_image_frame{});
          c = list.end() - 1;
        }

        auto& frame = std::get<exr_image_frame>(c->second);
        frame.set_channel(cname, &it.channel());
      }
    }

    std::ranges::sort(list, {}, &exr_frame::first);

    return list;
  }



  [[nodiscard]] bool has_channel_with_name(InputFile& file, std::string_view name) {
    const auto& channels = file.header().channels();

    for (auto it = channels.begin(); it != channels.end(); ++it) {
      if (it.name() == name) {
        return true;
      }
    }

    return false;
  }



  [[nodiscard]] std::string find_nonexisting_channel(
      InputFile&         file,
      const std::string& base
  ) {
    auto test = base + ".A";
    if (!has_channel_with_name(file, test)) {
      return test;
    }

    std::random_device rd;
    std::mt19937_64    gen{rd()};

    std::string buffer(sizeof(std::mt19937_64::result_type) * 2, ' ');

    for (size_t i = 0; i < 1024; ++i) {
      //NOLINTNEXTLINE(*-pointer-arithmetic)
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), gen(), 16);

      if (!has_channel_with_name(file, buffer)) {
        return buffer;
      }

      std::ranges::fill(buffer, ' ');
    }

    throw decode_error{codec::exr, "cannot find non-existing channel to fill alpha"};
  }



  [[nodiscard]] Slice create_slice(pixel_buffer& buffer, size_t index, float fill) {
    if (index >= n_channels(buffer.format().channels)) {
      throw decode_error{codec::exr, "color channel index out of bounds"};
    }

    size_t offset{index * byte_size(buffer.format().format)};

    return Slice{
      convert_data_format(buffer.format().format),
      utils::byte_pointer_cast<char>(buffer.data().subspan(offset).data()),
      buffer.format().size(),
      buffer.stride(),
      1, 1,
      fill
    };
  }








  class exr_decoder {
    public:
      explicit exr_decoder(details::decoder& decoder) :
        decoder_    {&decoder},
        reader_     {decoder_->input()},
        input_      {reader_},
        data_window_{input_.header().dataWindow()},
        width_      {static_cast<size_t>(data_window_.max.x - data_window_.min.x + 1)},
        height_     {static_cast<size_t>(data_window_.max.y - data_window_.min.y + 1)}
      {}



      void decode() {
        auto frame_sources = exr_determine_frames(input_);
        decoder_->frame_total(frame_sources.size());

        for (const auto& frame_source: frame_sources) {
          decode_frame(frame_source);
        }
      }



    private:
      details::decoder* decoder_;
      exr_reader        reader_;
      InputFile         input_;

      Box2i             data_window_;
      size_t            width_;
      size_t            height_;



      void decode_frame(const exr_frame& frame_source) {
        pixel_buffer buffer{
          width_,
          height_,
          determine_pixel_format(frame_source, decoder_->output_format())
        };

        auto& frame = decoder_->begin_frame(std::move(buffer));
        frame.gamma(gamma_linear);
        frame.alpha(has_alpha(frame.format().channels) ?
            alpha_mode::premultiplied : alpha_mode::none);



        FrameBuffer frame_buffer;

        auto& target = decoder_->target();

        std::optional<string> non_existing_channel;

        if (std::holds_alternative<const Channel*>(frame_source.second)) {
          frame_buffer.insert(frame_source.first, create_slice(target, 0, 0.f));

          size_t next_index{1};

          if (has_color(target.format().channels)) {
            frame_buffer.insert(frame_source.first,
                create_slice(target, next_index++, 0.f));
            frame_buffer.insert(frame_source.first,
                create_slice(target, next_index++, 0.f));
          }

          if (has_alpha(target.format().channels)) {
            if (!non_existing_channel) {
              non_existing_channel.emplace(
                  find_nonexisting_channel(input_, frame_source.first));
            }

            frame_buffer.insert(*non_existing_channel,
                create_slice(target, next_index++, 1.f));
          }

        } else {
          if (!has_color(target.format().channels)) {
            throw decode_error{codec::exr, "cannot load image without rgb channels"};
          }

          std::string cname = frame_source.first;
          cname.pop_back();

          frame_buffer.insert((cname + 'R'), create_slice(target, 0, 0.f));
          frame_buffer.insert((cname + 'G'), create_slice(target, 1, 0.f));
          frame_buffer.insert((cname + 'B'), create_slice(target, 2, 0.f));

          if (has_alpha(target.format().channels)) {
            frame_buffer.insert((cname + 'A'), create_slice(target, 3, 1.f));
          }
        }

        input_.setFrameBuffer(frame_buffer);



        transfer_pixels();

        decoder_->finish_frame();
      }



      void transfer_pixels() {
        if (input_.header().lineOrder() == LineOrder::INCREASING_Y) {
          for (auto y = data_window_.min.y; y <= data_window_.max.y; ++y) {
            input_.readPixels(y, y);
            decoder_->frame_mark_ready_until_line(y - data_window_.min.y + 1);
          }
        } else {
          for (auto y = data_window_.max.y; y >= data_window_.min.y; --y) {
            input_.readPixels(y, y);
            decoder_->frame_mark_ready_from_line(y - data_window_.min.y);
          }
        }
      }
  };
}



void decode_exr(details::decoder& dec) {
  try {
    exr_decoder edec{dec};
    edec.decode();

  } catch (const base_exception&) {
    throw;
  } catch (std::exception& ex) {
    throw decode_error{codec::exr, ex.what()};
  } catch (...) {
    throw decode_error{codec::exr, "unknown exception"};
  }
}
