project('lecrunch',
  version: '3',
  license: 'LGPL-3.0-or-later',
  default_options: [
    'python.install_env=auto',
  ],
)

pymod = import('python')
python = pymod.find_installation('python',
  modules: ['array', 'numpy', 're', 'socket', 'struct'],
)
python.dependency()

lecrunch_src = files('LeCrunch3.py')
python.install_sources(lecrunch_src,
  subdir: 'LeCrunch3',
)

lecrunch_dep = declare_dependency(
  sources: lecrunch_src,
)
