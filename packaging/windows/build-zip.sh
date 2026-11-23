#!/bin/sh
set -eu

# SDL3_DLL points to the x86_64 Windows SDL3 runtime. For MinGW builds,
# use static C/C++ runtime linking or supply additional runtime DLLs.
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build_dir=${BUILD_DIR:-"$root/build-windows"}
output=${1:-"$root/dist/LastEichhof-Windows-x86_64.zip"}
: "${SDL3_DLL:?Set SDL3_DLL to the x86_64 SDL3.dll}"
cmake --build "$build_dir" --target beer-sdl
tmp=$(mktemp -d "${TMPDIR:-/tmp}/lastbeer-windows.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/LastEichhof" "$(dirname -- "$output")"
cp "$build_dir/beer-sdl.exe" "$SDL3_DLL" "$root/BEER.DAT" "$root/README.md" "$root/MODERNIZATION.md" "$root/LICENSE.GPL" "$root/packaging/SDL3-LICENSE.txt" "$tmp/LastEichhof/"
mkdir -p "$tmp/LastEichhof/docs"
cp -R "$root/docs/screenshots" "$tmp/LastEichhof/docs/"
case "$output" in /*) ;; *) output="$PWD/$output";; esac
(cd "$tmp" && cmake -E tar cf "$output" --format=zip LastEichhof)
printf 'Windows zip: %s\n' "$output"
