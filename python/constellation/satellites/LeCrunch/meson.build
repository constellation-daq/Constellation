# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('satellite_lecrunch')
  subdir_done()
endif

dependency('lecrunch', default_options: [])
lecrunch_sat_files = files(
  '__main__.py',
  'LeCrunchSatellite.py',
)

py.install_sources(lecrunch_sat_files,
  subdir: 'constellation/satellites/LeCrunch'
)
py.dependency()
