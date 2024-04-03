# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('cxx_tools')
  subdir_done()
endif

# Core / CHIRP

executable('broadcast_recv',
  sources: 'broadcast_recv.cpp',
  dependencies: [core_dep],
)
executable('broadcast_send',
  sources: 'broadcast_send.cpp',
  dependencies: [core_dep],
)
executable('chirp_manager',
  sources: 'chirp_manager.cpp',
  dependencies: [core_dep, magic_enum_dep],
)
executable('chirp_recv',
  sources: 'chirp_recv.cpp',
  dependencies: [core_dep],
)
executable('chirp_send',
  sources: 'chirp_send.cpp',
  dependencies: [core_dep, magic_enum_dep],
)

# Core / Logging

executable('cmdp_log_send',
  sources: 'cmdp_log_send.cpp',
  dependencies: [core_dep],
)

# Satellite

executable('dummy_controller',
  sources: 'dummy_controller.cpp',
  dependencies: [core_dep, cppzmq_dep],
)