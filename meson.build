# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

project('constellation', 'cpp',
  default_options: ['warning_level=3', 'cpp_std=c++20'],
  license: 'EUPL-1.2',
)

constellation_inc = include_directories('.')

asio_dep = dependency('asio')
magic_enum_dep = dependency('magic_enum')

asio_test = library('asio_test',
    sources: ['BroadcastRecv.cpp', 'BroadcastSend.cpp'],
    dependencies: [asio_dep],
    cpp_args: ['-DASIO_STANDALONE=1'],
)

asio_test_dep = declare_dependency(
    link_with: [asio_test],
    dependencies: [asio_dep],
    compile_args: ['-DASIO_STANDALONE=1'],
)

chirp_lib = library('chirp',
    sources: ['CHIRP/Message.cpp'],
    include_directories: include_directories('CHIRP'),
)

executable('broadcast_recv',
    sources: ['broadcast_recv.cpp'],
    dependencies: [asio_test_dep],
)

executable('broadcast_send',
    sources: ['broadcast_send.cpp'],
    dependencies: [asio_test_dep],
)

executable('chirp_send',
    sources: ['chirp_send.cpp'],
    dependencies: [asio_test_dep],
    link_with: [chirp_lib],
)

executable('chirp_recv',
    sources: ['chirp_recv.cpp'],
    dependencies: [asio_test_dep, magic_enum_dep],
    link_with: [chirp_lib],
)
