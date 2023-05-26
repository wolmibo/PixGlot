// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_PROGRESS_TOKEN_HPP_INCLUDED
#define PIXGLOT_PROGRESS_TOKEN_HPP_INCLUDED

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>



namespace pixglot {

class frame;



class progress_access_token {
  friend class progress_token;

  public:
    progress_access_token(const progress_access_token&)     = delete;
    progress_access_token(progress_access_token&&) noexcept = default;

    progress_access_token& operator=(const progress_access_token&)     = delete;
    progress_access_token& operator=(progress_access_token&&) noexcept = default;

    ~progress_access_token() = default;



    progress_access_token() : state_{std::make_shared<shared_state>()} {}



    void finish() {
      state_->finished = true;
    }



    [[nodiscard]] bool proceed() const {
      return state_->proceed;
    }



    [[nodiscard]] bool progress(float f) {
      state_->progress = f;
      return proceed();
    }



    [[nodiscard]] bool append_frame(frame& f) {
      std::lock_guard lock{state_->callback_mutex};
      if (state_->callback) {
        state_->callback(f);
      }
      return proceed();
    }





  private:
    struct shared_state {
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

    std::shared_ptr<shared_state> state_;



    explicit progress_access_token(std::shared_ptr<shared_state> state) :
      state_{std::move(state)}
    {}
};





class progress_token {
  public:
    progress_token(const progress_token&)     = delete;
    progress_token(progress_token&&) noexcept = default;

    progress_token& operator=(const progress_token&)     = delete;
    progress_token& operator=(progress_token&&) noexcept = default;

    ~progress_token() = default;

    progress_token() : state_{new progress_access_token::shared_state()} {}



    [[nodiscard]] bool  finished() const { return state_->finished; }
    [[nodiscard]] float progress() const { return state_->progress; }



    void frame_callback(std::move_only_function<void(frame&)> callback = {}) {
      std::lock_guard lock{state_->callback_mutex};
      state_->callback = std::move(callback);
    }



    void stop() {
      state_->proceed = false;
    }



    progress_access_token access_token() {
      //NOLINTNEXTLINE(*owning-memory)
      state_.reset(new progress_access_token::shared_state(state_->take_away()));
      return progress_access_token{state_};
    }



  private:
    std::shared_ptr<progress_access_token::shared_state> state_;
};

}

#endif // PIXGLOT_PROGRESS_TOKEN_HPP_INCLUDED
