# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

satellite_src = files(
  'CSCPHandler.cpp',
)

satellite_lib = library('ConstellationSatellite',
  sources: satellite_src,
  include_directories: constellation_inc,
  dependencies: [core_dep, cppzmq_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: ['-DCNSTLN_BUILDLIB=1'],
  install: true,
)

install_headers(
  'CSCPHandler.hpp',
  subdir: 'constellation/satellite',
)
