# SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('satellite_flight_recorder')
  subdir_done()
endif

satellite_type = 'FlightRecorder'

satellite_sources = files(
  'FlightRecorderSatellite.cpp',
)

satellite_dependencies = [listener_dep, spdlog_dep]

satellites_to_build += [[satellite_type, satellite_sources, satellite_dependencies]]
