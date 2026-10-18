#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to Android NDK r29}"
toolchain="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin"
out=../../overportcli/src/main/resources/galaxyxr/arm64-v8a
mkdir -p "$out"
diagnostics=()
if [[ ${GXR_INPUT_DIAGNOSTICS:-0} == 1 ]]; then diagnostics=(-DGXR_INPUT_DIAGNOSTICS); fi
if [[ ${GXR_HAND_DIAGNOSTICS:-0} == 1 ]]; then diagnostics+=(-DGXR_HAND_DIAGNOSTICS); fi
"$toolchain/aarch64-linux-android29-clang++" -std=c++17 -O2 -Wall -Wextra \
    -Werror -Wno-missing-field-initializers -fvisibility-inlines-hidden \
    -static-libstdc++ -shared -fPIC -Iinclude "${diagnostics[@]}" policy.cpp -ldl -llog -landroid -lEGL -lGLESv3 \
    -Wl,-z,max-page-size=16384 -Wl,-z,defs -Wl,-Bsymbolic \
    -Wl,--version-script=exports.map -Wl,-soname,libopenxr_loader_generic.so -o "$out/libopenxr_loader_generic.bin"
"$toolchain/llvm-strip" --strip-unneeded "$out/libopenxr_loader_generic.bin"
sha256sum "$out/libopenxr_loader_generic.bin"
