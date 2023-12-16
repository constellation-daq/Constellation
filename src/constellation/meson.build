# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

asio_raw_dep = dependency('asio')
asio_dep = declare_dependency(
  dependencies: asio_raw_dep,
  compile_args: ['-DASIO_STANDALONE=1'] ,
)

thread_dep = dependency('threads')

magic_enum_dep = dependency('magic_enum')

spdlog_dep = dependency('spdlog',
  default_options: ['tests=False']
)

msgpack_dep = dependency('msgpack-cxx',
  version: '>=6.1.0',
  default_options: ['boost=enabled', 'tests=disabled'])

cppzmq_dep = dependency('cppzmq')

constellation_inc = include_directories('..')

subdir('core')
subdir('chirp')
subdir('core/logging')
