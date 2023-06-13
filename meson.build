project(
  'pixglot',
  'cpp',
  default_options: [
    #'cpp_std=c++20',
    'warning_level=3'
  ],
  version: '0.0.1',
  license: 'MIT'
)

# Use this until meson supports C++23 (gcc is anyways the only supported compiler)
cppargs = ['-std=c++2b']

subdir('pixglot')

if get_option('tests')
  subdir('tests')
endif

if get_option('examples')
  subdir('examples')
endif
