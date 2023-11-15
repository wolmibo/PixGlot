#include "pixglot/reader.hpp"

#include "pixglot/exception.hpp"

using namespace pixglot;



class reader::impl {
  public:
    FILE* fptr;



    impl(FILE* f) : fptr{f} {}

    impl(const impl&) = delete;
    impl(impl&&)      = delete;

    impl& operator=(const impl&) = delete;
    impl& operator=(impl&&) = delete;

    ~impl() {
      fclose(fptr); //NOLINT(*-owning-memory)
    }
};





reader::reader(reader&&) noexcept = default;

reader& reader::operator=(reader&&) noexcept = default;

reader::~reader() = default;



reader::reader(const std::filesystem::path& p) :
  //NOLINTNEXTLINE(*-owning-memory)
  impl_{std::make_unique<impl>(fopen(p.c_str(), "rb"))}
{
  if (impl_->fptr == nullptr) {
    throw no_stream_access{p.string()};
  }
}





size_t reader::read(std::span<std::byte> buffer) {
  if (impl_->fptr != nullptr) {
    return fread(buffer.data(), sizeof(std::byte), buffer.size(), impl_->fptr);
  }
  return 0;
}



size_t reader::peek(std::span<std::byte> buffer) const {
  if (impl_->fptr != nullptr) {
    size_t current = ftell(impl_->fptr);
    size_t count   = fread(buffer.data(), sizeof(std::byte), buffer.size(), impl_->fptr);
    fseek(impl_->fptr, current, SEEK_SET);
    return count;
  }
  return 0;
}





bool reader::skip(size_t count) {
  if (impl_->fptr != nullptr) {
    return fseek(impl_->fptr, count, SEEK_CUR) == 0;
  }
  return false;
}



size_t reader::position() const {
  return ftell(impl_->fptr);
}



size_t reader::size() const {
  size_t current = ftell(impl_->fptr);
  fseek(impl_->fptr, 0, SEEK_END);
  size_t size = ftell(impl_->fptr);
  fseek(impl_->fptr, current, SEEK_SET);
  return size;
}



bool reader::seek(size_t pos) {
  if (impl_->fptr != nullptr) {
    return fseek(impl_->fptr, pos, SEEK_SET) == 0;
  }
  return false;
}





bool reader::eof() const {
  return (impl_->fptr != nullptr) && feof(impl_->fptr) != 0;
}
