# SPDX-FileCopyrightText: 2022-2023 Stephan Lachnit
# SPDX-License-Identifier: EUPL-1.2

conf_data = configuration_data()
conf_data.set('version', meson.project_version())
conf_data.set('install_full_libdir', get_option('prefix') / get_option('libdir'))
configure_file(
  input: 'config.hpp',
  output: 'config.hpp',
  configuration: conf_data,
)

Core_src = files(
  'DSOLoader.cpp',
)

subdir('logging')

Core_lib = library('ConstellationCore',
  sources: Core_src,
  include_directories: [Constellation_inc],
  dependencies: [simdjson_dep, magic_enum_dep, cppzmq_dep],
  link_whole: [CoreLogging_lib],
  install: true,
)

Core_dep = declare_dependency(
  link_with: Core_lib,
  include_directories: [Constellation_inc],
)
