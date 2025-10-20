# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# Build listener only if feature is enabled:
if not get_option('listener_telemetry_console')
  subdir_done()
endif

if get_option('build_gui') == 'none'
  message('Skipping TelemetryConsole since build_gui is set to none')
  subdir_done()
endif

qtcharts_version = get_option('build_gui')
qtcharts = import(qt_version)
qtcharts_dep = dependency(qt_version, modules: ['Core', 'Gui', 'Widgets', 'Charts'])

qrc_files = qt.compile_resources(sources: 'TelemetryConsole.qrc')
ui_files = qt.compile_ui(sources: ['TelemetryConsole.ui'])
moc_files = qt.compile_moc(headers: ['QMetricDisplay.hpp', 'QStatListener.hpp', 'TelemetryConsole.hpp'])

executable('TelemetryConsole',
  sources: ['main.cpp', 'TelemetryConsole.cpp', 'QMetricDisplay.cpp', 'QMetricSeries.cpp', 'QStatListener.cpp', qrc_files, moc_files, ui_files],
  dependencies: [core_dep, gui_dep, listener_dep, exec_dep, argparse_dep, qtcharts_dep],
  install: true,
  install_rpath: constellation_rpath,
)
