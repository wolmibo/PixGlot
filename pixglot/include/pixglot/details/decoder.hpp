// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_DETAILS_DECODER_HPP_INCLUDED
#define PIXGLOT_DETAILS_DECODER_HPP_INCLUDED

#include "pixglot/image.hpp"
#include "pixglot/output-format.hpp"
#include "pixglot/progress-token.hpp"
#include "pixglot/reader.hpp"



namespace pixglot::details {

class decoder {
  public:
    enum class abort{};

    decoder(reader&, progress_access_token, output_format);

    [[nodiscard]] image finish();



    void   begin_frame(frame);
    frame& current_frame() { return current_frame_.value(); }
    void   finish_frame();

    void   finish_frame(frame);



    void progress(float);
    void progress(size_t, size_t, size_t = 0, size_t = 1);

    void warn(std::string_view);



    [[nodiscard]] reader&                       input()               { return *reader_; }
    [[nodiscard]] const pixglot::output_format& output_format() const { return format_;  }



  private:
    reader*                reader_;
    progress_access_token  token_;
    image                  image_;
    pixglot::output_format format_;

    std::optional<frame>   current_frame_;
};

}

#endif // PIXGLOT_DETAILS_DECODER_HPP_INCLUDED
