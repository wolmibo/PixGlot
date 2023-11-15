sources = [
  'src/codecs.cpp',
  'src/conversions.cpp',
  'src/conversions-cpu-endian.cpp',
  'src/conversions-gl.cpp',
  'src/decode.cpp',
  'src/decoder.cpp',
  'src/frame.cpp',
  'src/frame-source-info.cpp',
  'src/gl-texture.cpp',
  'src/image.cpp',
  'src/metadata.cpp',
  'src/output-format.cpp',
  'src/pixel-buffer.cpp',
  'src/pixel-format.cpp',
  'src/progress-token.cpp',
  'src/reader.cpp',
  'src/square-isometry.cpp',
]


dependencies = [
  dependency('gl'),
  dependency('epoxy')
]

config = configuration_data()

if get_option('cpu-conversions')
  sources += 'src/conversions-cpu.cpp'
  sources += 'src/conversions-cpu-orientation.cpp'
  sources += 'src/conversions-cpu-pixel-format.cpp'
  config.set('PIXGLOT_WITH_CPU_CONVERSIONS', 1)
else
  sources += 'src/conversions-no-cpu.cpp'
endif

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

webpdec = dependency('libwebpdecoder', required: get_option('webp'))
webpdem = dependency('libwebpdemux',   required: get_option('webp'))
webpmux = dependency('libwebpmux',     required: get_option('webp'))
if webpdec.found() and webpdem.found() and webpmux.found()
  sources      += 'src/codecs/webp.cpp'
  dependencies += webpdec
  dependencies += webpdem
  dependencies += webpmux
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

if (cpp.has_header('rapidxml.h', required: get_option('xmp')))
  sources      += 'src/metadata/xmp.cpp'
  config.set('PIXGLOT_WITH_XMP', 1)
endif

if get_option('exif')
  sources += 'src/metadata/exif.cpp'

  exif_string_gen = executable('exif-string-generator',
                               'src/metadata/exif-string-generator.cpp',
                               cpp_args: cppargs)

  sources += custom_target(
    'exif-generate-strings',
    output:  'exif-strings.cpp',
    input:   'src/metadata/exif-tag-names.txt',
    command: [exif_string_gen, '@INPUT@', '@OUTPUT@']
  )
  config.set('PIXGLOT_WITH_EXIF', 1)
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
