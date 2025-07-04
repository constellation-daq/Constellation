# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

cpp = meson.get_compiler('cpp')
pkgconfig = find_program('pkg-config', required: false)

# argparse
argparse_dep = dependency('argparse')

# Asio
asio_raw_dep = dependency('asio')
asio_dep = declare_dependency(
  dependencies: asio_raw_dep,
  compile_args: ['-DASIO_STANDALONE=1', '-DASIO_HAS_SNPRINTF=1'],
)

# ZeroMQ with draft API
libzmq_sys_has_draft_api_poller = false
# Check system libzmq (we cannot use dependency since we would have to overwrite it)
if pkgconfig.found()
  libzmq_sys_pkgconfig_flags = run_command([pkgconfig, '--cflags', '--libs', 'libzmq'], check: false)
  libzmq_sys_has_draft_api_poller = cpp.links('''
  #include <zmq.h>
  int main() {
    zmq_poller_new();
    return 0;
  }
  ''', args: libzmq_sys_pkgconfig_flags.stdout().split(), name: 'libzmq_sys_has_draft_api_poller')
endif
# Options for the libzmq and cppzmq subprojects (use both in subproject and dependency in case of forcefallback)
libzmq_sp_default_options = [
  'drafts=true',
  'websockets=disabled',
  'curve=false',
  'openpgm=disabled',
  'norm=disabled',
  'warning_level=0',
  'default_library=static',
]
cppzmq_sp_default_options = [
  'examples=false',
  'tests=disabled',
]
if not libzmq_sys_has_draft_api_poller
  # Override libzmq and cppzmq dependencies to always use fallback, regardless of meson settings
  libzmq_sp = subproject('libzmq', default_options: libzmq_sp_default_options)
  meson.override_dependency('libzmq', libzmq_sp.get_variable('libzmq_dep'))
  cppzmq_sp = subproject('cppzmq', default_options: cppzmq_sp_default_options)
  meson.override_dependency('cppzmq', cppzmq_sp.get_variable('cppzmq_dep'))
endif
# Get libzmq dependency first and then cppzmq dependency to ensure libzmq is setup up correctly with draft API
dependency('libzmq', default_options: libzmq_sp_default_options)
cppzmq_dep = dependency('cppzmq', default_options: cppzmq_sp_default_options)

# magic_enum
magic_enum_dep = dependency('magic_enum', default_options: ['test=false'])

# msgpack-cxx
msgpack_cxx_dep = dependency('msgpack-cxx', default_options: ['boost=disabled', 'tests=disabled'])

# spdlog
spdlog_dep = dependency('spdlog', default_options: ['tests=disabled', 'default_library=static'])

# TOML++
tomlplusplus_dep = dependency('tomlplusplus', default_options: ['default_library=static'])

# System libraries
threads_dep = dependency('threads')
dl_dep = dependency('dl', required: false)

# Memory allocator
jemalloc_dep = dependency('jemalloc', required: get_option('jemalloc'))
if jemalloc_dep.found()
  message('Using jemalloc as memory allocator')
endif

# Compilation arguments and private system dependencies
constellation_args = []
constellation_private_deps = []
if host_machine.system() == 'windows'
  # Disable parts of Windows API with conflicting macro definitions
  constellation_args += ['-DWIN32_LEAN_AND_MEAN', '-DNOGDI']
  # Add ws2_32 and iphlpapi to dependencies
  constellation_private_deps += cpp.find_library('ws2_32')
  constellation_private_deps += cpp.find_library('iphlpapi')
endif
# Add flags for static libraries
if get_option('default_library') == 'static'
  constellation_args += '-DCNSTLN_STATIC'
elif get_option('default_library') == 'both'
  error('Building both static and shared libraries is not possible')
endif

constellation_inc = include_directories('.')

# RPATH for non-default install locations
constellation_rpath = get_option('prefix') / get_option('libdir')

subdir('constellation')
subdir('tools')
subdir('controllers')
subdir('listeners')
subdir('satellites')
subdir('tests')

# Install metainfo and icon for Flathub
install_data(
  meson.project_source_root() / 'docs' / 'etc' / 'de.desy.constellation.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo',
)
install_data(
  meson.project_source_root() / 'docs' / 'logo' / 'logo.svg',
  rename: 'de.desy.constellation.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)
