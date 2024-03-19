# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

asio_raw_dep = dependency('asio')
asio_dep = declare_dependency(
  dependencies: asio_raw_dep,
  compile_args: ['-DASIO_STANDALONE=1'],
)

thread_dep = dependency('threads')

catch2_dep = dependency('catch2-with-main',
  version: '>=3.0.0',
  default_options: ['tests=false'])

magic_enum_dep = dependency('magic_enum', default_options: ['test=false'])

spdlog_dep = dependency('spdlog', default_options: ['tests=disabled'])

msgpack_cxx_dep = dependency('msgpack-cxx', default_options: ['boost=disabled', 'tests=disabled'])

libzmq_dep = dependency('libzmq',
  default_options: [
    'drafts=true',
    'websockets=disabled',
    'curve=false',
    'tls=disabled',
    'nss=disabled',
    'openpgm=disabled',
    'norm=disabled',
    'warning_level=0',
])

cppzmq_dep = dependency('cppzmq', default_options: ['catch2=disabled'])

constellation_inc = include_directories('..')

subdir('core')
subdir('satellite')
subdir('tools')
