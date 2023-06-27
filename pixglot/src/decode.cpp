#include "config.hpp"
#include "decoder.hpp"
#include "pixglot/decode.hpp"
#include "pixglot/codecs-magic.hpp"

#include <utility>

using namespace pixglot;



#ifdef PIXGLOT_WITH_JPEG
[[nodiscard]] image decode_jpeg(decoder&&);
#endif
#ifdef PIXGLOT_WITH_JPEG
[[nodiscard]] image decode_png(decoder&&);
#endif
#ifdef PIXGLOT_WITH_AVIF
[[nodiscard]] image decode_avif(decoder&&);
#endif
#ifdef PIXGLOT_WITH_EXR
[[nodiscard]] image decode_exr(decoder&&);
#endif
#ifdef PIXGLOT_WITH_PPM
[[nodiscard]] image decode_ppm(decoder&&);
#endif
#ifdef PIXGLOT_WITH_WEBP
[[nodiscard]] image decode_webp(decoder&&);
#endif
#ifdef PIXGLOT_WITH_GIF
[[nodiscard]] image decode_gif(decoder&&);
#endif
#ifdef PIXGLOT_WITH_JXL
[[nodiscard]] image decode_jxl(decoder&&);
#endif



image pixglot::decode(
    reader&               r,
    codec                 c,
    progress_access_token pat,
    output_format         fmt
) {
  try {
    decoder dec{r, std::move(pat), fmt};

    switch (c) {
#ifdef PIXGLOT_WITH_JPEG
      case codec::jpeg: return decode_jpeg(std::move(dec));
#endif
#ifdef PIXGLOT_WITH_PNG
      case codec::png:  return decode_png(std::move(dec));
#endif
#ifdef PIXGLOT_WITH_AVIF
      case codec::avif: return decode_avif(std::move(dec));
#endif
#ifdef PIXGLOT_WITH_EXR
      case codec::exr:  return decode_exr(std::move(dec));
#endif
#ifdef PIXGLOT_WITH_PPM
      case codec::ppm:  return decode_ppm(std::move(dec));
#endif
#ifdef PIXGLOT_WITH_WEBP
      case codec::webp: return decode_webp(std::move(dec));
#endif
#ifdef PIXGLOT_WITH_GIF
      case codec::gif:  return decode_gif(std::move(dec));
#endif
#ifdef PIXGLOT_WITH_JXL
      case codec::jxl:  return decode_jxl(std::move(dec));
#endif
      default: break;
    }
  } catch (pixglot::base_exception&) {
    throw;
  } catch (std::exception& ex) {
    throw base_exception{std::string{"fatal error: "} + ex.what() +
      "\n(this is most likely a bug or a problem outside of the control of pixglot)"};
  }

  throw no_decoder{};
}



image pixglot::decode(reader& r, progress_access_token pat, output_format fmt) {
  std::array<std::byte, recommended_magic_size> buffer{};
  size_t count = r.peek(buffer);

  std::span<const std::byte> magic{buffer.data(), count};

  if (auto c = determine_codec(magic)) {
    return decode(r, *c, std::move(pat), fmt);
  }
  throw no_decoder{};
}



image pixglot::decode(reader&& r, progress_access_token pat, output_format fmt) {
  return decode(r, std::move(pat), fmt);
}



image pixglot::decode(
    reader&&              r,
    codec                 c,
    progress_access_token pat,
    output_format         fmt
) {
  return decode(r, c, std::move(pat), fmt);
}
