#!/bin/sh
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0
set -e

NAME="Constellation"
VERSION=$(git describe --tags --dirty)

# Create tarball for repo
git archive HEAD --format=tar --prefix=${NAME}-${VERSION}/ --output=${NAME}-${VERSION}.tar

# Download subprojects
meson subprojects purge --confirm
meson subprojects download
rm -r subprojects/packagecache/

# Delete asio docs (file names too long)
rm -r subprojects/asio-*/doc

# Add subprojects to tarball
tar -rf ${NAME}-${VERSION}.tar --exclude-vcs --transform="s,^subprojects,${NAME}-$VERSION/subprojects," subprojects/*/

# Compress tarball
xz -9 "${NAME}-${VERSION}.tar"

# Print SHA-256
sha256sum "${NAME}-${VERSION}.tar.xz"
