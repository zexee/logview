#!/bin/sh
# Fetch vendored third-party dependencies for lv's optional GUI build
# (-DLV_BUILD_GUI=ON). Idempotent: re-running only clones what's missing and
# re-applies the PDCursesMod patch.
#
# Default proxy is empty; set HTTPS_PROXY env var or pass --proxy <url> to
# route downloads through a corporate proxy.

set -e

THIRDPARTY_DIR="$(cd "$(dirname "$0")/.." && pwd)/third_party"
PATCHES_DIR="$(cd "$(dirname "$0")/.." && pwd)/third_party_patches"
PROXY=""

while [ $# -gt 0 ]; do
    case "$1" in
        --proxy) PROXY="$2"; shift 2 ;;
        *) echo "Unknown arg: $1" >&2; exit 1 ;;
    esac
done

if [ -n "$PROXY" ]; then
    export HTTPS_PROXY="$PROXY"
    export HTTP_PROXY="$PROXY"
fi

mkdir -p "$THIRDPARTY_DIR"
cd "$THIRDPARTY_DIR"

clone_if_missing() {
    # clone_if_missing <dir> <url> <tag>
    if [ ! -d "$1" ] || [ ! -d "$1/.git" ]; then
        echo "== Cloning $1 @ $3"
        git clone --depth=1 --branch "$3" "$2" "$1"
    fi
}

clone_if_missing SDL2        https://github.com/libsdl-org/SDL.git          release-2.30.0
clone_if_missing SDL2_ttf    https://github.com/libsdl-org/SDL_ttf.git      release-2.22.0
clone_if_missing PDCursesMod https://github.com/Bill-Gray/PDCursesMod.git   v4.4.0

# SDL2_ttf needs FreeType as a git submodule (vendored build).
if [ ! -f SDL2_ttf/external/freetype/CMakeLists.txt ]; then
    echo "== Fetching SDL2_ttf/freetype submodule"
    git -C SDL2_ttf submodule update --init --depth=1 external/freetype
fi

# PDCursesMod's gl backend swaps the SDL window itself, which conflicts with
# ImGui drawing on top of the same back buffer. Apply our patch that adds a
# pdc_no_swap flag.
if [ ! -f PDCursesMod/.lv-patch-applied ]; then
    echo "== Applying lv-no-swap patch to PDCursesMod"
    git -C PDCursesMod checkout .
    git -C PDCursesMod apply "$PATCHES_DIR/pdcursesmod-lv-no-swap.patch"
    touch PDCursesMod/.lv-patch-applied
fi

echo "== Done. third_party/ is ready for -DLV_BUILD_GUI=ON."
