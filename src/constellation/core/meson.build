# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

core_src = files(
  'logging/CMDPSink.cpp',
  'logging/Logger.cpp',
  'logging/SinkManager.cpp',
  'message/Header.cpp',
)

core_lib = library('ConstellationCore',
  sources: core_src,
  include_directories: constellation_inc,
  dependencies: [asio_dep, cppzmq_dep, magic_enum_dep, msgpackc_cxx_dep, spdlog_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: ['-DCNSTLN_BUILDLIB=1'],
  install: true,
)

install_headers(
  'config.hpp',
  subdir: 'constellation/core',
)

install_headers(
  'logging/CMDPSink.hpp',
  'logging/Level.hpp',
  'logging/log.hpp',
  'logging/Logger.hpp',
  'logging/ProxySink.hpp',
  'logging/SinkManager.hpp',
  subdir: 'constellation/logging',
)

install_headers(
  'message/Header.hpp',
  'message/Protocol.hpp',
  subdir: 'constellation/core/message',
)

install_headers(
  'utils/dictionary.hpp',
  'utils/std23.hpp',
  'utils/stdbyte_casts.hpp',
  subdir: 'constellation/core/utils',
)

core_dep = declare_dependency(
  link_with: core_lib,
  include_directories: constellation_inc,
  dependencies: [asio_dep, cppzmq_dep, magic_enum_dep, msgpackc_cxx_dep, spdlog_dep],
)

# logging test tools
executable('cmdp_log_send',
  sources: 'logging/test/cmdp_log_send.cpp',
  dependencies: core_dep,
)

# logging tests
test_log = executable('test_log',
  sources: 'logging/test/test_log.cpp',
  dependencies: [core_dep, catch2_dep],
)
test('Logging test', test_log,
  args: ['--durations', 'yes', '--verbosity', 'high'],
  is_parallel: false)
