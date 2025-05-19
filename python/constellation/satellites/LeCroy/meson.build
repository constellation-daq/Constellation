# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('satellite_lecroy')
  subdir_done()
endif

lecroy_sat_files = files(
  '__main__.py',
  'LeCroySatellite.py',
)

py.install_sources(lecroy_sat_files,
  subdir: 'constellation/satellites/LeCroy'
)
