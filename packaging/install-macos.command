#!/bin/bash
#
# Progressions - installs the plug-in on macOS and says what it found.
#
# Double-click this file in Finder. It sits next to Progressions.vst3 and does
# the three things that have to happen, in the order they have to happen in.
#
# The reason this exists: two of those three steps fail in silence. A plug-in
# copied to a folder the host does not scan is simply never seen, and one still
# carrying the downloaded-file quarantine is refused by macOS without a word -
# the DAW just never lists it. Typed out as separate commands, one of them
# fails in the middle and the failure scrolls past. So this does the whole
# thing and then prints what actually ended up on disk.

set -uo pipefail

say()  { printf '%s\n' "$1"; }
good() { printf '  \033[32mOK\033[0m    %s\n' "$1"; }
bad()  { printf '  \033[31mBAD\033[0m   %s\n' "$1"; }

finish() {
    say ""
    read -r -p "Press Enter to close" _
    exit "${1:-0}"
}

say ""
say "Progressions - macOS install"
say "============================"
say ""

here=$(cd "$(dirname "$0")" && pwd)
source_bundle="$here/Progressions.vst3"

if [ ! -d "$source_bundle" ]; then
    bad "No Progressions.vst3 next to this script."
    say ""
    say "  This has to live in the same folder as the plug-in."
    say "  Folder searched: $here"
    finish 1
fi
good "Found the plug-in"

# The two binaries a .vst3 bundle carries, one per system. The Windows one is
# not needed here, but its absence means the download is not the merged build.
if [ ! -f "$source_bundle/Contents/MacOS/Progressions" ]; then
    bad "The bundle has no macOS binary inside it - this copy is incomplete."
    finish 1
fi
if /usr/bin/file "$source_bundle/Contents/MacOS/Progressions" | grep -q "Mach-O universal"; then
    good "It is a universal binary - Intel and Apple Silicon"
else
    say "  NOTE  Not a universal binary. It will still run if it matches this Mac."
fi

target_dir="$HOME/Library/Audio/Plug-Ins/VST3/Nowhr Dynamics"
target="$target_dir/Progressions.vst3"

if ! mkdir -p "$target_dir"; then
    bad "Could not create $target_dir"
    finish 1
fi

# An old copy has to go first, or the two get mixed together file by file and
# the result is neither version.
if [ -d "$target" ]; then
    rm -rf "$target" && good "Removed the copy that was already there"
fi

if ! cp -R "$source_bundle" "$target_dir/"; then
    bad "Could not copy into $target_dir"
    finish 1
fi
good "Copied to $target"

# The step that fails most silently of all: macOS refuses a quarantined plug-in
# and the DAW skips it with no error anywhere.
if xattr -dr com.apple.quarantine "$target" 2>/dev/null; then
    good "Cleared the downloaded-file quarantine"
else
    say "  NOTE  Nothing was quarantined."
fi

say ""
say "Installed. What is on disk now:"
say ""
/bin/ls -la "$target_dir"
say ""
/usr/bin/file "$target/Contents/MacOS/Progressions" | sed 's/^/  /'
if [ -f "$target/Contents/x86_64-win/Progressions.vst3" ]; then
    good "The Windows binary is in there too - the same folder serves both systems"
fi

say ""
say "In your DAW:"
say ""
say "  1. Restart it."
say "  2. In Ableton Live: Preferences -> Plug-Ins, hold OPTION and click Rescan."
say "     Holding Option is what makes Live look again at a plug-in it has"
say "     already refused once; an ordinary rescan will not."
say "  3. Look under INSTRUMENTS. Progressions is an instrument, not an effect."
say ""
say "Still not listed after all three? Send back everything printed above."
finish 0
