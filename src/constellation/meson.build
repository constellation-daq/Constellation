# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

asio_raw_dep = dependency('asio')
asio_dep = declare_dependency(
  dependencies: asio_raw_dep,
  compile_args: ['-DASIO_STANDALONE=1'] ,
)

magic_enum_dep = dependency('magic_enum')

constellation_inc = include_directories('..')

subdir('protocols/CHIRP')
