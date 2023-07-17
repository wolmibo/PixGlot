// Copyright (c) 2023 wolmibo
// SPDX-License-Identifier: MIT

#ifndef PIXGLOT_INPUT_PLANE_INFO_HPP_INCLUDED
#define PIXGLOT_INPUT_PLANE_INFO_HPP_INCLUDED

#include "pixglot/pixel-format.hpp"

#include <array>
#include <memory>



namespace pixglot {



enum class color_model {
  yuv,
  rgb,
  palette,
  value
};

[[nodiscard]] std::string_view stringify(color_model);
[[nodiscard]] std::string      to_string(color_model);





enum class chroma_subsampling {
  cs420 = 420,
  cs422 = 422,
  cs444 = 444,
};

[[nodiscard]] std::string_view stringify(chroma_subsampling);
[[nodiscard]] std::string      to_string(chroma_subsampling);





static constexpr int data_source_format_float_mask {1 << 30};

enum class data_source_format : int {
  index = -1,

  none = 0,

  u1  = 1,
  u2  = 2,
  u4  = 4,
  u8  = 8,
  u10 = 10,
  u12 = 12,
  u16 = 16,
  u32 = 32,

  f16 = 16 | data_source_format_float_mask,
  f32 = 32 | data_source_format_float_mask
};

[[nodiscard]] std::string_view stringify(data_source_format);
[[nodiscard]] std::string      to_string(data_source_format);

[[nodiscard]] bool             is_float (data_source_format);
[[nodiscard]] size_t           bit_count(data_source_format);

[[nodiscard]] data_format      to_data_format(data_source_format);






class input_plane_info {
  public:
    input_plane_info(const input_plane_info&);
    input_plane_info(input_plane_info&&) noexcept;
    input_plane_info& operator=(const input_plane_info&);
    input_plane_info& operator=(input_plane_info&&) noexcept;

    ~input_plane_info();

    input_plane_info();



    [[nodiscard]] pixglot::color_model                     color_model()        const;
    [[nodiscard]] const std::array<data_source_format, 4>& color_model_format() const;

    [[nodiscard]] chroma_subsampling                       subsampling()        const;



    [[nodiscard]] bool has_color() const;
    [[nodiscard]] bool has_alpha() const;



    void color_model       (pixglot::color_model);
    void color_model_format(std::array<data_source_format, 4>);
    void subsampling       (chroma_subsampling);



  private:
    class impl;
    std::unique_ptr<impl> impl_;
};

}

#endif // PIXGLOT_INPUT_PLANE_INFO_HPP_INCLUDED
