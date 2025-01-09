# Building as Flatpak

Constellation can be built and distributed as [Flatpak](https://flatpak.org/). The advantage is that this only requires an
installation of Meson and Flatpak to build and makes it easy to distribute Constellation to the users.

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

## Preparing the Flatpak

To build the Flatpak, a [MetaInfo file](https://www.freedesktop.org/software/appstream/docs/) for Constellation needs to be
created. The file should be named `de.desy.constellation.metainfo.xml` and located in a folder named `flatpak` within a clone
of the Constellation repository with the following contents:

```xml
<?xml version="1.0" encoding="UTF-8" ?>
<component type="desktop-application">
  <id>de.desy.constellation</id>
  <metadata_license>CC0-1.0</metadata_license>

  <name>Constellation</name>
  <developer id="de.desy"><name>Deutsches Elektronen-Synchrotron DESY</name></developer>
  <summary>Control and Data Acquisition System</summary>
  <project_license>EUPL-1.2</project_license>
  <url type="homepage">https://constellation.pages.desy.de/</url>
  <url type="vcs-browser">https://gitlab.desy.de/constellation/constellation</url>

  <description>
    <p>
      Constellation is an autonomous control and data acquisition system for small-scale experiments and experimental setup with volatile and dynamic constituents such as testbeam environments or laboratory test stands. Constellation aims to provide a flexible framework that requires minimal effort for the integration of new devices, that is based on widely adopted open-source network communication libraries and that keeps the required maintenance as low as possible.
    </p>
    <p>
      This Flatpak contains the graphical MissionControl interface for controlling and many C++ satellites. They can be started with <code>flatpak run de.desy.constellation -t TYPE</code>. The following satellites are included:
    </p>
    <ul>
      <li>DevNullReceiver</li>
      <li>EudaqNativeWriter</li>
      <li>RandomTransmitter</li>
      <li>Sputnik</li>
    </ul>
    <p>
      Please note that file permissions for file writing have to be given explicitly e.g. with Flatseal.
    </p>
  </description>
  <screenshots>
    <screenshot type="default">
      <image>https://constellation.pages.desy.de/_images/missioncontrol_run.png</image>
      <caption>MissionControl main window</caption>
    </screenshot>
  </screenshots>
  <keywords>
    <keyword translate="no">DAQ</keyword>
    <keyword>testbeam</keyword>
  </keywords>
  <branding>
    <color type="primary" scheme_preference="light">#6fbbd8</color>
    <color type="primary" scheme_preference="dark">#d97e7c</color>
  </branding>

  <content_rating type="oars-1.1" />
  <requires>
    <display_length compare="ge">768</display_length>
  </requires>
  <recommends>
    <control>keyboard</control>
    <control>pointing</control>
  </recommends>
  <supports>
    <control>touch</control>
  </supports>

  <launchable type="desktop-id">de.desy.constellation.missioncontrol.desktop</launchable>

  <releases>
      <release version="0.2" date="2024-12-09">
        <url type="details">https://constellation.pages.desy.de/news/2024-12-09-Constellation-Release-0.2.html</url>
      </release>
      <release version="0.1" date="2024-11-05">
        <url type="details">https://constellation.pages.desy.de/news/2024-11-05-Constellation-Release-0.1.html</url>
      </release>
  </releases>
</component>
```

Next, the manifest for the Flatpak can be created. It should be named `de.desy.constellation.yml` and located within the same
`flatpak` folder:

```yaml
id: de.desy.constellation
runtime: org.kde.Platform
runtime-version: "6.8"
sdk: org.kde.Sdk
command: Satellite
finish-args:
  - --share=network
  - --filesystem=host
  - --share=ipc
  - --socket=fallback-x11
  - --socket=wayland
  - --device=dri
cleanup:
  - /include
  - /lib/pkgconfig
  - /share/pkgconfig
modules:
  - name: Constellation
    buildsystem: meson
    config-opts:
      - -Dbuildtype=release
      - -Dimpl_py=disabled
      - -Dcxx_tools=false
      - -Dcxx_tests=disabled
      - -Dsatellite_dev_null_receiver=true
      - -Dsatellite_eudaq_native_writer=true
      - -Dsatellite_random_transmitter=true
      - -Dsatellite_sputnik=true
    sources:
      - type: dir
        path: ..
  - name: Constellation-metainfo
    buildsystem: simple
    build-commands:
      - install -D de.desy.constellation.metainfo.xml /app/share/metainfo/de.desy.constellation.metainfo.xml
    sources:
      - type: file
        path: de.desy.constellation.metainfo.xml
```

## Building the Flatpak

Since internet access is not available during the Flatpak build, all dependencies need to be downloaded beforehand. This can
be easily done with Meson:

```sh
meson subprojects download
```

Finally the Flatpak can be built by running the following command in the `flatpak` folder:

```sh
flatpak run --user org.flatpak.Builder --force-clean --user --install --install-deps-from=flathub --ccache --repo=repo builddir de.desy.constellation.yml
```

The Flatpak should now be installed for the current user.

## Running the Flatpak

Currently, the Flatpak only contains the C++ side of Constellation. It includes the hardware-independent satellites and the
MissionControl GUI. The GUIs can be started like any other desktop application.

Satellites can be started using the following command:

```sh
flatpak run de.desy.constellation -t Sputnik -n Flatpak -g edda
```

```{attention}
Satellite that require access to the file system might require additional file system permissions for special data folder
like e.g. `/data`. These path can be added using e.g. [Flatseal](https://flathub.org/apps/com.github.tchx84.Flatseal).
```
