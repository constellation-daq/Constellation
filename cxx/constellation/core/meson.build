# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

core_src = files(
  'chirp/BroadcastRecv.cpp',
  'chirp/BroadcastSend.cpp',
  'chirp/Manager.cpp',
  'logging/CMDPSink.cpp',
  'logging/Logger.cpp',
  'logging/SinkManager.cpp',
  'message/CDTP1Message.cpp',
  'message/CHIRPMessage.cpp',
  'message/CSCP1Message.cpp',
  'message/Dictionary.cpp',
  'message/BaseHeader.cpp',
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
  'chirp/BroadcastRecv.hpp',
  'chirp/BroadcastSend.hpp',
  'chirp/CHIRP_definitions.hpp',
  'chirp/Manager.hpp',
  subdir: 'constellation/core/chirp',
)

install_headers(
  'external/md5.h',
  subdir: 'constellation/core/external',
)

install_headers(
  'logging/CMDPSink.hpp',
  'logging/Level.hpp',
  'logging/log.hpp',
  'logging/Logger.hpp',
  'logging/ProxySink.hpp',
  'logging/SinkManager.hpp',
  subdir: 'constellation/core/logging',
)

install_headers(
  'message/CDTP1Message.hpp',
  'message/CHIRPMessage.hpp',
  'message/CMDP1Header.hpp',
  'message/CSCP1Message.hpp',
  'message/Dictionary.hpp',
  'message/BaseHeader.hpp',
  'message/Protocol.hpp',
  'message/satellite_definitions.hpp',
  subdir: 'constellation/core/message',
)

install_headers(
  'utils/casts.hpp',
  'utils/exceptions.hpp',
  'utils/ports.hpp',
  'utils/std23.hpp',
  'utils/string.hpp',
  subdir: 'constellation/core/utils',
)

core_dep = declare_dependency(
  link_with: core_lib,
  include_directories: constellation_inc,
  dependencies: [asio_dep, cppzmq_dep, magic_enum_dep, msgpack_cxx_dep, spdlog_dep],
)