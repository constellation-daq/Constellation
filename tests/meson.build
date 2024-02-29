# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# Meson only used to run C++ tests
if not impl_cpp
    subdir_done()
endif

# CHIRP

test_chirp_broadcast = executable('test_chirp_broadcast',
  sources: 'test_chirp_broadcast.cpp',
  dependencies: [chirp_dep, catch2_dep],
)
test('CHIRP broadcast test', test_chirp_broadcast,
  args: ['--durations', 'yes', '--verbosity', 'high'],
  is_parallel: false,
)

test_chirp_manager = executable('test_chirp_manager',
  sources: 'test_chirp_manager.cpp',
  dependencies: [chirp_dep, catch2_dep],
)
test('CHIRP manager test', test_chirp_manager,
  args: ['--durations', 'yes', '--verbosity', 'high'],
  is_parallel: false,
)

test_chirp_message = executable('test_chirp_message',
  sources: 'test_chirp_message.cpp',
  dependencies: [chirp_dep, catch2_dep],
)
test('CHIRP message test', test_chirp_message,
  args: ['--durations', 'yes', '--verbosity', 'high'],
)

# Core / Logging

test_core_logging = executable('test_core_logging',
  sources: 'test_core_logging.cpp',
  dependencies: [core_dep, catch2_dep],
)
test('Core logging test', test_core_logging,
  args: ['--durations', 'yes', '--verbosity', 'high'],
  is_parallel: false,
)

# Core / Message

test_core_message = executable('test_core_message',
  sources: 'test_core_message.cpp',
  dependencies: [core_dep, catch2_dep],
)
test('Core message header test', test_core_message,
  args: ['--durations', 'yes', '--verbosity', 'high'],
)

# Satellite

test_satellite_fsm = executable('test_satellite_fsm',
  sources: 'test_satellite_fsm.cpp',
  dependencies: [satellite_dep, catch2_dep],
)
test('Satellite FSM test', test_satellite_fsm,
     args: ['--durations', 'yes', '--verbosity', 'high'],
)
