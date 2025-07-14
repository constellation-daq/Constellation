# SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

FROM gitlab.desy.de:5555/constellation/build-images/fedora-clang:latest
ARG SRC_DIR "."

# Set up and build Constellation
RUN meson setup build -Dcxx_tests=disabled -Dbuild_gui=none -Dbuildtype=release -Db_lto=true -Db_lto_mode=thin ${SRC_DIR}
RUN meson compile -C build
RUN meson install -C build

# Minify image
RUN rm -rf ${SRC_DIR} build/
