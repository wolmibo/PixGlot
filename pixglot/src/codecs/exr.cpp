#include "pixglot/details/decoder.hpp"
#include "pixglot/details/hermit.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/frame-source-info.hpp"
#include "pixglot/metadata.hpp"
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



  class exr_reader : details::hermit, public IStream {
    public:
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
        if (!input_->seek(pos)) {
          throw decode_error{codec::exr, "unable to seek in source"};
        }
      }



    private:
      reader* input_;
  };





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



  [[nodiscard]] data_source_format data_source_format_channel(const Channel* channel) {
    if (channel == nullptr) {
      return data_source_format::none;
    }

    switch (channel->type) {
      case PixelType::HALF:  return data_source_format::f16;
      case PixelType::UINT:  return data_source_format::u32;
      default:               return data_source_format::f32;
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
    if (format.expand_gray_to_rgb().prefers(true)) {
      cc = add_color(cc);
    }
    if (format.fill_alpha().prefers(true)) {
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



  void set_frame_name(frame& dest_frame, const exr_frame& source_frame) {
    if (source_frame.first.empty()) {
      return;
    }

    if (std::holds_alternative<const Channel*>(source_frame.second)) {
      dest_frame.name(source_frame.first);

    } else if (source_frame.first.ends_with(".R") && source_frame.first.size() > 2) {
      dest_frame.name(source_frame.first.substr(0, source_frame.first.size() - 2));
    }
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





  void set_frame_source_info(frame_source_info& fsi, const exr_frame& frame_source) {
    if (const auto* channel = std::get_if<const Channel*>(&frame_source.second)) {
      fsi.color_model(color_model::value);
      fsi.color_model_format({
        data_source_format_channel(*channel),
        data_source_format_channel(*channel),
        data_source_format_channel(*channel),
        data_source_format::none,
      });

    } else if (const auto* image =
               std::get_if<exr_image_frame>(&frame_source.second)) {
      fsi.color_model(color_model::rgb);
      fsi.color_model_format({
        data_source_format_channel(image->red),
        data_source_format_channel(image->green),
        data_source_format_channel(image->blue),
        data_source_format_channel(image->alpha)
      });
    }
  }





  [[nodiscard]] std::string compression_name(const Compression& comp) {
    switch (comp) {
      case B44A_COMPRESSION:  return "b44a";
      case B44_COMPRESSION:   return "b44";
      case DWAA_COMPRESSION:  return "dwaa";
      case DWAB_COMPRESSION:  return "dwab";
      case NO_COMPRESSION:    return "none";
      case PIZ_COMPRESSION:   return "piz";
      case PXR24_COMPRESSION: return "pxr24";
      case RLE_COMPRESSION:   return "rle";
      case ZIPS_COMPRESSION:  return "zips";
      case ZIP_COMPRESSION:   return "zip";
      default:                return "unknown";
    }
  }



  [[nodiscard]] std::string to_string(const Box2i& b) {
    return "(" + std::to_string(b.min.x) + ", " + std::to_string(b.min.y)
      + ", " + std::to_string(b.max.x - b.min.x + 1)
      + ", " + std::to_string(b.max.y - b.min.y + 1) + ")";
  }



  [[nodiscard]] std::string to_string(LevelMode lm) {
    switch (lm) {
      case ONE_LEVEL:     return "single";
      case MIPMAP_LEVELS: return "mipMap";
      case RIPMAP_LEVELS: return "ripMap";
      default:            return "unknown";
    }
  }



  [[nodiscard]] std::string to_string(LevelRoundingMode lm) {
    switch (lm) {
      case ROUND_DOWN: return "down";
      case ROUND_UP:   return "up";
      default:         return "unknown";
    }
  }



  [[nodiscard]] std::string to_string(LineOrder lo) {
    switch (lo) {
      case INCREASING_Y: return "increasingY";
      case DECREASING_Y: return "decreasingY";
      default:           return "unknown";
    }
  }



  [[nodiscard]] std::vector<metadata::key_value> list_metadata(const Header& header) {
    std::vector<metadata::key_value> out;

    if (header.hasVersion()) {
      out.emplace_back("exr.version", std::to_string(header.version()));
    }

    if (header.hasName()) {
      out.emplace_back("exr.name", header.name());
    }

    out.emplace_back("exr.compression", compression_name(header.compression()));

    if (header.compression() == ZIP_COMPRESSION ||
        header.compression() == ZIPS_COMPRESSION) {

      out.emplace_back("exr.zip.level", std::to_string(header.zipCompressionLevel()));
    } else if (header.compression() == DWAA_COMPRESSION ||
               header.compression() == DWAB_COMPRESSION) {

      out.emplace_back("exr.dwa.level", std::to_string(header.dwaCompressionLevel()));
    }

    out.emplace_back("exr.displayWindow", to_string(header.displayWindow()));
    out.emplace_back("exr.dataWindow",    to_string(header.dataWindow()));

    if (header.hasChunkCount()) {
      out.emplace_back("exr.chunks", std::to_string(header.chunkCount()));
    }

    if (header.hasTileDescription()) {
      out.emplace_back("exr.tileDesc.mode", to_string(header.tileDescription().mode));
      out.emplace_back("exr.tileDesc.xSize",
          std::to_string(header.tileDescription().xSize));
      out.emplace_back("exr.tileDesc.ySize",
          std::to_string(header.tileDescription().ySize));
      out.emplace_back("exr.tileDesc.rounding",
          to_string(header.tileDescription().roundingMode));
    }

    if (header.hasType()) {
      out.emplace_back("exr.type", header.type());
    }

    if (header.hasView()) {
      out.emplace_back("exr.view", header.view());
    }

    out.emplace_back("exr.lineOrder", to_string(header.lineOrder()));

    out.emplace_back("exr.par", std::to_string(header.pixelAspectRatio()));


    return out;
  }





  class exr_decoder : details::hermit {
    public:
      explicit exr_decoder(details::decoder& decoder) :
        decoder_    {&decoder},
        reader_     {decoder_->input()},
        input_      {reader_},
        data_window_{input_.header().dataWindow()},
        width_      {static_cast<size_t>(data_window_.max.x - data_window_.min.x + 1)},
        height_     {static_cast<size_t>(data_window_.max.y - data_window_.min.y + 1)}
      {
        decoder_->image().codec(codec::exr);
      }



      void decode() {
        auto frame_sources = exr_determine_frames(input_);
        decoder_->frame_total(frame_sources.size());

        decoder_->image().metadata().append_move(list_metadata(input_.header()));

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
        auto& frame = decoder_->begin_frame(width_, height_,
            determine_pixel_format(frame_source, decoder_->output_format()));

        set_frame_source_info(frame.source_info(), frame_source);
        set_frame_name(frame, frame_source);

        frame.gamma(gamma_linear);
        frame.alpha_mode(has_alpha(frame.format().channels) ?
            alpha_mode::premultiplied : alpha_mode::none);

        if (decoder_->wants_pixel_transfer()) {
          decoder_->begin_pixel_transfer();
          set_frame_buffer(frame_source);
          transfer_pixels();
          decoder_->finish_pixel_transfer();
        }

        decoder_->finish_frame();
      }



      void set_frame_buffer(const exr_frame& frame_source) {
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
