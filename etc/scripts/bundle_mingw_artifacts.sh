#!/bin/bash
# SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0
set -e

BUILD_DIR="build"
OUTPUT_DIR="mingw_artifacts"

MINGW_RUNTIME_LIBDIR="/usr/x86_64-w64-mingw32ucrt/sys-root/mingw/bin/"

rm -rf $OUTPUT_DIR
mkdir $OUTPUT_DIR
cp $BUILD_DIR/cxx/satellites/*.{exe,dll} $OUTPUT_DIR
cp $BUILD_DIR/cxx/constellation/*/*.dll $OUTPUT_DIR
cp $MINGW_RUNTIME_LIBDIR/libgcc_s_seh-1.dll $OUTPUT_DIR
cp $MINGW_RUNTIME_LIBDIR/libstdc++-6.dll $OUTPUT_DIR
cp $MINGW_RUNTIME_LIBDIR/libwinpthread-1.dll $OUTPUT_DIR
