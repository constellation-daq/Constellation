# SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

FROM gitlab.desy.de:5555/constellation/build-images/fedora-clang:latest

# Set up and build Constellation
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

# Remove build artifacts
RUN rm -rf build

# Minify image
RUN rm -rf /var/lib/apt/lists/* /usr/share/man/* /usr/share/lintian/overrides/* /usr/share/info/* /tmp/*

# Set entry point to satellite executable
ENTRYPOINT ["Satellite"]
