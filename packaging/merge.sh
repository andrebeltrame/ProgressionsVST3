#!/usr/bin/env bash
#
# Puts the macOS and Windows builds into one VST3 bundle and zips it.
#
#   packaging/merge.sh <macos-bundle.zip> <windows Progressions.vst3> [out-dir]
#
# A VST3 bundle is designed to hold every platform's binary side by side, each
# under its own Contents subfolder, so one folder serves both systems and each
# picks up its own.
#
# This has to run on macOS: the signature is applied *after* the Windows binary
# goes in, because adding a file to a signed bundle breaks the seal - and a
# broken seal is refused by macOS with nothing said anywhere. The host simply
# never lists the plug-in.
#
# CI calls this, and so can you when CI cannot: download the macos-build and
# windows-build artifacts from the run page, then point this at them.
#
# Out of it come two things in <out-dir>: the zip, holding the plug-in and the
# instructions, and install-windows.ps1 beside it.

set -euo pipefail

if [ $# -lt 2 ]; then
    sed -n '3,6p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi

MAC_ZIP=$1
WIN_DLL=$2
OUT_DIR=${3:-dist}

if [ "$(uname)" != "Darwin" ]; then
    echo "This has to run on macOS: codesign is what seals the merged bundle." >&2
    exit 1
fi

for required in "$MAC_ZIP" "$WIN_DLL"; do
    [ -f "$required" ] || { echo "Not found: $required" >&2; exit 1; }
done

ROOT=$(cd "$(dirname "$0")/.." && pwd)
VERSION=$(sed -n 's/.*project(Harmonia VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")
[ -n "$VERSION" ] || { echo "Could not read the version from CMakeLists.txt" >&2; exit 1; }

PAYLOAD="Progressions-$VERSION"
rm -rf "$PAYLOAD"
mkdir -p "$PAYLOAD" "$OUT_DIR"

ditto -x -k "$MAC_ZIP" "$PAYLOAD"
BUNDLE="$PAYLOAD/Progressions.vst3"
[ -d "$BUNDLE" ] || { echo "No Progressions.vst3 inside $MAC_ZIP" >&2; exit 1; }

mkdir -p "$BUNDLE/Contents/x86_64-win"
cp "$WIN_DLL" "$BUNDLE/Contents/x86_64-win/Progressions.vst3"

if [ -n "${MACOS_SIGN_IDENTITY:-}" ]; then
    codesign --force --options runtime --timestamp --sign "$MACOS_SIGN_IDENTITY" "$BUNDLE"
else
    codesign --force --sign - "$BUNDLE"
fi
codesign -v --strict "$BUNDLE"

# Both binaries have to be in there and be the right kind of binary.
file "$BUNDLE/Contents/MacOS/Progressions"
file "$BUNDLE/Contents/x86_64-win/Progressions.vst3"
file "$BUNDLE/Contents/MacOS/Progressions" | grep -q "Mach-O universal"
file "$BUNDLE/Contents/x86_64-win/Progressions.vst3" | grep -q "PE32+"
lipo -archs "$BUNDLE/Contents/MacOS/Progressions" | grep -q arm64
lipo -archs "$BUNDLE/Contents/MacOS/Progressions" | grep -q x86_64

# Two things in the zip and no more: the plug-in, and the page that says where
# to put it. The zip is what gets forwarded to somebody, and whoever opens it
# should find what to install and how, without a third item asking to be
# understood first.
#
# Stamped rather than typed, so the instructions cannot claim one version while
# the plug-in beside them is another.
sed "s/@VERSION@/$VERSION/" "$ROOT/packaging/INSTALL.txt" > "$PAYLOAD/INSTALL.txt"
ditto -c -k --keepParent "$PAYLOAD" "$OUT_DIR/Progressions-$VERSION.zip"

# The installers go beside the zip instead of inside it, one per system. They
# are a click away on the release page for whoever hits trouble and out of the
# way of everyone who does not.
#
# Both exist for the same reason: on either system the ways this fails are
# silent. A plug-in in a folder the host does not scan is never seen, and one
# still carrying the downloaded-file mark is refused with nothing said anywhere
# - the DAW simply never lists it. Typed out as separate commands, one step
# fails in the middle and the failure scrolls past. The scripts do the whole
# thing and then print what actually ended up on disk.
cp "$ROOT/packaging/install-windows.ps1" "$OUT_DIR/install-windows.ps1"
cp "$ROOT/packaging/install-macos.command" "$OUT_DIR/install-macos.command"
chmod +x "$OUT_DIR/install-macos.command"

rm -rf "$PAYLOAD"

# Say what actually went into the zip. Every round of "what is in this thing?"
# so far has been answered by reading this script rather than by looking at what
# it produced, and the two are only the same until they are not.
echo "In the zip:"
unzip -Z1 "$OUT_DIR/Progressions-$VERSION.zip" | awk -F/ 'NF<=2' | sed 's/^/  /'
echo "Beside it:"
ls -lh "$OUT_DIR"
