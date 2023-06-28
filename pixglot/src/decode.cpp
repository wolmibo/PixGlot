#include "config.hpp"
#include "pixglot/codecs-magic.hpp"
#include "pixglot/decode.hpp"
#include "pixglot/details/decoder.hpp"

#include <utility>

using namespace pixglot;



#ifdef PIXGLOT_WITH_JPEG
void decode_jpeg(details::decoder&);
#endif
#ifdef PIXGLOT_WITH_PNG
void decode_png(details::decoder&);
#endif
#ifdef PIXGLOT_WITH_AVIF
void decode_avif(details::decoder&);
#endif
#ifdef PIXGLOT_WITH_EXR
void decode_exr(details::decoder&);
#endif
#ifdef PIXGLOT_WITH_PPM
void decode_ppm(details::decoder&);
#endif
#ifdef PIXGLOT_WITH_WEBP
void decode_webp(details::decoder&);
#endif
/*#ifdef PIXGLOT_WITH_GIF
[[nodiscard]] image decode_gif(decoder&&);
#endif
#ifdef PIXGLOT_WITH_JXL
[[nodiscard]] image decode_jxl(decoder&&);
#endif*/



image pixglot::decode(
    reader&               r,
    codec                 c,
    progress_access_token pat,
    output_format         fmt
) {
  try {
    details::decoder dec{r, std::move(pat), fmt};

    switch (c) {
#ifdef PIXGLOT_WITH_JPEG
      case codec::jpeg: decode_jpeg(dec); break;
#endif
#ifdef PIXGLOT_WITH_PNG
      case codec::png:  decode_png(dec);  break;
#endif
#ifdef PIXGLOT_WITH_AVIF
      case codec::avif: decode_avif(dec); break;
#endif
#ifdef PIXGLOT_WITH_EXR
      case codec::exr:  decode_exr(dec);  break;
#endif
#ifdef PIXGLOT_WITH_PPM
      case codec::ppm:  decode_ppm(dec);  break;
#endif
#ifdef PIXGLOT_WITH_WEBP
      case codec::webp: decode_webp(dec); break;
#endif
/*#ifdef PIXGLOT_WITH_GIF
      case codec::gif:  return decode_gif(std::move(dec));
#endif
#ifdef PIXGLOT_WITH_JXL
      case codec::jxl:  return decode_jxl(std::move(dec));
#endif*/
      default: throw no_decoder{};
    }

    return dec.finish();
  } catch (pixglot::base_exception&) {
    throw;
  } catch (std::exception& ex) {
    throw base_exception{std::string{"fatal error: "} + ex.what() +
      "\n(this is most likely a bug or a problem outside of the control of pixglot)"};
  }
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
