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

# atomic_queue
atomic_queue_dep = dependency('atomic_queue', default_options: ['tests=false'])

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
  'libbsd=disabled',
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
  # Run libzmq and cppzmq subprojects to override dependencies regardless of meson settings
  subproject('libzmq', default_options: libzmq_sp_default_options)
  subproject('cppzmq', default_options: cppzmq_sp_default_options)
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
memory_allocator = get_option('memory_allocator')
malloc_dep = dependency('', required: false)
if memory_allocator == 'auto' or memory_allocator == 'jemalloc'
  jemalloc_dep = dependency('jemalloc', required: memory_allocator == 'jemalloc')
  if jemalloc_dep.found()
    memory_allocator = 'jemalloc'
    malloc_dep = jemalloc_dep
  endif
endif
if memory_allocator == 'auto' or memory_allocator == 'mimalloc'
  mimalloc_dep = dependency('mimalloc', required: memory_allocator == 'mimalloc', allow_fallback: true, default_options: ['warning_level=0', 'werror=false', 'b_ndebug=true'])
  if mimalloc_dep.found()
    memory_allocator = 'mimalloc'
    malloc_dep = mimalloc_dep
  endif
endif
if malloc_dep.found()
  message('Using ' + memory_allocator + ' as memory allocator')
else
  memory_allocator = 'system'
endif

# Compilation arguments and private system dependencies
constellation_args = []
constellation_private_deps = []
if host_machine.system() == 'windows'
  # Disable parts of Windows API with conflicting macro definitions
  constellation_args += ['-DWIN32_LEAN_AND_MEAN', '-DNOGDI']
  # Set minimum OS version to Windows 10
  constellation_args += '-D_WIN32_WINNT=0x0A00'
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
