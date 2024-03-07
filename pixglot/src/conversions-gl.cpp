#include <epoxy/gl.h>
#include <epoxy/gl_generated.h>

#include "pixglot/exception.hpp"
#include "pixglot/details/int_cast.hpp"
#include "pixglot/gl-texture.hpp"
#include "pixglot/pixel-format.hpp"
#include "pixglot/square-isometry.hpp"



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
        auto length        = int_cast<GLint>(source.size());
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



  class plane {
    public:
      static constexpr std::array<GLushort, 6> indices {
        0, 1, 2,
        1, 2, 3,
      };

      static constexpr std::array<GLfloat, 16> vertices {
        -1.f,  1.f, 0.f, 1.f,
         1.f,  1.f, 0.f, 1.f,
        -1.f, -1.f, 0.f, 1.f,
         1.f, -1.f, 0.f, 1.f,
      };

      [[nodiscard]] static GLuint generate_vertex_array() {
        GLuint vao{0};
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        return vao;
      }

      [[nodiscard]] static GLuint generate_vertex_buffer() {
        GLuint vbo{0};
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
            std::span{vertices}.size_bytes(), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
        glEnableVertexAttribArray(0);

        return vbo;
      }

      [[nodiscard]] static GLuint generate_index_buffer() {
        GLuint ibo{0};
        glGenBuffers(1, &ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            std::span{indices}.size_bytes(), indices.data(), GL_STATIC_DRAW);
        return ibo;
      }



      plane(const plane&)            = delete;
      plane(plane&&)                 = delete;
      plane& operator=(const plane&) = delete;
      plane& operator=(plane&&)      = delete;

      plane() :
        vao_{generate_vertex_array()},
        vbo_{generate_vertex_buffer()},
        ibo_{generate_index_buffer()}
      {}

      ~plane() {
        glDeleteBuffers(1, &ibo_);
        glDeleteBuffers(1, &vbo_);
        glDeleteVertexArrays(1, &vao_);
      }

      void draw() const {
        glBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, nullptr);
      }



    private:
      GLuint vao_{0};
      GLuint vbo_{0};
      GLuint ibo_{0};
  };
}



namespace {
constexpr std::string_view vertex_shader = R"(
#version 450 core

layout (location=0) in vec4 position;
layout (location=0) uniform mat4 transform;

out vec2 uvCoord;

void main() {
  uvCoord = vec2(0.5, 0.5) * position.xy + vec2(0.5, 0.5);

  gl_Position = transform * position;
}
)";



constexpr std::string_view fragment_shader = R"(
#version 450 core

out vec4 fragColor;

in vec2 uvCoord;
uniform sampler2D textureSampler;

layout (location=1) uniform vec4 exponent;

void main() {
  vec4 source_color = texture(textureSampler, uvCoord);
  fragColor = pow(source_color, exponent);
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

    {
      framebuffer buffer{target};
      program     program{vertex_shader, fragment_shader};
      plane       quad;

      glViewport(0, 0, int_cast<GLsizei>(width), int_cast<GLsizei>(height));
      glClearColor(1.f, 0.f, 0.f, 1.f);
      glClear(GL_COLOR_BUFFER_BIT);

      program.use();

      auto matrix = square_isometry_to_mat4x4(transform);
      glUniformMatrix4fv(0, 1, GL_TRUE, matrix.data());

      glUniform4f(1, gamma_diff, gamma_diff, gamma_diff, 1.f);

      texture.bind();
      quad.draw();
    }

    texture = std::move(target);
  }
}
