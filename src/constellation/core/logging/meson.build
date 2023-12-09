# SPDX-FileCopyrightText: 2022-2023 Stephan Lachnit
# SPDX-License-Identifier: EUPL-1.2

CoreLogging_src = files(
  'Logger.cpp',
  'LogSinkManager.cpp',
  'swap_ostringstream.cpp',
)

CoreLogging_lib = static_library(
  'ConstellationCoreLogging',
  sources: CoreLogging_src,
  include_directories: Constellation_inc,
  dependencies: [spdlog_dep],
  install: false,
)
