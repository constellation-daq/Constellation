# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# broadcast test tools
executable('broadcast_recv',
  sources: 'broadcast_recv.cpp',
  dependencies: chirp_dep,
)
executable('broadcast_send',
  sources: 'broadcast_send.cpp',
  dependencies: chirp_dep,
)

# CHIRP test tools
executable('chirp_recv',
  sources: 'chirp_recv.cpp',
  dependencies: [chirp_dep, magic_enum_dep],
)
executable('chirp_send',
  sources: 'chirp_send.cpp',
  dependencies: [chirp_dep, magic_enum_dep],
)
executable('chirp_manager',
  sources: 'chirp_manager.cpp',
  dependencies: [chirp_dep, magic_enum_dep],
)
