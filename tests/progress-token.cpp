#include "common.hpp"

#include <thread>

#include <pixglot/frame.hpp>
#include <pixglot/progress-token.hpp>

using namespace pixglot;



void test_sync() {
  progress_token pt;

  id_assert(!pt.finished());
  id_assert_eq(pt.progress(), 0.f);

  auto pat = pt.access_token();

  id_assert(pat.progress(0.5f));
  id_assert_eq(pt.progress(), 0.5f);
  id_assert(!pt.finished());

  pt.stop();
  id_assert(!pat.progress(0.6f));
  id_assert(!pat.proceed());
  id_assert_eq(pt.progress(), 0.6f);
  id_assert(!pt.finished());


  pat.finish();
  id_assert(pt.finished());
}





void test_disconnect() {
  progress_token pt;
  auto pat = pt.access_token();

  std::ignore = pat.progress(0.3f);
  id_assert_eq(pt.progress(), 0.3f);

  auto pat2 = pt.access_token();

  // pat is no longer connected to pt
  std::ignore = pat.progress(0.4f);
  id_assert_eq(pt.progress(), 0.3f);

  std::ignore = pat2.progress(0.5f);
  id_assert_eq(pt.progress(), 0.5f);
}





void test_callback() {
  int counter {0};
  frame f{
    .pixels = pixel_buffer{1, 1}
  };

  auto inc_counter = [&counter](frame&) { counter++; };

  progress_token pt;
  auto pat = pt.access_token();

  pt.frame_callback(inc_counter);
  id_assert_eq(counter, 0);
  std::ignore = pat.append_frame(f);
  id_assert_eq(counter, 1);

  pt.frame_callback();
  std::ignore = pat.append_frame(f);
  id_assert_eq(counter, 1);

  pt.frame_callback(inc_counter);
  std::ignore = pat.append_frame(f);
  id_assert_eq(counter, 2);

  auto pat2 = pt.access_token();
  std::ignore = pat.append_frame(f);
  id_assert_eq(counter, 2);

  std::ignore = pat2.append_frame(f);
  id_assert_eq(counter, 3);
}





void test_async() {
  int counter {0};

  frame f{
    .pixels = pixel_buffer{1, 1}
  };

  auto inc_counter = [&counter](frame&) { counter++; };

  progress_token pt;
  auto pat = pt.access_token();
  pt.frame_callback(inc_counter);

  id_assert_eq(counter, 0);
  id_assert(std::abs(pt.progress() - 0.f) < 1e-4f);
  id_assert(!pt.finished());

  std::jthread async([&pat, &f]{
    for (int i = 0; i < 100; ++i) {
      std::this_thread::sleep_for(std::chrono::microseconds(10));

      std::ignore = pat.progress(static_cast<float>(i+1) / 100.f);
      std::ignore = pat.append_frame(f);
    }
    pat.finish();
  });

  while (!pt.finished()) {
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }

  id_assert_eq(counter, 100);
  id_assert(std::abs(pt.progress() - 1.f) < 1e-4f);
  id_assert(pt.finished());
}





int main() {
  test_sync();
  test_disconnect();
  test_callback();
  test_async();
}
