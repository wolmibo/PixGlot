test('progress-token',
  executable('progress-token', 'progress-token.cpp',
    cpp_args: cppargs, dependencies: pixglot_dep))



test('pixel-format',
  executable('pixel-format', 'pixel-format.cpp',
    cpp_args: cppargs, dependencies: pixglot_dep))



test('pixel-buffer',
  executable('pixel-buffer', 'pixel-buffer.cpp',
    cpp_args: cppargs, dependencies: pixglot_dep))



test('square-isometry',
  executable('square-isometry', 'square-isometry.cpp',
    cpp_args: cppargs, dependencies: pixglot_dep))



test('readme',
  executable('readme', 'readme.cpp',
    cpp_args: cppargs, dependencies: pixglot_dep),
  args: [files('samples/P1.pbm')]
)





ppm_tester = executable('ppm', 'ppm.cpp', cpp_args: cppargs, dependencies: pixglot_dep)

test('ppm[P1.pbm]', ppm_tester, args: [files('samples/P1.pbm'), '--gray'] )
test('ppm[P2.pgm]', ppm_tester, args: [files('samples/P2.pgm'), '--gray'] )
