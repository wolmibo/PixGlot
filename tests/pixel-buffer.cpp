#include "common.hpp"

#include <tuple>

#include <pixglot/pixel-buffer.hpp>

using namespace pixglot;



int main() {
  pixel_buffer buff{1024, 1024};

  id_assert_eq(buff.width(),  1024u);
  id_assert_eq(buff.height(), 1024u);

  id_assert_eq(buff.stride(), 4096u);



  try {
    std::ignore = buff.row<gray<f32>>(512);
    exit(1);
  } catch (const bad_pixel_format& ex) {
    id_assert_eq(*ex.expected(), buff.format());
  }



  try {
    std::ignore = buff.row<pixel_format_to<pixel_format{}>::type>(buff.height());
    exit(1);
  } catch (const index_out_of_range& ex) {
    id_assert_eq(ex.bound(), buff.height());
  }



  size_t count{0};
  for (auto row: buff.rows<rgba<u8>>()) {
    count++;
    id_assert_eq(row.size(), buff.width());
  }
  id_assert_eq(count, buff.height());

  id_assert(pixel_buffer::padding() >= 4);
}
