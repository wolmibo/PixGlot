#include "pixglot/details/exif.hpp"
#include "pixglot/details/decoder.hpp"
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



    private:

      static constexpr std::array<std::byte, 2> little_e{std::byte{'I'}, std::byte{'I'}};
      static constexpr std::array<std::byte, 2> big_e   {std::byte{'M'}, std::byte{'M'}};



      std::span<const std::byte> buffer_;
      std::endian                byte_order_{std::endian::native};

      square_isometry            orientation_{};


#if 0
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
#endif


      enum class attribute : uint16_t {
        orientation = 0x112,
      };



#if 0
      using ifd_value = std::variant<
        std::array<uint8_t, 4>,
        std::array<uint16_t, 2>,
        uint32_t,
        int32_t,
        std::pair<value_type, size_t>
      >;

      struct ifd_entry {
        attribute tag;
        uint32_t  count;
        ifd_value value;
      };





      [[nodiscard]] ifd_entry read_ifd_entry(size_t offset) {
        return ifd_entry{
          .tag   = static_cast<attribute>(int_at<uint16_t>(offset)),
          .count = int_at<uint32_t>(offset + 4),
          .value = read_ifd_value(offset + 8, int_at<uint16_t>(offset + 2)),
        };
      }



      [[nodiscard]] ifd_value read_ifd_value(size_t offset, uint16_t type) {
        switch (static_cast<value_type>(type)) {
          case value_type::byte:
            return std::array<uint8_t, 4> {
              int_at<uint8_t>(offset + 3),
              int_at<uint8_t>(offset + 2),
              int_at<uint8_t>(offset + 1),
              int_at<uint8_t>(offset + 0)
            };

          case value_type::ushort:
            return std::array<uint16_t, 2> {
              int_at<uint16_t>(offset + 2),
              int_at<uint16_t>(offset + 0)
            };

          case value_type::ulong:
            return int_at<uint32_t>(offset);
          case value_type::slong:
            return int_at<int32_t>(offset);

          case value_type::ascii:
          case value_type::rational:
          case value_type::undefined:
          case value_type::srational:
          default: {
            auto off = int_at<uint32_t>(offset);
            if (off >= buffer_.size()) {
              off = 0;
            }
            return std::make_pair(static_cast<value_type>(type), off);
          }
        }
      }
#endif


      void load_entries(size_t offset) {
        while (offset != 0) {
          auto count = int_at<uint16_t>(offset);
          offset += 2;
          for (uint16_t i = 0; i < count; ++i) {
            if (static_cast<attribute>(int_at<uint16_t>(offset)) ==
                attribute::orientation) {
              orientation_ = square_isometry_from_tiff(int_at<uint16_t>(offset + 8));
            }
            offset += 12;
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

        load_entries(int_at<uint32_t>(4));
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






bool pixglot::details::fill_exif_metadata(
    std::span<const std::byte> buffer,
    metadata&                  /*meta*/,
    decoder&                   dec,
    square_isometry*           orientation
) {
  try {
    exif_decoder exif{buffer};

    *orientation = exif.orientation();
    return true;

  } catch (std::exception& ex) {
    dec.warn(std::string{"unable to parse exif: "} + ex.what());
  } catch (...) {
    dec.warn("unablt to parse exif: unknown error");
  }
  return false;
}





bool pixglot::details::is_exif(std::span<const std::byte> buffer) {
  return buffer.size() > magic.size()
    && std::ranges::equal(magic, buffer.subspan(0, magic.size()));
}
