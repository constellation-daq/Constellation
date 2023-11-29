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
test_broadcast = executable('test_broadcast',
  sources: 'test_broadcast.cpp',
  dependencies: chirp_dep,
)
test('CHIRP broadcast test', test_broadcast, is_parallel : false)

# unit tests for CHIRP message code
test_message = executable('test_message',
  sources: 'test_message.cpp',
  dependencies: chirp_dep,
)
test('CHIRP message test', test_message)

# unit tests for CHIRP manager code
test_manager = executable('test_manager',
  sources: 'test_manager.cpp',
  dependencies: chirp_dep,
)
test('CHIRP manager test', test_manager, is_parallel : false)
