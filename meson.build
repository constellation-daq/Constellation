# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

project('constellation', 'cpp',
  default_options: ['warning_level=3', 'cpp_std=c++20'],
  license: 'EUPL-1.2',
)

constellation_inc = include_directories('.')

 = dependency('asio')

asio_test = library('asio_test',
    sources: ['BroadcastRecv.cpp', 'BroadcastSend.cpp'],
    dependencies: [asio_dep],
)

executable('broadcast_recv',
    sources: ['broadcast_recv.cpp'],
    dependencies: [asio_dep],
    link_with: [asio_test],
)

executable('broadcast_send',
    sources: ['broadcast_send.cpp'],
    dependencies: [asio_dep],
    link_with: [asio_test],
)
