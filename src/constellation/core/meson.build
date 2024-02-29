# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

core_src = files(
  'logging/CMDPSink.cpp',
  'logging/Logger.cpp',
  'logging/SinkManager.cpp',
  'message/CDTP1Header.cpp',
  'message/CDTP1Message.cpp',
  'message/CSCP1Message.cpp',
  'message/Dictionary.cpp',
  'message/Header.cpp',
)

core_lib = library('ConstellationCore',
  sources: core_src,
  include_directories: constellation_inc,
  dependencies: [asio_dep, cppzmq_dep, magic_enum_dep, msgpack_cxx_dep, spdlog_dep],
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
  'message/CDTP1Header.hpp',
  'message/CDTP1Message.hpp',
  'message/CMDP1Header.hpp',
  'message/CSCP1Message.hpp',
  'message/Dictionary.hpp',
  'message/Header.hpp',
  'message/Protocol.hpp',
  subdir: 'constellation/core/message',
)

install_headers(
  'utils/casts.hpp',
  'utils/std23.hpp',
  subdir: 'constellation/core/utils',
)

core_dep = declare_dependency(
  link_with: core_lib,
  include_directories: constellation_inc,
  dependencies: [asio_dep, cppzmq_dep, magic_enum_dep, msgpack_cxx_dep, spdlog_dep],
)
