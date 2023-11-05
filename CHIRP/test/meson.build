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

# unit tests for broadcast code
broadcast_test = executable('broadcast_test',
  sources: 'broadcast_test.cpp',
  dependencies: chirp_dep,
)
test('CHIRP broadcast test', broadcast_test)

# unit tests for CHIRP message code
message_test = executable('message_test',
  sources: 'message_test.cpp',
  dependencies: chirp_dep,
)
test('CHIRP message test', message_test)
