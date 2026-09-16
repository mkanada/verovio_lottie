#!/usr/bin/env bash
# Builds libverovio.so for Android ABIs and stages each one under
# android-libs/<abi>/libverovio.so (the jniLibs layout a Flutter/Dart host
# app expects).
#
# Same CMake target as build_linux_so.sh, but with
# -DBUILD_AS_ANDROID_LIBRARY=ON plus the NDK toolchain (links liblog, sets
# the 16 KB page-size linker flag - see ../../cmake/CMakeLists.txt).
#
# Usage:
#   ./build_android_so.sh [abi ...]
#   ANDROID_NDK_ROOT=/path/to/ndk ./build_android_so.sh arm64-v8a x86_64
#   ANDROID_PLATFORM=android-24 ./build_android_so.sh   # default: android-21
#
# Needs: Android NDK (auto-detected via ANDROID_NDK_ROOT, ANDROID_HOME/ndk/*
# or ~/Android/Sdk/ndk/*), cmake, ninja.
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
verovio_root="$(cd "$script_dir/../.." && pwd)"

default_abis=(armeabi-v7a arm64-v8a x86 x86_64)
abis=("${@:-${default_abis[@]}}")

find_ndk() {
    if [[ -n "${ANDROID_NDK_ROOT:-}" && -d "$ANDROID_NDK_ROOT" ]]; then
        echo "$ANDROID_NDK_ROOT"
        return
    fi
    local base
    for base in "${ANDROID_HOME:-$HOME/Android/Sdk}" "$HOME/Android/Sdk"; do
        if [[ -d "$base/ndk" ]]; then
            # newest version sorts last
            local latest
            latest="$(ls "$base/ndk" | sort -V | tail -n 1)"
            if [[ -n "$latest" && -d "$base/ndk/$latest" ]]; then
                echo "$base/ndk/$latest"
                return
            fi
        fi
    done
    return 1
}

ndk_root="$(find_ndk)" || {
    echo "ERROR: Android NDK not found." >&2
    echo "Set ANDROID_NDK_ROOT (or ANDROID_HOME) to your NDK, e.g.:" >&2
    echo "  ANDROID_NDK_ROOT=\$HOME/Android/Sdk/ndk/29.0.13846066 $0" >&2
    exit 1
}
toolchain="$ndk_root/build/cmake/android.toolchain.cmake"
[[ -f "$toolchain" ]] || { echo "ERROR: toolchain file missing: $toolchain" >&2; exit 1; }

# llvm-strip from this same NDK (host triplet varies: linux-x86_64,
# darwin-x86_64, darwin-arm64, ...). Android binaries must be stripped with
# it, not the host `strip`. Stripping removes the debug symbols that account
# for ~90% of the unstripped .so size; the unstripped originals stay in the
# per-ABI build dirs for debugging.
llvm_strip="$(echo "$ndk_root"/toolchains/llvm/prebuilt/*/bin/llvm-strip)"
[[ -f "$llvm_strip" ]] || { echo "ERROR: llvm-strip not found in NDK: $ndk_root" >&2; exit 1; }

platform="${ANDROID_PLATFORM:-android-21}"
echo "Using NDK: $ndk_root ($($ndk_root/toolchains/llvm/prebuilt/linux-x86_64/bin/clang --version 2>/dev/null | head -n 1 || echo ndk))"
echo "ABIs: ${abis[*]}  PLATFORM: $platform"

for abi in "${abis[@]}"; do
    build_dir="$verovio_root/tools/build-android-$abi"
    out_dir="$script_dir/android-libs/$abi"
    mkdir -p "$build_dir" "$out_dir"
    echo "--- [$abi] configuring ---"
    cmake -S "$verovio_root/cmake" -B "$build_dir" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DANDROID_ABI="$abi" \
        -DANDROID_PLATFORM="$platform" \
        -DBUILD_AS_ANDROID_LIBRARY=ON \
        -DCMAKE_BUILD_TYPE=Release
    echo "--- [$abi] building ---"
    cmake --build "$build_dir" -j"$(nproc)"
    cp "$build_dir/libverovio.so" "$out_dir/libverovio.so"
    "$llvm_strip" "$out_dir/libverovio.so"
    echo "Built $out_dir/libverovio.so"
done
