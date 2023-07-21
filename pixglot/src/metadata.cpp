#include "pixglot/metadata.hpp"

#include <algorithm>
#include <vector>



using md = pixglot::metadata;

using key_value = std::pair<std::string, std::string>;



class md::impl {
  public:
    std::vector<key_value> entries_;
};




md::metadata(metadata&&) noexcept = default;
md& md::operator=(metadata&&) noexcept = default;

md::metadata(const metadata& rhs) :
  impl_{std::make_unique<impl>(*rhs.impl_)}
{}

md& md::operator=(const metadata& rhs) {
  if (&rhs == this) { return *this; }
  *impl_ = *rhs.impl_;
  return *this;
}

md::~metadata() = default;



md::metadata() :
  impl_{std::make_unique<impl>()}
{}





size_t md::size()  const { return impl_->entries_.size();  }
bool   md::empty() const { return impl_->entries_.empty(); }

//NOLINTBEGIN(*-pointer-arithmetic)
const md::key_value* md::begin() const { return impl_->entries_.data(); }
const md::key_value* md::end()   const { return begin() + size();       }

md::key_value* md::begin() { return impl_->entries_.data(); }
md::key_value* md::end()   { return begin() + size();       }
//NOLINTEND(*-pointer-arithmetic)





bool md::contains(std::string_view key) const {
  auto it = std::ranges::lower_bound(impl_->entries_, key, {}, &key_value::first);
  return (it != impl_->entries_.end() && it->first == key);
}



std::optional<std::string_view> md::find(std::string_view key) const {
  auto it = std::ranges::lower_bound(impl_->entries_, key, {}, &key_value::first);
  if (it != impl_->entries_.end() && it->first == key) {
    return it->second;
  }
  return {};
}



std::string& md::operator[](std::string_view key) {
  auto it = std::ranges::lower_bound(impl_->entries_, key, {}, &key_value::first);
  if (it != impl_->entries_.end() && it->first == key) {
    return it->second;
  }

  return impl_->entries_.emplace(it, std::string{key}, std::string{})->second;
}



const std::string& md::operator[](std::string_view key) const {
  auto it = std::ranges::lower_bound(impl_->entries_, key, {}, &key_value::first);
  if (it != impl_->entries_.end() && it->first == key) {
    return it->second;
  }

  throw std::out_of_range{"key not found in metadata"};
}





void md::emplace(std::string key, std::string value) {
  auto it = std::ranges::lower_bound(impl_->entries_, key, {}, &key_value::first);

  if (it != impl_->entries_.end() && it->first == key) {
    it->first = std::move(value);
    return;
  }

  impl_->entries_.emplace(it, std::move(key), std::move(value));
}



void md::append_move(std::span<key_value> list) {
  impl_->entries_.reserve(size() + list.size());

  for (auto& pair: list) {
    impl_->entries_.emplace_back(std::move(pair));
  }

  std::ranges::sort(impl_->entries_, {}, &key_value::first);
}
