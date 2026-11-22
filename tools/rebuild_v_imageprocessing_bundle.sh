#!/usr/bin/env bash
# Rebuild the bundled V-ImageProcessing static library for Linux x86_64 and
# refresh the bundled public headers from a V-ImageProcessing source checkout.
#
# Requires access to the private V-ImageProcessing source (lab-internal).
#
# Usage:
#   tools/rebuild_v_imageprocessing_bundle.sh <path-to-V-ImageProcessing>/src/V
#
# What it does:
#   1. Configures this repository with -DV_IP_SOURCE_ROOT=<path> in Release
#      mode (GCC: -O3 -DNDEBUG, position-independent code, C++17) and builds
#      only the V_2D static-library target.
#   2. Copies the result to third_party/v_imageprocessing/lib/linux-x86_64/.
#   3. Refreshes every header already present under
#      third_party/v_imageprocessing/include/ whose content differs from the
#      source checkout (the set of bundled headers is not changed). Bundled
#      headers keep CRLF line endings.
#
# The Windows library (lib/win-x64/V_2D.lib) is rebuilt on a Windows machine;
# see third_party/v_imageprocessing/NOTICE.md.
set -euo pipefail

if [ "$#" -ne 1 ] || [ ! -d "$1/API_tier/2D/include" ]; then
    echo "usage: $0 <path-to-V-ImageProcessing>/src/V" >&2
    exit 2
fi

v_root="$(cd "$1" && pwd)"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bundle_root="${repo_root}/third_party/v_imageprocessing"
build_dir="${repo_root}/build_v_imageprocessing"

cmake -S "${repo_root}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DV_IP_SOURCE_ROOT="${v_root}"
cmake --build "${build_dir}" --target V_2D -j"$(nproc)"

cp "${build_dir}/libV_2D.a" "${bundle_root}/lib/linux-x86_64/libV_2D.a"
echo "updated lib/linux-x86_64/libV_2D.a"

(cd "${bundle_root}/include" && find . -type f -name '*.h' | sort) | while read -r header; do
    source_header="${v_root}/${header}"
    bundled_header="${bundle_root}/include/${header}"
    if [ ! -f "${source_header}" ]; then
        echo "warning: ${header} is not in the source checkout" >&2
        continue
    fi
    if ! diff -q --strip-trailing-cr "${bundled_header}" "${source_header}" > /dev/null; then
        awk '{ sub(/\r$/, ""); printf "%s\r\n", $0 }' "${source_header}" > "${bundled_header}"
        echo "updated include/${header}"
    fi
done
