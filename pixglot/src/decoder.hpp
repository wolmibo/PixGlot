#ifndef PIXGLOT_DECODER_HPP_INCLUDED
#define PIXGLOT_DECODER_HPP_INCLUDED

#include "pixglot/image.hpp"
#include "pixglot/output-format.hpp"
#include "pixglot/progress-token.hpp"
#include "pixglot/reader.hpp"

#include <algorithm>
#include <string_view>



using namespace pixglot;

enum class abort_decoding{};

class decoder {
  public:
    decoder(reader& r, progress_access_token pat, output_format fmt) :
      reader_{&r},
      pat_{std::move(pat)},
      output_image_{},
      format_{fmt}
    {}



    [[nodiscard]] image finalize_image() {
      pat_.finish();
      return std::move(output_image_);
    }



  protected:
    void append_frame(frame f) {
      make_format_compatible(f, format_);

      output_image_.frames.emplace_back(std::move(f));
      if (!pat_.append_frame(output_image_.frames.back())) {
        throw abort_decoding{};
      }
    }



    void progress(float f) {
      if (!pat_.progress(f)) {
        throw abort_decoding{};
      }
    }



    void progress(size_t i, size_t n, size_t j = 0, size_t m = 1) {
      if (i > n || j > m || n * m == 0) {
        throw std::domain_error{"progress must be in the range [0, 1]"};
      }

      progress(
        (static_cast<float>(i) / static_cast<float>(n) + static_cast<float>(j))
        / static_cast<float>(m)
      );
    }



    void warn(std::string_view msg) {
      output_image_.warnings.emplace_back(msg);
    }



    [[nodiscard]] reader&              input()              { return *reader_;      }
    [[nodiscard]] const output_format& format_out()   const { return format_;       }
    [[nodiscard]] image&               output_image()       { return output_image_; }



  private:
    reader*               reader_;
    progress_access_token pat_;
    image                 output_image_;
    output_format         format_;
};


#endif // PIXGLOT_DECODER_HPP_INCLUDED
