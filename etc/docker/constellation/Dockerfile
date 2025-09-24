# SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

FROM ubuntu:24.04

# Ensure non-interactive dpkg invocation
ENV DEBIAN_FRONTEND=noninteractive

# Update image
RUN apt-get update && apt-get full-upgrade --autoremove -y

# Install git and SSL certificates
RUN apt-get install --no-install-recommends -y git ca-certificates netbase

# Install Meson and GCC toolchain
RUN apt-get install --no-install-recommends -y meson g++

# Set up and build Constellation
RUN --mount=type=bind,rw,source=.,target=/constellation meson setup build /constellation \
    -Dbuildtype=release \
    -Db_lto=true \
    -Dcxx_tools=false \
    -Dcxx_tests=disabled \
    -Dbuild_gui=none \
    -Dsatellite_dev_null_receiver=true \
    -Dsatellite_mattermost=true \
    -Dsatellite_random_transmitter=true \
    && meson compile -C build \
    && meson install -C build --tags runtime

# Remove build artifacts, toolchain, and git
RUN rm -rf build
RUN apt-get remove --auto-remove --purge -y meson g++ git

# Minify image
RUN rm -rf /var/lib/apt/lists/* /usr/share/man/* /usr/share/lintian/overrides/* /usr/share/info/* /tmp/*

# Set entry point to satellite executable
ENTRYPOINT ["Satellite"]
