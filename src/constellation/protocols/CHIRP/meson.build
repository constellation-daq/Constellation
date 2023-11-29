chirp_src = files(
  'BroadcastRecv.cpp',
  'BroadcastSend.cpp',
  'Message.cpp',
  'Manager.cpp',
)

chirp_lib = library('CHIRP',
  sources: chirp_src,
  include_directories: constellation_inc,
  dependencies: asio_dep,
  gnu_symbol_visibility: 'hidden',
  cpp_args: ['-DASIO_STANDALONE=1', '-DCHIRP_BUILDLIB=1'],
)

chirp_dep = declare_dependency(
  link_with: chirp_lib,
  include_directories: constellation_inc,
  dependencies: asio_dep,
  compile_args: ['-DASIO_STANDALONE=1'],
)

subdir('test')
