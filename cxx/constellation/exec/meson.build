# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

exec_src = files(
  'cli.cpp',
  'cpp.cpp',
  'DSOLoader.cpp',
  'satellite.cpp',
)

exec_lib = library('ConstellationExec',
  sources: exec_src + [build_vcs_hpp],
  include_directories: constellation_inc,
  dependencies: [core_dep, satellite_dep, argparse_dep, asio_dep, dl_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: constellation_args + ['-DCNSTLN_BUILDLIB=1'],
  install: true,
  install_rpath: constellation_rpath,
)

exec_dep = declare_dependency(
  link_with: exec_lib,
  compile_args: constellation_args,
  include_directories: constellation_inc,
  dependencies: [core_dep, satellite_dep, argparse_dep],
)
meson.override_dependency('ConstellationExec', exec_dep)

pkg.generate(exec_lib,
  description: 'Constellation exec library',
  variables: ['bindir=${prefix}/'+get_option('bindir')],
  uninstalled_variables: ['bindir=${prefix}/'+get_option('bindir')],
  extra_cflags: constellation_args,
  libraries: [core_dep, satellite_dep],
)

install_headers(
  'cli.hpp',
  'cpp.hpp',
  'DSOLoader.hpp',
  'exceptions.hpp',
  'satellite.hpp',
  subdir: 'constellation/exec',
)

executable('Satellite',
  sources: 'satellite_main.cpp',
  dependencies: [exec_dep],
  install: true,
  install_rpath: constellation_rpath,
)

# Template files for in-repo satellite implementations
satellite_template_dir = meson.current_source_dir() / 'templates'
satellite_generator_template = files(satellite_template_dir / 'generator.cpp.in')
satellite_main_template = files(satellite_template_dir / 'main.cpp.in')

install_data(
  satellite_generator_template,
  satellite_main_template,
  install_dir: get_option('includedir') / 'constellation/exec/templates',
)
