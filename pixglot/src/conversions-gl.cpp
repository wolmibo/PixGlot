#include "pixglot/exception.hpp"
#include "pixglot/gl-texture.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/square-isometry.hpp"

#include <epoxy/gl.h>
#include <epoxy/gl_generated.h>



namespace {
  [[nodiscard]] GLuint generate_framebuffer() {
    GLuint name{0};
    glGenFramebuffers(1, &name);
    if (name == 0) {
      throw pixglot::base_exception{"unable to create glFramebuffer"};
    }
    return name;
  }



  class framebuffer {
    public:
      framebuffer(const pixglot::gl_texture& texture) :
        id_          {generate_framebuffer()},
        binding_lock_{&id_}
      {
        glBindFramebuffer(GL_FRAMEBUFFER, id_);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, texture.id(), 0);

        static constexpr std::array<GLenum, 1> draw_buffers = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(draw_buffers.size(), draw_buffers.data());

        if (auto res = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            res != GL_FRAMEBUFFER_COMPLETE) {
          throw pixglot::base_exception{"failed to initialize glFramebuffer"};
        }
      }



    private:
      GLuint id_{0};

      struct framebuffer_deleter {
        void operator()(GLuint* /*placeholder*/) {
          glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
      };

      std::unique_ptr<GLuint, framebuffer_deleter> binding_lock_;
  };



  class shader {
    public:
      shader(const shader&)            = delete;
      shader(shader&&)                 = delete;
      shader& operator=(const shader&) = delete;
      shader& operator=(shader&&)      = delete;

      shader(GLenum type, std::string_view source) :
        shader_{glCreateShader(type)} {
        GLint length       = source.length();
        const GLchar* data = source.data();

        glShaderSource(shader_, 1, &data, &length);
        glCompileShader(shader_);

        GLint status{GL_FALSE};
        glGetShaderiv(shader_, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE || shader_ == 0) {
          throw pixglot::base_exception{"unable to compile shader"};
        }
      }

      [[nodiscard]] GLuint get() const { return shader_; }

      ~shader() { glDeleteShader(shader_); }

    private:
      GLuint shader_{0};
  };



  class program {
    public:
      program(const program&)            = delete;
      program(program&&)                 = delete;
      program& operator=(const program&) = delete;
      program& operator=(program&&)      = delete;

      program(std::string_view vs, std::string_view fs) :
        program_{glCreateProgram()} {
        shader vertex  {GL_VERTEX_SHADER,   vs};
        glAttachShader(program_, vertex.get());

        shader fragment{GL_FRAGMENT_SHADER, fs};
        glAttachShader(program_, fragment.get());

        glLinkProgram(program_);

        GLint status{GL_FALSE};
        glGetProgramiv(program_, GL_LINK_STATUS, &status);
        if (status == GL_FALSE || program_ == 0) {
          glDeleteProgram(program_);
          throw pixglot::base_exception{"unable to link program"};
        }

        glDetachShader(program_, vertex.get());
        glDetachShader(program_, fragment.get());
      }

      void use() const { glUseProgram(program_); }

      ~program() { glDeleteProgram(program_); }

    private:
      GLuint program_{0};
  };
}



namespace {
constexpr std::string_view vertex_shader = R"(
#version 450 core

layout (location=0) in vec4 position;
layout (location=0) uniform mat4 transform;

out vec2 uvCoord;

void main() {
  uvCoord = vec2(0.5, -0.5) * position.xy + vec2(0.5, 0.5);

  gl_Position = transform * position;
}
)";



constexpr std::string_view fragment_shader = R"(
#version 450 core

out vec4 fragColor;

in vec2 uvCoord;
uniform sampler2D textureSampler;

layout (location=1) uniform float exponent;

void main() {
  vec4 source_color = texture(textureSampler, uvCoord);

  fragColor = pow(source_color, vec4(exponent, exponent, exponent, 1.f));
}
)";
}



namespace pixglot::details {
  void convert(
      gl_texture&     texture,
      pixel_format    target_format,
      int             premultiply,
      float           gamma_diff,
      square_isometry transform
  ) {
    if (transform == square_isometry::identity
        && std::abs(gamma_diff - 1.f) < 1e-7
        && premultiply == 0
        && target_format == texture.format()) {
      return;
    }

    size_t width {texture.width()};
    size_t height{texture.height()};

    if (flips_xy(transform)) {
      std::swap(width, height);
    }

    gl_texture target{width, height, target_format};

    if (false) {
      framebuffer buffer{target};
      program     program{vertex_shader, fragment_shader};
    }

    texture = std::move(target);
  }
}
