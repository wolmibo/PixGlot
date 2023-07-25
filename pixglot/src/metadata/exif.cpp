#include "pixglot/details/exif.hpp"
#include "pixglot/details/decoder.hpp"
#include "pixglot/details/string-bytes.hpp"
#include "pixglot/details/tiff-orientation.hpp"
#include "pixglot/exception.hpp"
#include "pixglot/metadata.hpp"

#include <algorithm>

using namespace pixglot;
using namespace pixglot::details;



namespace {
  constexpr std::array<std::byte, 6> magic {
    std::byte{'E'}, std::byte{'x'}, std::byte{'i'}, std::byte{'f'},
    std::byte{0}, std::byte{0}
  };





  enum class value_type : uint16_t {
    byte      = 1,
    ascii     = 2,
    ushort    = 3,
    ulong     = 4,
    rational  = 5,
    undefined = 7,
    slong     = 8,
    srational = 10,
  };

  [[nodiscard]] size_t byte_size(value_type t) {
    using enum value_type;
    switch (t) {
      case byte:
      case ascii:
      case undefined:
        return 1;
      case ushort:
        return 2;
      case ulong:
      case slong:
        return 4;
      case rational:
      case srational:
        return 8;
    }
    return 0;
  }





  [[nodiscard]] char hex_char(int val) {
    if (val < 10) {
      return val + '0';
    }
    return val - 10 + 'a';
  }





  class exif_decoder {
    public:
      explicit exif_decoder(std::span<const std::byte> buffer)
        : buffer_{buffer}
      {
        if (!is_exif(buffer_)) {
          throw decode_error{codec::jpeg, "exif: invalid file format"};
        }
        buffer_ = buffer.subspan(magic.size());

        read_tiff();
      }



      [[nodiscard]] square_isometry orientation() const {
        return orientation_;
      }



      [[nodiscard]] std::span<metadata::key_value> entries() {
        return entries_;
      }



    private:
      static constexpr std::array<std::byte, 2> little_e{std::byte{'I'}, std::byte{'I'}};
      static constexpr std::array<std::byte, 2> big_e   {std::byte{'M'}, std::byte{'M'}};



      std::span<const std::byte>       buffer_;
      std::endian                      byte_order_{std::endian::native};

      square_isometry                  orientation_{};

      std::vector<metadata::key_value> entries_;


      enum class attribute : uint16_t {
        orientation = 0x112,
        exif_ifd    = 0x8769,
        gps_ifd     = 0x8825,
      };


      struct ifd_entry {
        attribute  tag;
        value_type type;
        uint32_t   count;
        size_t     offset;

        [[nodiscard]] size_t size() const { return byte_size(type) * count; }
      };


      std::optional<ifd_entry>         exif_ifd_;
      std::optional<ifd_entry>         gps_ifd_;





      [[nodiscard]] ifd_entry read_ifd_entry(size_t& offset) {
        offset += 12;

        return ifd_entry {
          .tag    = static_cast<attribute>(int_at<uint16_t>(offset - 12)),
          .type   = static_cast<value_type>(int_at<uint16_t>(offset - 10)),
          .count  = int_at<uint32_t>(offset - 8),
          .offset = offset - 4
        };
      }







      [[nodiscard]] std::string read_byte_array_at(size_t offset, size_t count) {
        std::string out;
        out.reserve(count * 4 - 1);
        for (size_t i = 0; i < count; ++i) {
          if (i != 0) { out.push_back(' '); }
          auto value = int_at<uint8_t>(offset++);
          out.push_back('x');
          out.push_back(hex_char(value >> 4));
          out.push_back(hex_char(value & 0xf));
        }
        return out;
      }


      template<typename T>
      [[nodiscard]] std::string read_int_array_at(size_t offset, size_t count) {
        if (count == 1) {
          return std::to_string(int_at<T>(offset));
        }
        std::string out = "[";
        for (size_t i = 0; i < count; ++i) {
          if (i != 0) { out += ", "; }
          out += std::to_string(int_at<T>(offset));
          offset += sizeof(T);
        }
        out.push_back(']');
        return out;
      }



      template<typename T>
      [[nodiscard]] std::string read_rational_array_at(size_t offset, size_t count) {
        if (count == 1) {
          return std::to_string(int_at<T>(offset));
        }
        std::string out = "[";
        for (size_t i = 0; i < count; ++i) {
          if (i != 0) { out += ", "; }
          out += std::to_string(int_at<T>(offset)) + '/'
               + std::to_string(int_at<T>(offset + sizeof(T)));

          offset += 2 * sizeof(T);
        }
        out.push_back(']');
        return out;
      }



      [[nodiscard]] std::string read_string_at(size_t offset, size_t count) {
        auto data = std::span{buffer_}.subspan(offset);
        return details::string_from(data.data(), std::min(data.size(), count));
      }





      [[nodiscard]] std::string deref_to_string(const ifd_entry& entry, size_t offset) {
        switch (entry.type) {
          case value_type::byte:
            return read_int_array_at<uint8_t>(offset, entry.count);
          case value_type::ascii:
            return read_string_at(offset, entry.count);
          case value_type::ushort:
            return read_int_array_at<uint16_t>(offset, entry.count);
          case value_type::ulong:
            return read_int_array_at<uint32_t>(offset, entry.count);
          case value_type::rational:
            return read_rational_array_at<uint32_t>(offset, entry.count);
          case value_type::undefined:
            return read_byte_array_at(offset, entry.count);
          case value_type::slong:
            return read_int_array_at<int32_t>(offset, entry.count);
          case value_type::srational:
            return read_rational_array_at<int32_t>(offset, entry.count);
          default:
            return "<value@" + std::to_string(offset) + ">";
        }
      }



      [[nodiscard]] std::string entry_to_string(const ifd_entry& entry) {
        if (entry.count == 0) {
          return "[]";
        }

        if (entry.size() == 0) {
          return "<unknown value>";
        }

        if (entry.size() > 4) {
          return deref_to_string(entry, int_at<uint32_t>(entry.offset));
        }

        return deref_to_string(entry, entry.offset);
      }



      void handle_entry(const ifd_entry& entry) {
        auto tag = std::to_underlying(entry.tag);

        std::string key = "exif.0x0000";
        key[ 7] = hex_char((tag >> 12) & 0xf);
        key[ 8] = hex_char((tag >>  8) & 0xf);
        key[ 9] = hex_char((tag >>  4) & 0xf);
        key[10] = hex_char((tag >>  0) & 0xf);

        entries_.emplace_back(std::move(key), entry_to_string(entry));
      }





      void load_entries(size_t offset, bool first) {
        while (offset != 0) {
          auto count = int_at<uint16_t>(offset);
          offset += 2;

          for (uint16_t i = 0; i < count; ++i) {
            auto entry = read_ifd_entry(offset);

            handle_entry(entry);

            if (first) {
              switch (entry.tag) {
                case attribute::orientation:
                  orientation_ = square_isometry_from_tiff(int_at<uint16_t>(entry.offset));
                  break;
                case attribute::exif_ifd:
                  exif_ifd_.emplace(entry);
                  break;
                case attribute::gps_ifd:
                  gps_ifd_.emplace(entry);
                  break;
              }
            }
          }
          offset = int_at<uint32_t>(offset);
        }
      }





      void read_tiff() {
        if (buffer_.size() < 8) {
          throw decode_error{codec::jpeg, "exif: incomplete tiff header"};
        }

        if (std::ranges::equal(buffer_.subspan(0, little_e.size()), little_e)) {
          byte_order_ = std::endian::little;
        } else if (std::ranges::equal(buffer_.subspan(0, big_e.size()), big_e)) {
          byte_order_ = std::endian::big;
        }

        if (int_at<uint16_t>(2) != 42) {
          throw decode_error{codec::jpeg, "exif: tiff: wrong byte order"};
        }

        load_entries(int_at<uint32_t>(4), true);

        if (exif_ifd_) {
          load_entries(int_at<uint32_t>(exif_ifd_->offset), false);
        }

        if (gps_ifd_) {
          load_entries(int_at<uint32_t>(gps_ifd_->offset), false);
        }
      }





      template<typename T>
      [[nodiscard]] T int_at(size_t offset) {
        if (offset > buffer_.size() - sizeof(T)) {
          throw decode_error{codec::jpeg, "exif: unexpected eof"};
        }

        if constexpr (sizeof(T) > 1) {
          T value{};
          std::ranges::copy(buffer_.subspan(offset, sizeof(T)),
              std::as_writable_bytes(std::span{&value, 1}).begin());

          if (byte_order_ == std::endian::native) {
            return value;
          }
          return std::byteswap(value);
        } else {
          return static_cast<T>(buffer_[offset]);
        }
      }
  };
}





std::string pixglot_metadata_find_unique_key(const metadata&,
    std::string_view, std::string_view);





void pixglot::details::fill_exif_metadata(
    std::span<const std::byte> buffer,
    metadata&                  meta,
    decoder&                   dec,
    square_isometry*           orientation
) {
  try {
    meta.emplace(pixglot_metadata_find_unique_key(meta, "pixglot.exif", ".rawSize"),
                 std::to_string(buffer.size()));

    exif_decoder exif{buffer};

    *orientation = exif.orientation();

    meta.append_move(exif.entries());

  } catch (std::exception& ex) {
    dec.warn(std::string{"unable to parse exif: "} + ex.what());
  } catch (...) {
    dec.warn("unablt to parse exif: unknown error");
  }
}





bool pixglot::details::is_exif(std::span<const std::byte> buffer) {
  return buffer.size() > magic.size()
    && std::ranges::equal(magic, buffer.subspan(0, magic.size()));
}
