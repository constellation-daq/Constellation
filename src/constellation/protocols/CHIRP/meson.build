# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

chirp_src = files(
  'BroadcastRecv.cpp',
  'BroadcastSend.cpp',
  'Message.cpp',
  'Manager.cpp',
)

chirp_lib = library('CHIRP',
  sources: chirp_src,
  include_directories: constellation_inc,
  dependencies: [asio_dep, thread_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: ['-DCHIRP_BUILDLIB=1'],
  install : true,
)

install_headers(
  'BroadcastRecv.hpp',
  'BroadcastSend.hpp',
  'exceptions.hpp',
  'Manager.hpp',
  'Message.hpp',
  'protocol_info.hpp',
  subdir : 'constellation/protocols/CHIRP'
)

chirp_dep = declare_dependency(
  link_with: chirp_lib,
  include_directories: constellation_inc,
  dependencies: [asio_dep, thread_dep],
)

subdir('test')
