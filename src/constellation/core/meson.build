# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

core_src = files(
  'message/Header.cpp',
)

core_lib = library('ConstellationCore',
  sources: core_src,
  include_directories: constellation_inc,
  dependencies: [msgpackc_cxx_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: ['-DCHIRP_BUILDLIB=1'],
  install: true,
)

install_headers(
  'config.hpp',
  'std23.hpp',
  subdir: 'constellation/core',
)

install_headers(
  'message/Header.hpp',
  'message/Protocol.hpp',
  subdir: 'constellation/core/message',
)

install_headers(
  'utils/dictionary.hpp',
  'utils/stdbyte_casts.hpp',
  subdir: 'constellation/core/utils',
)

subdir('logging')
