# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

log_src = files(
  'Logger.cpp',
  'LogSinkManager.cpp',
  'swap_ostringstream.cpp',
  'MessageHeader.cpp',
)

log_lib = static_library(
  'ConstellationCoreLogging',
  sources: log_src,
  include_directories: constellation_inc,
  dependencies: [asio_dep, spdlog_dep, magic_enum_dep, msgpack_dep, cppzmq_dep],
  install: false,
)

log_dep = declare_dependency(
  link_with: log_lib,
  include_directories: constellation_inc,
)
