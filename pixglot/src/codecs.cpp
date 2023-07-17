#include "pixglot/codecs.hpp"
#include "pixglot/codecs-magic.hpp"
#include "pixglot/reader.hpp"

using namespace pixglot;



std::optional<codec> pixglot::determine_codec(
  [[maybe_unused]] std::span<const std::byte> input
) {
  if (false) {}
#ifdef PIXGLOT_WITH_JPEG
  else if (magic<codec::jpeg>::check(input)) return codec::jpeg;
#endif
#ifdef PIXGLOT_WITH_PNG
  else if (magic<codec::png>::check(input)) return codec::png;
#endif
#ifdef PIXGLOT_WITH_AVIF
  else if (magic<codec::avif>::check(input)) return codec::avif;
#endif
#ifdef PIXGLOT_WITH_EXR
  else if (magic<codec::exr>::check(input)) return codec::exr;
#endif
#ifdef PIXGLOT_WITH_PPM
  else if (magic<codec::ppm>::check(input)) return codec::ppm;
#endif
#ifdef PIXGLOT_WITH_WEBP
  else if (magic<codec::webp>::check(input)) return codec::webp;
#endif
#ifdef PIXGLOT_WITH_GIF
  else if (magic<codec::gif>::check(input)) return codec::gif;
#endif
#ifdef PIXGLOT_WITH_JXL
  else if (magic<codec::jxl>::check(input)) return codec::jxl;
#endif

  return {};
}



std::optional<codec> pixglot::determine_codec(
  const std::filesystem::path& p
) {
  try {
    reader r{p};

    std::array<std::byte, recommended_magic_size> buffer{};
    size_t count = r.peek(buffer);

    std::span<const std::byte> magic{buffer.data(), count};

    return determine_codec(magic);
  } catch (...) {
    return {};
  }
}





std::string_view pixglot::stringify(codec c) {
  switch (c) {
    case codec::jpeg: return "jpeg";
    case codec::png:  return "png";
    case codec::avif: return "avif";
    case codec::exr:  return "exr";
    case codec::ppm:  return "ppm";
    case codec::webp: return "webp";
    case codec::gif:  return "gif";
    case codec::jxl:  return "jxl";
  }

  return "<invalid codec>";
}



std::vector<std::string_view> pixglot::mime_types(codec c) {
  switch (c) {
    case codec::jpeg: return {"image/jpeg"};
    case codec::png:  return {"image/png"};
    case codec::avif: return {"image/avif"};
    case codec::exr:  return {"image/x-exr"};
    case codec::ppm:  return {
      "image/x-portable-bitmap",
      "image/x-portable-graymap",
      "image/x-portable-pixmap"
    };
    case codec::webp: return {"image/webp"};
    case codec::gif:  return {"image/gif"};
    case codec::jxl:  return {"image/jxl"};
  }
  return {};
}
