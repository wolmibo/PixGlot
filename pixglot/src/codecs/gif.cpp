#include "config.hpp"
#include "pixglot/details/decoder.hpp"
#include "pixglot/details/xmp.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/frame-source-info.hpp"
#include "pixglot/metadata.hpp"
#include "pixglot/pixel-format-conversion.hpp"
#include "pixglot/utils/cast.hpp"

#include <chrono>

#include <gif_lib.h>

using namespace pixglot;



namespace {
  [[nodiscard]] std::string get_error_message(int error) {
    const char* msg{GifErrorString(error)};
    if (msg != nullptr) {
      return msg;
    }
    return "unknown error: " + std::to_string(error);
  }



  void gif_assert(
      int                         error,
      const std::string&          msg,
      const std::source_location& location = std::source_location::current()
  ) {
    if (error != D_GIF_SUCCEEDED) {
      throw decode_error{codec::gif, msg + ": " + get_error_message(error), location};
    }
  }



  [[nodiscard]] constexpr size_t saturating_cast(int v) {
    return v < 0 ? 0 : v;
  }



  class gif_file {
    public:
      gif_file(const gif_file&) = delete;
      gif_file(gif_file&&)      = delete;
      gif_file& operator=(const gif_file&) = delete;
      gif_file& operator=(gif_file&&)      = delete;

      ~gif_file() {
        int error{D_GIF_SUCCEEDED};
        DGifCloseFile(gif_, &error);
        gif_assert(error, "unable to close gif file");
      }



      explicit gif_file(reader& input) :
        input_{&input}
      {
        int error{D_GIF_SUCCEEDED};
        gif_ = DGifOpen(this, &read, &error);
        gif_assert(error, "unable to open gif file");

        if (gif_ == nullptr) {
          throw decode_error{codec::gif, "unable to open gif file"};
        }
      }





      [[nodiscard]] GifFileType*       get()              { return gif_; }
                    GifFileType*       operator->()       { return gif_; }
                    const GifFileType* operator->() const { return gif_; }



      void slurp() {
        if (DGifSlurp(gif_) != GIF_OK) {
          gif_assert(gif_->Error, "unable to slurp gif");
        }
      }





    private:
      reader*      input_;
      GifFileType* gif_;



      [[nodiscard]] static int read(GifFileType* file, GifByteType* data, int count) {
        auto* self = static_cast<gif_file*>(file->UserData);

        return self->input_->read(
            std::as_writable_bytes(std::span{data, saturating_cast(count)}));
      }
  };




  [[nodiscard]] inline rgba<u8> convert_gif_color(GifColorType c) {
    return rgba<u8>{
      .r = c.Red,
      .g = c.Green,
      .b = c.Blue,
      .a = 0xff
    };
  }



  enum class dispose {
    leave_in_place,
    background,
    previous,
  };



  [[nodiscard]] constexpr dispose convert_disposal(int disp) {
    switch (disp) {
      default:
      case DISPOSE_DO_NOT:
        return dispose::leave_in_place;
      case DISPOSE_PREVIOUS:
        return dispose::previous;
      case DISPOSE_BACKGROUND:
        return dispose::background;
    }
  }



  class gif_meta {
    public:
      explicit gif_meta(const SavedImage& img) {
        for (const auto& block: eblocks(img)) {
          if (auto gcb = to_gcb(block)) {
            load_gcb(*gcb);
          }
        }
      }



      [[nodiscard]] std::optional<size_t>     alpha()        const {return alpha_index_;}
      [[nodiscard]] std::chrono::microseconds duration()     const {return duration_;   }
      [[nodiscard]] dispose                   dispose_mode() const {return dispose_;    }



    private:
      std::optional<size_t>     alpha_index_;
      std::chrono::microseconds duration_{0};
      dispose                   dispose_{dispose::background};



      void load_gcb(const GraphicsControlBlock& gcb) {
        alpha_index_ = to_alpha_index(gcb.TransparentColor);
        duration_    = std::chrono::microseconds(gcb.DelayTime * 10000);
        dispose_     = convert_disposal(gcb.DisposalMode);
      }



      [[nodiscard]] static std::span<ExtensionBlock> eblocks(const SavedImage& img) {
        return std::span{
          img.ExtensionBlocks,
          saturating_cast(img.ExtensionBlockCount)
        };
      }



      [[nodiscard]] static std::optional<GraphicsControlBlock> to_gcb(
          const ExtensionBlock& block
      ) {
        if (block.Function == GRAPHICS_EXT_FUNC_CODE) {
          GraphicsControlBlock gcb;
          DGifExtensionToGCB(block.ByteCount, block.Bytes, &gcb);
          return gcb;
        }
        return {};
      }



      [[nodiscard]] static std::optional<size_t> to_alpha_index(int ix) {
        if (ix == NO_TRANSPARENT_COLOR || ix < 0) {
          return {};
        }
        return ix;
      }
  };




  class gif_palette {
    public:
      gif_palette(
          const ColorMapObject& cmap,
          std::optional<size_t> alpha_index,
          int                   background_index
      ) {
        if (cmap.ColorCount < 0) {
          throw decode_error{codec::gif, "negative color count"};
        }

        auto color_count = static_cast<size_t>(cmap.ColorCount);

        if (color_count > colors_.size()) {
          throw decode_error{codec::gif, "to many colors in palette"};
        }

        valid_colors_ = valid_colors_.subspan(0, color_count);



        std::ranges::transform(std::span{cmap.Colors, color_count},
            valid_colors_.begin(), convert_gif_color);




        if (background_index >= 0
            && static_cast<size_t>(background_index) < valid_colors_.size()) {
          background_ = valid_colors_[background_index];
        }

        if (alpha_index && *alpha_index < valid_colors_.size()) {
          valid_colors_[*alpha_index] = neutral_color();
        }
      }



      [[nodiscard]] rgba<u8> background() const {
        return background_;
      }



      [[nodiscard]] rgba<u8> resolve_color(u8 index) const {
        if (index < valid_colors_.size()) {
          return valid_colors_[index];
        }
        throw decode_error{codec::gif, "palette index out of range"};
      }





    private:
      std::array<rgba<u8>, 256> colors_      {};
      std::span<rgba<u8>>       valid_colors_{colors_};
      rgba<u8>                  background_  {neutral_color()};



      [[nodiscard]] static constexpr rgba<u8> neutral_color() {
        return rgba<u8> {
          .r = 0,
          .g = 0,
          .b = 0,
          .a = 0
        };
      }
  };





  struct gif_rect {
    size_t x;
    size_t y;
    size_t width;
    size_t height;

    explicit gif_rect(const GifImageDesc& desc) :
      x{saturating_cast(desc.Left)}, y{saturating_cast(desc.Top)},
      width {saturating_cast(desc.Width)}, height{saturating_cast(desc.Height)}
    {}
  };





  void transfer_pixels_over_background(
      details::decoder&   decoder,
      const SavedImage&   img,
      const gif_palette&  palette,
      const pixel_buffer& background
  ) {
    gif_rect rect{img.ImageDesc};
    auto source = std::span{img.RasterBits, rect.width * rect.height};

    for (size_t y = 0; y < rect.y; ++y) {
      std::ranges::copy(background.row_bytes(y), decoder.target().row_bytes(y).begin());
      decoder.frame_mark_ready_until_line(y);
    }



    for (size_t y = rect.y; y < rect.y + rect.height; ++y) {
      auto row = decoder.target().row<rgba<u8>>(y);
      auto old = background.row<rgba<u8>>(y);


      auto it = std::copy_n(old.begin(), rect.x, row.begin());

      auto jt = old.begin() + rect.x;
      for (auto in: source.subspan((y - rect.y) * rect.width, rect.width)) {
        *it = palette.resolve_color(in);
        if (it->a != 0xff) {
          *it = *jt;
        }

        ++it; ++jt;
      }

      std::copy(jt, old.end(), it);

      decoder.frame_mark_ready_until_line(y);
    }



    for (size_t y = rect.y + rect.height; y < decoder.target().height(); ++y) {
      std::ranges::copy(background.row_bytes(y), decoder.target().row_bytes(y).begin());
      decoder.frame_mark_ready_until_line(y);
    }
  }





  [[nodiscard]] std::string counted_name(std::string_view prefix, size_t count) {
    if (count == 0) {
      return std::string{prefix};
    }
    return std::string{prefix} + std::to_string(count);
  }



  [[nodiscard]] std::string_view string_view_from_block(const ExtensionBlock& block) {
    if (block.ByteCount == 0 || block.Bytes == nullptr) {
      return {};
    }

    //NOLINTNEXTLINE(*-reinterpret-cast)
    return {reinterpret_cast<const char*>(block.Bytes), saturating_cast(block.ByteCount)};
  }



  [[nodiscard]] float pixel_aspect_ratio(GifByteType aspect) {
    if (aspect == 0) {
      return 1.f;
    }
    return (static_cast<float>(aspect) + 15.f) / 64.f;
  }








  class gif_decoder {
    public:
      explicit gif_decoder(details::decoder& decoder) :
        decoder_{&decoder},
        gif_    {decoder_->input()}
      {
        gif_.slurp();

        width_  = gif_->SWidth;  //NOLINT(*initializer)
        height_ = gif_->SHeight; //NOLINT(*initializer)

        decoder_->frame_total(gif_->ImageCount);
      }



      void decode() {
        fill_global_metadata();

        for (int i = 0; i < gif_->ImageCount; ++i) {
          decode_frame(gif_->SavedImages[i]); //NOLINT(*pointer-arithmetic)
        }
      }




    private:
      details::decoder*           decoder_;
      gif_file                    gif_;
      size_t                      width_;
      size_t                      height_;

      std::optional<pixel_buffer> background;



      void decode_frame(const SavedImage& img) {
        assert_frame_size(img);

        gif_meta meta{img};
        gif_palette palette{current_color_map(img), meta.alpha(), gif_->SBackGroundColor};

        auto& frame = decoder_->begin_frame(width_, height_, rgba<u8>::format());

        frame.source_info().color_model(color_model::palette);
        frame.source_info().color_model_format({
            data_source_format::u8,
            data_source_format::u8,
            data_source_format::u8,
            meta.alpha() ? data_source_format::index : data_source_format::none
        });

        frame.alpha_mode(get_preferred_alpha_mode());
        frame.duration  (meta.duration());


        if (decoder_->wants_pixel_transfer()) {
          decoder_->begin_pixel_transfer();

          if (!background) {
            background.emplace(width_, height_, rgba<u8>::format());
            for (size_t y = 0; y < background->height(); ++y) {
              std::ranges::fill(background->row<rgba<u8>>(y), palette.background());
            }
          }

          transfer_pixels_over_background(*decoder_, img, palette, *background);

          switch (meta.dispose_mode()) {
            case dispose::background: {
              gif_rect rect{img.ImageDesc};
              for (size_t y = 0; y < rect.y + rect.height; ++y) {
                auto row = background->row<rgba<u8>>(y).subspan(rect.x, rect.width);
                std::ranges::fill(row, palette.background());
              }
            } break;
            case dispose::previous:
              break;
            case dispose::leave_in_place:
              background = decoder_->target();
              break;
          }

          decoder_->finish_pixel_transfer();
        }

        decoder_->finish_frame();
      }



      [[nodiscard]] ColorMapObject& current_color_map(const SavedImage& img) const {
        if (img.ImageDesc.ColorMap != nullptr) {
          return *img.ImageDesc.ColorMap;
        }
        if (gif_->SColorMap != nullptr) {
          return *gif_->SColorMap;
        }
        throw decode_error{codec::gif, "unable to find color map"};
      }



      void assert_frame_size(const SavedImage& img) const {
        if (img.ImageDesc.Left + img.ImageDesc.Width > static_cast<int>(width_) ||
          img.ImageDesc.Top + img.ImageDesc.Height > static_cast<int>(height_)) {
          throw decode_error{codec::gif, "frame exceeds canvas bounds"};
        }
      }



      [[nodiscard]] alpha_mode get_preferred_alpha_mode() const {
        if (decoder_->output_format().alpha_mode().prefers(alpha_mode::premultiplied)) {
          return alpha_mode::premultiplied;
        }
        return alpha_mode::straight;
      }




      void fill_global_metadata() {
        std::vector<metadata::key_value> out {
          {"gif.par", std::to_string(pixel_aspect_ratio(gif_->AspectByte))}
        };

        size_t comment  {0};
        size_t plaintext{0};
#ifdef PIXGLOT_WITH_XMP
        size_t xmp      {0};
#endif

        for (const auto& block: std::span{gif_->ExtensionBlocks,
                                          saturating_cast(gif_->ExtensionBlockCount)}) {
          switch (block.Function) {
            case COMMENT_EXT_FUNC_CODE:
              out.emplace_back(counted_name("comment", comment++),
                              std::string{string_view_from_block(block)});

              break;
            case PLAINTEXT_EXT_FUNC_CODE:
              out.emplace_back(counted_name("plaintext", plaintext++),
                               std::string{string_view_from_block(block)});
              break;
#ifdef PIXGLOT_WITH_XMP
            case APPLICATION_EXT_FUNC_CODE:
              if (block.ByteCount >= 11) {
                auto all = string_view_from_block(block);
                auto data = all.substr(11);
                if (all.substr(0, 11) == "XMP DataXMP" &&
                    details::fill_xmp_metadata(data, *decoder_)) {

                  out.emplace_back("pixglot." + counted_name("xmp", xmp++) + ".raw",
                                   data);
                }
              }
              break;
#endif
          }
        }

        decoder_->image().metadata().append_move(out);
      }
  };

}



void decode_gif(details::decoder& dec) {
  gif_decoder gdec{dec};
  gdec.decode();
}

