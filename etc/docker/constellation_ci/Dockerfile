# SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

FROM gitlab.desy.de:5555/constellation/build-images/fedora-clang:latest

# Install pip
RUN dnf install -y python3-pip

# Set up and build Constellation
RUN --mount=type=bind,rw,source=.,target=/constellation pip install --break-system-packages \
    ./constellation[cli,hdf5,influx,lecroy,visa]
RUN --mount=type=bind,rw,source=.,target=/constellation meson setup build /constellation \
    -Dbuildtype=release \
    -Db_lto=true \
    -Db_lto_mode=thin \
    -Dcxx_tools=false \
    -Dcxx_tests=disabled \
    -Dbuild_gui=none \
    -Dsatellite_dev_null_receiver=true \
    -Dsatellite_mattermost=true \
    -Dsatellite_random_transmitter=true \
    && meson compile -C build \
    && meson install -C build

# Remove build artifacts and pip
RUN rm -rf build
RUN dnf remove -y python3-pip

# Minify image
RUN dnf clean all
RUN rm -rf /var/cache/dnf/* /usr/share/man/* /tmp/*

# Set entry point to satellite executable
ENTRYPOINT ["Satellite"]
