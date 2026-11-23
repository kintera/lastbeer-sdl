#!/bin/sh
set -eu

# Build on an old enough Linux baseline for the systems that will run the image.
# APPIMAGETOOL may point to appimagetool itself or an extracted AppRun.
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build_dir=${BUILD_DIR:-"$root/build-sdl"}
output=${1:-"$root/dist/LastEichhof-x86_64.AppImage"}
tool=${APPIMAGETOOL:-appimagetool}
case $(uname -m) in x86_64) ;; *) echo "This package script currently targets x86_64." >&2; exit 1;; esac
command -v "$tool" >/dev/null 2>&1 || { echo "Set APPIMAGETOOL to appimagetool." >&2; exit 1; }
cmake --build "$build_dir" --target beer-sdl
sdl=$(ldd "$build_dir/beer-sdl" | awk '/libSDL3\.so/{print $3; exit}')
[ -n "$sdl" ] && [ -f "$sdl" ] || { echo "Cannot locate the linked SDL3 library." >&2; exit 1; }
tmp=$(mktemp -d "${TMPDIR:-/tmp}/lastbeer-appdir.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
appdir="$tmp/LastEichhof.AppDir"
mkdir -p "$appdir/usr/bin" "$appdir/usr/lib" "$appdir/usr/share/licenses/lastbeer" "$(dirname -- "$output")"
cp "$build_dir/beer-sdl" "$root/BEER.DAT" "$appdir/usr/bin/"
cp -L "$sdl" "$appdir/usr/lib/libSDL3.so.0"
cp "$root/packaging/linux/AppRun" "$appdir/AppRun"
chmod +x "$appdir/AppRun" "$appdir/usr/bin/beer-sdl"
cp "$root/packaging/linux/lastbeer.desktop" "$appdir/lastbeer.desktop"
cp "$root/packaging/linux/lastbeer.svg" "$appdir/lastbeer.svg"
cp "$root/README.md" "$root/LICENSE.GPL" "$root/packaging/SDL3-LICENSE.txt" "$appdir/usr/share/licenses/lastbeer/"
if [ -n "${APPIMAGE_RUNTIME:-}" ]; then
   ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$tool" --runtime-file "$APPIMAGE_RUNTIME" "$appdir" "$output"
else
   ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$tool" "$appdir" "$output"
fi
printf 'AppImage: %s\n' "$output"
