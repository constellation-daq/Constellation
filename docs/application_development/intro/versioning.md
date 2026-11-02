# Versioning & Compatibility

Constellation follows [semantic versioning](https://semver.org/) with version numbers of the form `MAJOR.MINOR.PATCH`.
This page describes what each part of the version number means for satellites developed against the Constellation
libraries.

## Major Version

Satellites built against the same major version of Constellation can communicate with each other and with controllers and
listeners of the same major version. This holds independently of the minor and patch version and of the implementation
language. This means that a satellite compiled against version `1.0.2` can be operated in the same Constellation as a
satellite compiled against version `1.1.1`, and both can be controlled by a controller compiled against Constellation
version `1.3.0`.

The major version is increased for changes which may break the communication between components, such as changes to the
message format of a protocol or the introduction of a new protocol version, e.g. from `CDTP1` to `CDTP2`. Satellites of
different major versions might still be able to communicate, since individual protocols can be forward or backward
compatible, but this is not guaranteed. In this case, porting satellites to the latest stable version is recommended.

## Minor Version

New minor versions add features that may extend or change the satellite interface. In this case satellite code might need to
be adapted when updating. All required changes are listed in the release notes.

Within the same minor version, the satellite interface of the C++ and Python libraries does not change, and satellite code
does not require any modification.

## Patch Version

Patch versions only contain bug fixes and do not add new features or change library APIs. This means no code changes are
necessary for third-party Constellation components written for Constellation `1.0.2` when the library is updated to `1.0.3`.

Updating to the latest patch version of the used minor version is always recommended.

## Specifying the Constellation Dependency

Satellites should restrict the Constellation dependency to the minor version they have been developed against, which allows
updates to new patch versions while protecting against interface changes:

::::{tab-set}
:::{tab-item} C++ / Meson
:sync: cxx

```meson
constellation_dep = dependency('ConstellationSatellite', version: ['>=1.2.0', '<1.3.0'])
```

:::
:::{tab-item} Python
:sync: python

```toml
dependencies = ["ConstellationDAQ>=1.2.0,<1.3.0"]
```

:::
::::

```{note}
Binary compatibility of the C++ libraries is not guaranteed, also not between patch versions. C++ satellites should
therefore always be compiled against the same Constellation version which is used to execute them.
```
