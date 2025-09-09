# Adding a Satellite to the Framework

Once a satellite has been successfully developed and tested, it can be useful to easily share the satellite with others by
adding it directly to the framework. This guide will goes through the steps required to add a satellite to the main
Constellation repository.

```{seealso}
Please consult the [introduction](../intro/listing.md#inclusion-in-the-constellation-repository) on whether a particular
satellite is suitable for inclusion.
```

Constellation uses the [Meson build system](https://mesonbuild.com/) for both the C++ and Python implementation of the
framework. The main task of migrating an external framework to a framework satellite is implementing the build and
installation of the satellite in Meson.

::::{tab-set}
:::{tab-item} C++

All C++ framework satellites live within the `cxx/satellites` subfolder in the main repository.
The following steps need to be followed to add the satellite to the common build:

* For the new satellites a new folder should be created named after the satellite type. The header and implementation files
  for the satellite class should be named `ExampleSatellite.*pp` where `Example` is the satellite type.

* A new `meson.build` files needs to be created with the following content:

  ```meson
  # Build satellite if option is set
  if not get_option('satellite_example')
    subdir_done()
  endif

  # Type this satellite identifies as
  satellite_type = 'Example'

  # Source files to be compiled for this satellite
  satellite_sources = files(
    'ExampleSatellite.cpp',
  )

  # Additional dependencies for this satellite
  satellite_dependencies = []

  # Add [type, sources, dependencies] to build list
  satellites_to_build += [[satellite_type, satellite_sources, satellite_dependencies]]
  ```

  In this file, the satellite type, the source files and the build option need to be adjusted.

* External dependencies can added like this:

  ```meson
  my_dep = dependency('TheLibrary')

  satellite_dependencies = [my_dep]
  ```

  More details on Meson dependencies can be found in the [Meson manual](https://mesonbuild.com/Dependencies.html).

* To include the newly created `meson.build` file in the build process, it has to be added to the `cxx/satellite/meson.build`
  file using `subdir('Example')`.

* Finally, the build option for the satellite has to be added to the `meson_options.txt` file:

  ```meson
  option('satellite_example', type: 'boolean', value: false, description: 'Build Example satellite')
  ```

  The satellite can now be enabled with `meson configure build -Dsatellite_example=true`.

:::
:::{tab-item} Python
:sync: python

All Python framework satellites live within the `python/constellation/satellites` subfolder in the main repository.
The following steps need to be followed to add the satellite to the common build:

* For the new satellites a new folder should be created named after the satellite type. The Python file for the satellite
  class should be named `Example.py` where `Example` is the satellite type. Additionally the starting script needs to be
  contained in a separate `__main__.py` file.

* A new `meson.build` files needs to be created with the following content:

  ```meson
  py_sat_files = files(
    '__main__.py',
    'Example.py',
  )

  py.install_sources(py_sat_files,
    subdir: 'constellation/satellites/Example')
  ```

  In this file, the source files and the installation location need to be adjusted with the new satellite type.

* To include the newly created `meson.build` file in the build process, it has to be added to the
  `python/constellation/satellite/meson.build` file using `subdir('Example')`.

* To create an executable for the satellite (i.e. an entry point), the main function can be added to `[project.scripts]` in
  the `pyprojects.toml` file in the root directory:

  ```TOML
  SatelliteExample = "constellation.satellites.Example.__main__:main"
  ```

* Additional dependencies need to be added to `[project.optional-dependencies]` in the `pyprojects.toml` file via:

  ```TOML
  example = ["numpy"]
  ```

* Running `pip install --no-build-isolation -e .[example]` in the venv now installs the satellite executable and all required
  dependencies.

:::
::::
