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
    progress_access_token(const progress_access_token&) = delete;
    progress_access_token(progress_access_token&&) noexcept;

    progress_access_token& operator=(const progress_access_token&) = delete;
    progress_access_token& operator=(progress_access_token&&) noexcept;

    ~progress_access_token();



    progress_access_token();



    void finish();

    [[nodiscard]] bool proceed() const;

    [[nodiscard]] bool progress(float);
    [[nodiscard]] bool append_frame(frame&);



  private:
    class shared_state;
    std::shared_ptr<shared_state> state_;



    explicit progress_access_token(std::shared_ptr<shared_state> state) :
      state_{std::move(state)}
    {}
};





class progress_token {
  public:
    progress_token(const progress_token&) = delete;
    progress_token(progress_token&&) noexcept;

    progress_token& operator=(const progress_token&) = delete;
    progress_token& operator=(progress_token&&) noexcept;

    ~progress_token();

    progress_token();



    void frame_callback(std::move_only_function<void(frame&)> = {});

    [[nodiscard]] bool  finished() const;
    [[nodiscard]] float progress() const;

    void stop();



    [[nodiscard]] progress_access_token access_token();



  private:
    std::shared_ptr<progress_access_token::shared_state> state_;
};

}

#endif // PIXGLOT_PROGRESS_TOKEN_HPP_INCLUDED
