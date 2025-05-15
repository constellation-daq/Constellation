project('lecrunch',
  version: '3',
  license: 'LGPL-3.0-or-later',
  default_options: [
    'python.install_env=auto',
  ],
)

python = import('python').find_installation(
  modules: ['array', 'numpy', 're', 'socket', 'struct'],
)
impl_py = python.found()
if impl_py
  lecrunch_src = files('LeCrunch3.py')
  python.install_sources(lecrunch_src,
    subdir: 'LeCrunch3',
  )

  lecrunch_dep = declare_dependency(
    sources: lecrunch_src,
  )
endif
