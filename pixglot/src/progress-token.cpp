#include "pixglot/progress-token.hpp"

using namespace pixglot;



class progress_access_token::shared_state {
  public:
    std::atomic<float> progress{0.f};
    std::atomic<bool>  finished{false};
    std::atomic<bool>  proceed {true};

    std::move_only_function<void(frame&)> callback{};
    std::mutex                            callback_mutex{};



  [[nodiscard]] shared_state take_away() {
    std::lock_guard lock{callback_mutex};

    return shared_state {
      .progress{progress.load()},
      .finished{finished.load()},
      .proceed {proceed.load()},

      .callback      {std::exchange(callback, {})},
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





bool progress_access_token::append_frame(frame& f) {
  std::lock_guard lock{state_->callback_mutex};
  if (state_->callback) {
    state_->callback(f);
  }
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
