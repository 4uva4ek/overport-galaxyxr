#!/usr/bin/env bash
# Build the open OpenXR dispatcher (Molier/openxr-dispatcher-open, MIT) for arm64-v8a with the Android NDK
# and package it as the CLI resource galaxyxr/arm64-v8a/libopenxr_loader.bin. Upstream CMakeLists.txt is
# used unchanged (API 29, Release). Works on Linux, macOS and Git Bash on Windows.
#
# Environment:
#   ANDROID_NDK_HOME  Android NDK r29 (required)
#   CMAKE_DIR         directory holding cmake and ninja (default: Android SDK cmake 3.22.1, else PATH)
#   DISPATCHER_SRC    existing checkout of openxr-dispatcher-open at the pinned commit;
#                     unset: the pinned commit is cloned into native/galaxyxr/build/dispatcher
set -euo pipefail
cd "$(dirname "$0")"
UPSTREAM=https://github.com/Molier/openxr-dispatcher-open
COMMIT=68479ea28e69202577cba51fbcab9baeb827a7fc   # v1.0.0, 2026-10-06
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to Android NDK r29}"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) host=windows-x86_64; exe=.exe; native() { cygpath -m "$1"; } ;;
  Darwin) host=darwin-x86_64; exe=; native() { printf '%s' "$1"; } ;;
  *) host=linux-x86_64; exe=; native() { printf '%s' "$1"; } ;;
esac
[ -f "$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" ] || { echo "NDK not found at $ANDROID_NDK_HOME" >&2; exit 1; }
bin=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$host/bin
sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-${LOCALAPPDATA:+$LOCALAPPDATA/Android/Sdk}}}
cmake_dir=${CMAKE_DIR:-$sdk/cmake/3.22.1/bin}
cmake=$cmake_dir/cmake$exe; ninja=$cmake_dir/ninja$exe
[ -x "$cmake" ] || cmake=$(command -v cmake) || { echo "cmake not found (set CMAKE_DIR)" >&2; exit 1; }
[ -x "$ninja" ] || ninja=$(command -v ninja) || { echo "ninja not found (set CMAKE_DIR)" >&2; exit 1; }

mkdir -p build
src=${DISPATCHER_SRC:-$PWD/build/dispatcher}
if [ -z "${DISPATCHER_SRC:-}" ]; then
  [ -d "$src/.git" ] || git clone -q -c core.autocrlf=false "$UPSTREAM" "$src"
  git -C "$src" -c advice.detachedHead=false checkout -q "$COMMIT"
fi
head=$(git -C "$src" rev-parse HEAD)
[ "$head" = "$COMMIT" ] || { echo "DISPATCHER_SRC is at $head, expected $COMMIT" >&2; exit 1; }
git -C "$src" diff --quiet HEAD -- src vendor CMakeLists.txt || { echo "DISPATCHER_SRC has local changes" >&2; exit 1; }

build=$PWD/build/dispatcher-arm64-v8a
"$cmake" -S "$(native "$src")" -B "$(native "$build")" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$(native "$ninja")" \
  -DCMAKE_TOOLCHAIN_FILE="$(native "$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake")" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DCMAKE_BUILD_TYPE=Release > build/dispatcher-configure.log
"$cmake" --build "$(native "$build")" > build/dispatcher-build.log

resources=../../overportcli/src/main/resources/galaxyxr
out=$resources/arm64-v8a/libopenxr_loader.bin
mkdir -p "$resources/arm64-v8a"
cp "$build/libopenxr_loader.so" "$out"
echo "== open dispatcher $COMMIT: $(stat -c %s "$out") bytes, NDK $(basename "$ANDROID_NDK_HOME")"
"$bin/llvm-readelf$exe" -d "$out" | grep -E 'SONAME|NEEDED'
"$bin/llvm-readelf$exe" -l "$out" | awk '/LOAD/ && $NF != "0x4000" {bad=1} END {exit bad}' \
  || { echo "LOAD segments are not 16 KiB aligned" >&2; exit 1; }
NM="$bin/llvm-nm$exe" sh "$src/tests/check_exports.sh" "$out"
diff -q --strip-trailing-cr "$src/LICENSE" "$resources/LICENSE-openxr-dispatcher-open.txt" > /dev/null \
  || { echo "upstream LICENSE differs from the packaged notice" >&2; exit 1; }
diff -q --strip-trailing-cr "$src/src/third_party/LICENSE-cJSON" "$resources/LICENSE-cJSON.txt" > /dev/null \
  || { echo "upstream cJSON licence differs from the packaged notice" >&2; exit 1; }
sha256sum "$out"
