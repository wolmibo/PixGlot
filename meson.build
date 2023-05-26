project(
  'pixglot',
  'cpp',
  default_options: [
    #'cpp_std=c++20',
    'warning_level=3'
  ],
  version: '0.0.1'
)

# Use this until meson supports C++23 (gcc is anyways the only supported compiler)
add_global_arguments('-std=c++2b', language: 'cpp')

subdir('pixglot')

subdir('tests')
