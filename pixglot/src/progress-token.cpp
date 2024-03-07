#include "pixglot/progress-token.hpp"

#include <atomic>

using namespace pixglot;



class progress_access_token::shared_state {
  public:
    std::atomic<float> progress{0.f};
    std::atomic<bool>  finished{false};
    std::atomic<bool>  proceed {true};

    std::atomic<bool>  upload       {false};
    std::atomic<bool>  flush_uploads{false};

    std::move_only_function<void(frame&)>            callback;
    std::move_only_function<void(const frame_view&)> callback_begin;
    std::mutex                                       callback_mutex;



  [[nodiscard]] shared_state take_away() {
    std::lock_guard lock{callback_mutex};

    return shared_state {
      .progress{progress.load()},
      .finished{finished.load()},
      .proceed {proceed.load()},

      .upload       {upload.load()},
      .flush_uploads{flush_uploads.load()},

      .callback      {std::exchange(callback, {})},
      .callback_begin{std::exchange(callback_begin, {})},
      .callback_mutex{},
    };
  }
};





progress_access_token::progress_access_token(progress_access_token&&) noexcept = default;

progress_access_token&
progress_access_token::operator=(progress_access_token&&) noexcept = default;

progress_access_token::~progress_access_token() = default;



progress_access_token::progress_access_token() :
  state_{std::make_shared<shared_state>()}
{}





void progress_access_token::finish() {
  state_->finished = true;
}



bool progress_access_token::proceed() const {
  return state_->proceed;
}



bool progress_access_token::progress(float f) {
  state_->progress = f;
  return proceed();
}



bool progress_access_token::upload_requested() {
  return state_->upload.exchange(false);
}



bool progress_access_token::flush_uploads() const {
  return state_->flush_uploads;
}



namespace {
  template<typename Fnc, typename... Args>
  void invoke_save(Fnc&& f, Args&& ...args) {
    if (std::forward<Fnc>(f)) {
      std::invoke(std::forward<Fnc>(f), std::forward<Args>(args)...);
    }
  }
}



bool progress_access_token::append_frame(frame& f) {
  std::lock_guard lock{state_->callback_mutex};
  invoke_save(state_->callback, f);
  return proceed();
}



bool progress_access_token::begin_frame(const frame_view& f) {
  std::lock_guard lock{state_->callback_mutex};
  invoke_save(state_->callback_begin, f);
  return proceed();
}






progress_token::progress_token(progress_token&&) noexcept = default;
progress_token& progress_token::operator=(progress_token&&) noexcept = default;

progress_token::~progress_token() = default;



progress_token::progress_token() :
  state_{new progress_access_token::shared_state()}
{}





bool progress_token::finished() const {
  return state_->finished;
}



float progress_token::progress() const {
  return state_->progress;
}



void progress_token::stop() {
  state_->proceed = false;
}





progress_access_token progress_token::access_token() {
  //NOLINTNEXTLINE(*owning-memory)
  state_.reset(new progress_access_token::shared_state(state_->take_away()));
  return progress_access_token{state_};
}





void progress_token::frame_callback(std::move_only_function<void(frame&)> callback) {
  std::lock_guard lock{state_->callback_mutex};
  state_->callback = std::move(callback);
}



void progress_token::frame_begin_callback(
    std::move_only_function<void(const frame_view&)> callback
) {
  std::lock_guard lock{state_->callback_mutex};
  state_->callback_begin = std::move(callback);
}





void progress_token::upload_available() const {
  state_->upload = true;
}



void progress_token::flush_uploads(bool flush) const {
  state_->flush_uploads = flush;
}
