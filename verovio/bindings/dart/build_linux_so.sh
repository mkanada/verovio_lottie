#!/usr/bin/env bash
# Builds libverovio.so for the current host (Linux) and copies it next to
# this Dart package, so `DynamicLibrary.open('libverovio.so')` can find it
# with the working directory set to bindings/dart/.
#
# Mirrors the shape of csa8820/verovio_flutter's build_android_so.sh /
# build_ios_xcframework.sh: a thin script around the same CMake
# -DBUILD_AS_LIBRARY=ON target used for every other native binding here
# (see ../../cmake/CMakeLists.txt), one per target platform. Only the
# Linux host build is exercised in this repo; Android/iOS need their own
# script using -DBUILD_AS_ANDROID_LIBRARY=ON with the NDK toolchain, or
# ../iOS/create_ios_framework_headers.sh's approach, respectively - neither
# SDK is available in this environment to script and verify blind.
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
verovio_root="$(cd "$script_dir/../.." && pwd)"
build_dir="$verovio_root/tools/build-library"

mkdir -p "$build_dir"
cmake -S "$verovio_root/cmake" -B "$build_dir" \
    -DBUILD_AS_LIBRARY=ON \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" -j"$(nproc)"

cp "$build_dir/libverovio.so" "$script_dir/libverovio.so"
echo "Built $script_dir/libverovio.so"
