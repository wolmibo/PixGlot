#include "../decoder.hpp"
#include "pixglot/frame.hpp"
#include "pixglot/pixel-format-conversion.hpp"

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

        if (alpha_index && *alpha_index < valid_colors_.size()) {
          valid_colors_[*alpha_index] = neutral_color();
        }



        if (!alpha_index && background_index >= 0
            && static_cast<size_t>(background_index) < valid_colors_.size()) {
          background_ = valid_colors_[background_index];
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
      pixel_buffer&      target,
      const SavedImage&  img,
      const gif_palette& palette
  ) {
    gif_rect rect{img.ImageDesc};
    auto source = std::span{img.RasterBits, rect.width * rect.height};

    for (size_t y = 0; y < rect.y; ++y) {
      std::ranges::fill(target.row<rgba<u8>>(y), palette.background());
    }



    for (size_t y = rect.y; y < rect.y + rect.height; ++y) {
      auto row = target.row<rgba<u8>>(y);
      auto src = source.subspan((y - rect.y) * rect.width, rect.width);



      auto it = std::fill_n(row.begin(), rect.x, palette.background());

      it = std::transform(src.begin(), src.end(), it,
          [palette](u8 in){ return palette.resolve_color(in); });

      std::fill(it, row.end(), palette.background());
    }



    for (size_t y = rect.y + rect.height; y < target.height(); ++y) {
      std::ranges::fill(target.row<rgba<u8>>(y), palette.background());
    }
  }





  void transfer_pixels_over_buffer(
      pixel_buffer&       target,
      const SavedImage&   img,
      const gif_palette&  palette,
      const pixel_buffer& background
  ) {
    gif_rect rect{img.ImageDesc};
    auto source = std::span{img.RasterBits, rect.width * rect.height};

    for (size_t y = 0; y < rect.y; ++y) {
      std::ranges::copy(background.row_bytes(y), target.row_bytes(y).begin());
    }



    for (size_t y = rect.y; y < rect.y + rect.height; ++y) {
      auto row = target.row<rgba<u8>>(y);
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
    }



    for (size_t y = rect.y + rect.height; y < target.height(); ++y) {
      std::ranges::copy(background.row_bytes(y), target.row_bytes(y).begin());
    }
  }




  class gif_decoder : public decoder {
    public:
      explicit gif_decoder(decoder&& parent) :
        decoder{std::move(parent)},
        gif    {input()}
      {
        gif.slurp();

        width  = gif->SWidth;  //NOLINT(*initializer)
        height = gif->SHeight; //NOLINT(*initializer)
      }



      void decode() {
        for (int i = 0; i < gif->ImageCount; ++i) {
          append_frame(decode_frame(gif->SavedImages[i])); //NOLINT(*pointer-arithmetic)
          progress(i, gif->ImageCount);
        }
      }




    private:
      gif_file gif;
      size_t   width;
      size_t   height;

      std::optional<pixel_buffer> old_pixels;



      [[nodiscard]] frame decode_frame(const SavedImage& img) {
        assert_frame_size(img);

        gif_meta meta{img};
        gif_palette palette{current_color_map(img), meta.alpha(), gif->SBackGroundColor};

        pixel_buffer buffer{width, height, rgba<u8>::format()};
        if (old_pixels) {
          transfer_pixels_over_buffer(buffer, img, palette, *old_pixels);
        } else {
          transfer_pixels_over_background(buffer, img, palette);
        }

        switch (meta.dispose_mode()) {
          case dispose::background:
            old_pixels = {};
            break;
          case dispose::previous:
            break;
          case dispose::leave_in_place:
            old_pixels = buffer;
            break;
        }

        output_image().animated = output_image().animated
          || (meta.duration() > std::chrono::microseconds{0});

        return frame {
          .pixels      = std::move(buffer),
          .orientation = square_isometry::identity,

          .alpha       = get_preferred_alpha_mode(),
          .gamma       = gamma_s_rgb,
          .endianess   = std::endian::native,

          .duration    = meta.duration()
        };
      }



      [[nodiscard]] ColorMapObject& current_color_map(const SavedImage& img) const {
        if (img.ImageDesc.ColorMap != nullptr) {
          return *img.ImageDesc.ColorMap;
        }
        if (gif->SColorMap != nullptr) {
          return *gif->SColorMap;
        }
        throw decode_error{codec::gif, "unable to find color map"};
      }



      void assert_frame_size(const SavedImage& img) const {
        if (img.ImageDesc.Left + img.ImageDesc.Width > static_cast<int>(width) ||
          img.ImageDesc.Top + img.ImageDesc.Height > static_cast<int>(height)) {
          throw decode_error{codec::gif, "frame exceeds canvas bounds"};
        }
      }



      [[nodiscard]] alpha_mode get_preferred_alpha_mode() const {
        if (format_out().alpha.prefers(alpha_mode::premultiplied)) {
          return alpha_mode::premultiplied;
        }
        return alpha_mode::straight;
      }
  };

}



image decode_gif(decoder&& dec) {
  gif_decoder gdec{std::move(dec)};
  gdec.decode();
  return gdec.finalize_image();
}

