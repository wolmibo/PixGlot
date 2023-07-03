sources = [
  'src/codecs.cpp',
  'src/conversions.cpp',
  'src/conversions-cpu.cpp',
  'src/conversions-cpu-endian.cpp',
  'src/conversions-cpu-orientation.cpp',
  'src/conversions-gl.cpp',
  'src/decode.cpp',
  'src/decoder.cpp',
  'src/frame.cpp',
  'src/gl-texture.cpp',
  'src/output-format.cpp',
  'src/pixel-format.cpp',
  'src/square-isometry.cpp',
  'src/strings.cpp',
]


dependencies = [
  dependency('gl'),
  dependency('epoxy')
]

config = configuration_data()

jpeg = dependency('libjpeg', required: get_option('jpeg'))
if jpeg.found()
  sources      += 'src/codecs/jpeg.cpp'
  dependencies += jpeg
  config.set('PIXGLOT_WITH_JPEG', 1)
endif

png = dependency('libpng', required: get_option('png'))
if png.found()
  sources      += 'src/codecs/png.cpp'
  dependencies += png
  config.set('PIXGLOT_WITH_PNG', 1)
endif

avif = dependency('libavif', required: get_option('avif'))
if avif.found()
  sources      += 'src/codecs/avif.cpp'
  dependencies += avif
  config.set('PIXGLOT_WITH_AVIF', 1)
endif

exr = dependency('OpenEXR', required: get_option('exr'))
if exr.found()
  sources      += 'src/codecs/exr.cpp'
  dependencies += exr
  config.set('PIXGLOT_WITH_EXR', 1)
endif

if get_option('ppm')
  sources      += 'src/codecs/ppm.cpp'
  config.set('PIXGLOT_WITH_PPM', 1)
endif

webp = dependency('libwebpdemux', required: get_option('webp'))
if webp.found()
  sources      += 'src/codecs/webp.cpp'
  dependencies += webp
  config.set('PIXGLOT_WITH_WEBP', 1)
endif

jxl = dependency('libjxl', required: get_option('jxl'))
if jxl.found()
  sources      += 'src/codecs/jxl.cpp'
  dependencies += jxl
  config.set('PIXGLOT_WITH_JXL', 1)
endif

cpp = meson.get_compiler('cpp')
giflib = cpp.find_library('gif', required: get_option('gif'))
if giflib.found()
  sources      += 'src/codecs/gif.cpp'
  dependencies += giflib
  config.set('PIXGLOT_WITH_GIF', 1)
endif


cfg = configure_file(output: 'config.hpp', configuration: config)



install_project = not meson.is_subproject() or get_option('install_as_subproject')

if meson.is_subproject()
  summary('install', install_project)
endif



pixglot_lib = library(
  'pixglot',
  sources,
  cpp_args:            cppargs,
  dependencies:        dependencies,
  install:             install_project,
  include_directories: ['include', '.']
)



if install_project
  install_headers(cfg, subdir: 'pixglot')
  subdir('include')

  pkg = import('pkgconfig')
  pkg.generate(pixglot_lib)
endif



pixglot_dep = declare_dependency(
  include_directories: ['include', '.'],
  dependencies:        dependencies,
  link_with:           pixglot_lib
)
