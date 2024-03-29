#include "pixglot/metadata.hpp"

#include "pixglot/details/tiff-orientation.hpp"
#include "pixglot/square-isometry.hpp"

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
  auto it = std::ranges::lower_bound(impl_->entries_, key, {}, &key_value::key);
  return (it != impl_->entries_.end() && it->key() == key);
}



std::optional<std::string_view> md::find(std::string_view key) const {
  auto it = std::ranges::lower_bound(impl_->entries_, key, {}, &key_value::key);
  if (it != impl_->entries_.end() && it->key() == key) {
    return it->value();
  }
  return {};
}





void md::emplace(std::string key, std::string value) {
  auto it = std::ranges::lower_bound(impl_->entries_, key, {}, &key_value::key);

  if (it != impl_->entries_.end() && it->key() == key) {
    *it = key_value{key, value};
    return;
  }

  impl_->entries_.emplace(it, std::move(key), std::move(value));
}



void md::append_move(std::span<key_value> list) {
  impl_->entries_.reserve(size() + list.size());

  for (auto& pair: list) {
    impl_->entries_.emplace_back(std::move(pair));
  }

  std::ranges::sort(impl_->entries_, {}, &key_value::key);
}



void md::append_move(std::vector<key_value> list) {
  append_move(std::span{list});
}





std::optional<pixglot::square_isometry> pixglot::orientation_from_metadata(
    const metadata& md
) {
  if (auto tiff = md.find("tiff:Orientation"); tiff && tiff->size() == 1) {
    return details::square_isometry_from_tiff(tiff->front());
  }

  return {};
}





std::string pixglot_metadata_find_unique_key(
    const pixglot::metadata& metadata,
    std::string_view         prefix,
    std::string_view         suffix
) {
  std::span<const pixglot::metadata::key_value> all_data{metadata};

  auto ideal = std::string{prefix} + std::string{suffix};

  auto it = std::ranges::lower_bound(all_data, ideal,
                                     {}, &pixglot::metadata::key_value::key);

  if (it == all_data.end() || it->key() != ideal) {
    return ideal;
  }

  auto jt = it;

  while (jt != all_data.end() &&
         jt->key().starts_with(prefix) && jt->key().ends_with(suffix)) {
    ++jt;
  }

  return std::string{prefix} + std::to_string(jt - it) + std::string{suffix};
}
