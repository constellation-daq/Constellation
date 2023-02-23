core_logging_src = files(
  'Logger.cpp',
  'LogSinkManager.cpp',
  'swap_ostringstream.cpp',
)

CoreLogging_lib = static_library(
  'ConstellationCoreLogging',
  sources: core_logging_src,
  install: false,
  include_directories: [Constellation_inc],
  dependencies: [spdlog_dep],
)
