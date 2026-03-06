# Building as Flatpak

Constellation can be built and distributed as [Flatpak](https://flatpak.org/). The advantage is that this only requires an
installation of Meson and Flatpak to build and makes it easy to distribute Constellation to the users.

Information on how to start and run the Constellation Flatpak can be found in the
[operator guide](../operator_guide/get_started/installation.md#installing-as-flatpak-package-from-flathub).

## Setting up Flatpak

The installation instruction for Flatpak can be found on the [Flatpak website](https://flatpak.org/setup/).
To build the Flatpak, some additional steps have to be taken.

First, [Flathub](https://flathub.org/) needs to be added as a repository user-wide:

```sh
flatpak remote-add --if-not-exists --user flathub https://dl.flathub.org/repo/flathub.flatpakrepo
```

Then the Flatpak Builder needs to be installed:

```sh
flatpak install -y --user flathub org.flatpak.Builder
```

## Preparing the Flatpak Build

Since internet access is not available during the Flatpak build, all dependencies need to be downloaded beforehand.
For the C++ dependencies this can be achieved using Meson:

```sh
meson subprojects download
```

For the Python dependencies [`req2flatpak`](https://github.com/johannesjh/req2flatpak) is used to generate a manifest on the fly by running the following commands in the `flatpak` folder:

```sh
pip install pip-tools req2flatpak
pip-compile --extra cli --extra hdf5 --extra influx --extra lecroy --extra visa -o requirements.txt ../pyproject.toml
req2flatpak -r requirements.txt -t cp313-x86_64 cp313-aarch64 -o pypi-dependencies.json
```

````{note}
The current Python version of the Flatpak runtime to be specified in the `req2flatpak` command can be checked via:

```sh
flatpak run --user --command=cat org.freedesktop.Sdk /usr/manifest.json | grep cpython -A 1
```
````

## Building the Flatpak

The Flatpak can be built by running the following command in the `flatpak` folder:

```sh
flatpak run --user org.flatpak.Builder --force-clean --user --install --install-deps-from=flathub --mirror-screenshots-url=https://dl.flathub.org/media/ --ccache --repo=repo builddir de.desy.constellation.yml
```

The Flatpak should now be installed for the current user.

After the build, the linter can be run to check for any blockers for the submission to Flathub:

```sh
flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest de.desy.constellation.yml
flatpak run --command=flatpak-builder-lint org.flatpak.Builder repo repo
```
