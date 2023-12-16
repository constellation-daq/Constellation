# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

CoreLogging_src = files(
  'Logger.cpp',
  'LogSinkManager.cpp',
  'swap_ostringstream.cpp',
)

CoreLogging_lib = static_library(
  'ConstellationCoreLogging',
  sources: CoreLogging_src,
  include_directories: constellation_inc,
  dependencies: [spdlog_dep, magic_enum_dep, msgpack_dep, cppzmq_dep],
  install: false,
)
