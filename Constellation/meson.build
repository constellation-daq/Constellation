# SPDX-FileCopyrightText: 2022 Stephan Lachnit
# SPDX-License-Identifier: EUPL-1.2

cppzmq_dep = dependency('cppzmq')
magic_enum_dep = dependency('magic_enum')
serializablefsm_dep = dependency('serializablefsm')
simdjson_dep = dependency('simdjson')
spdlog_dep = dependency('spdlog')
thread_dep = dependency('threads')

Constellation_inc = include_directories('..')

# Note for UNIX: Setting RPATH is required when library installation location is not in the
# system's library search path. On Windows this setting is ignored anyway.
rpath_libdir = ''
if get_option('install_rpath')
  rpath_libdir = get_option('prefix') / get_option('libdir')
endif

subdir('core')
subdir('RC')
subdir('SAT')
subdir('exec')
subdir('modules')
