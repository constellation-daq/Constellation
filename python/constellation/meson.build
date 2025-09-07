# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

core_files = files(
  'core/__init__.py',
  'core/base.py',
  'core/chirp.py',
  'core/chirpmanager.py',
  'core/commandmanager.py',
  'core/chp.py',
  'core/cmdp.py',
  'core/cscp.py',
  'core/cdtp.py',
  'core/controller.py',
  'core/configuration.py',
  'core/error.py',
  'core/fsm.py',
  'core/heartbeatchecker.py',
  'core/heartbeater.py',
  'core/logging.py',
  'core/monitoring.py',
  'core/multicast.py',
  'core/network.py',
  'core/protocol.py',
  'core/py.typed',
  'core/receiver_satellite.py',
  'core/satellite.py',
  'core/transmitter_satellite.py',
)

core_message_files = files(
  'core/message/__init__.py',
  'core/message/cscp1.py',
  'core/message/cdtp2.py',
  'core/message/exceptions.py',
  'core/message/msgpack_helpers.py',
  'core/message/multipart.py',
)

satellites_files = files(
  'satellites/__init__.py',
)

py.install_sources(core_files,
  subdir: 'constellation/core')

py.install_sources(core_message_files,
  subdir: 'constellation/core/message')

py.install_sources(satellites_files,
  subdir: 'constellation/satellites')

subdir('satellites')

# Create file for version code name
version_code_name_data = configuration_data()
version_code_name_data.set('version_code_name', version_code_name)
version_code_name_file = configure_file(
  configuration: version_code_name_data,
  input: 'core/version_code_name',
  output: 'version_code_name',
)
py.install_sources(version_code_name_file,
  subdir: 'constellation/core',
)
