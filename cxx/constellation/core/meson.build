# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

core_src = files(
  'chirp/MulticastSocket.cpp',
  'chirp/Manager.cpp',
  'config/Configuration.cpp',
  'config/Dictionary.cpp',
  'config/Value.cpp',
  'config/exceptions.cpp',
  'heartbeat/HeartbeatManager.cpp',
  'heartbeat/HeartbeatSend.cpp',
  'log/CMDPSink.cpp',
  'log/Logger.cpp',
  'log/SinkManager.cpp',
  'message/BaseHeader.cpp',
  'message/CDTP1Message.cpp',
  'message/CHIRPMessage.cpp',
  'message/CHP1Message.cpp',
  'message/CMDP1Message.cpp',
  'message/CSCP1Message.cpp',
  'metrics/Metric.cpp',
  'metrics/MetricsManager.cpp',
  'networking/asio_helpers.cpp',
  'networking/zmq_helpers.cpp',
)

core_lib = library(
  'ConstellationCore',
  sources: core_src,
  include_directories: constellation_inc,
  dependencies: constellation_private_deps + [
    asio_dep,
    cppzmq_dep,
    magic_enum_dep,
    msgpack_cxx_dep,
    spdlog_dep,
    threads_dep,
    jemalloc_dep,
  ],
  gnu_symbol_visibility: 'hidden',
  cpp_args: constellation_args + ['-DCNSTLN_BUILDLIB=1'],
  install: true,
  install_rpath: constellation_rpath,
)

core_dep = declare_dependency(
  link_with: core_lib,
  compile_args: constellation_args,
  include_directories: constellation_inc,
  dependencies: [
    asio_dep,
    cppzmq_dep,
    magic_enum_dep,
    msgpack_cxx_dep,
    spdlog_dep,
    threads_dep,
    jemalloc_dep,
  ],
)

pkg.generate(core_lib,
  description: 'Constellation core library',
  extra_cflags: constellation_args,
  libraries: [asio_dep, asio_raw_dep, cppzmq_dep, magic_enum_dep, msgpack_cxx_dep, spdlog_dep],
)

install_headers(
  'chirp/MulticastSocket.hpp',
  'chirp/Manager.hpp',
  subdir: 'constellation/core/chirp',
)

install_headers(
  'config/Configuration.hpp',
  'config/Configuration.ipp',
  'config/Dictionary.hpp',
  'config/Value.hpp',
  'config/Value.ipp',
  'config/exceptions.hpp',
  subdir: 'constellation/core/config',
)

install_headers(
  'external/md5.h',
  subdir: 'constellation/core/external',
)

install_headers(
  'heartbeat/HeartbeatRecv.hpp',
  'heartbeat/HeartbeatSend.hpp',
  'heartbeat/HeartbeatManager.hpp',
  subdir: 'constellation/core/heartbeat',
)

install_headers(
  'log/CMDPSink.hpp',
  'log/Level.hpp',
  'log/log.hpp',
  'log/Logger.hpp',
  'log/ProxySink.hpp',
  'log/SinkManager.hpp',
  subdir: 'constellation/core/log',
)

install_headers(
  'message/BaseHeader.hpp',
  'message/CDTP1Message.hpp',
  'message/CHIRPMessage.hpp',
  'message/CHP1Message.hpp',
  'message/CMDP1Message.hpp',
  'message/CSCP1Message.hpp',
  'message/exceptions.hpp',
  'message/PayloadBuffer.hpp',
  subdir: 'constellation/core/message',
)

install_headers(
  'metrics/exceptions.hpp',
  'metrics/Metric.hpp',
  'metrics/MetricsManager.hpp',
  'metrics/MetricsManager.ipp',
  'metrics/stat.hpp',
  subdir: 'constellation/core/metrics',
)

install_headers(
  'networking/asio_helpers.hpp',
  'networking/exceptions.hpp',
  'networking/Port.hpp',
  'networking/zmq_helpers.hpp',
  subdir: 'constellation/core/networking',
)

install_headers(
  'protocol/CDTP_definitions.hpp',
  'protocol/CHIRP_definitions.hpp',
  'protocol/CHP_definitions.hpp',
  'protocol/CSCP_definitions.hpp',
  'protocol/Protocol.hpp',
  subdir: 'constellation/core/protocol',
)

install_headers(
  'pools/BasePool.hpp',
  'pools/BasePool.ipp',
  'pools/SubscriberPool.hpp',
  'pools/SubscriberPool.ipp',
  subdir: 'constellation/core/pools',
)

install_headers(
  'utils/casts.hpp',
  'utils/enum.hpp',
  'utils/exceptions.hpp',
  'utils/ManagerLocator.hpp',
  'utils/std_future.hpp',
  'utils/string.hpp',
  'utils/string_hash_map.hpp',
  'utils/thread.hpp',
  'utils/timers.hpp',
  'utils/type.hpp',
  subdir: 'constellation/core/utils',
)
