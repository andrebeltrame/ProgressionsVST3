#!/usr/bin/env bash
# Builds the macOS installer from a freshly built VST3.
#
#   packaging/macos/build-pkg.sh <path-to-Progressions.vst3> <version> <output.pkg>
#
# Signing is opt-in through the environment, so the same script runs on a
# machine with certificates and on one without:
#
#   MACOS_SIGN_IDENTITY     "Developer ID Application: ..."  signs the plugin
#   MACOS_INSTALLER_IDENTITY "Developer ID Installer: ..."   signs the package
#
# Without them the package still installs, but Gatekeeper will warn - see
# README. Notarisation is a separate step and needs an Apple account.

set -euo pipefail

BUNDLE="${1:?usage: build-pkg.sh <Progressions.vst3> <version> <output.pkg>}"
VERSION="${2:?missing version}"
OUTPUT="${3:?missing output path}"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STAGING="$(mktemp -d)"
trap 'rm -rf "$STAGING"' EXIT

# The vendor folder is part of the install layout, not something the user makes.
mkdir -p "$STAGING/root/Nowhr Dynamics"
ditto "$BUNDLE" "$STAGING/root/Nowhr Dynamics/$(basename "$BUNDLE")"

if [[ -n "${MACOS_SIGN_IDENTITY:-}" ]]; then
    echo "Signing the plugin as $MACOS_SIGN_IDENTITY"
    codesign --force --options runtime --timestamp \
             --sign "$MACOS_SIGN_IDENTITY" \
             "$STAGING/root/Nowhr Dynamics/$(basename "$BUNDLE")"
else
    echo "No MACOS_SIGN_IDENTITY: shipping the ad-hoc signature the build made."
fi

# The seal has to be intact whether we just signed it or not: a bundle with a
# broken signature is refused by macOS and skipped by the host in silence.
codesign -v --strict "$STAGING/root/Nowhr Dynamics/$(basename "$BUNDLE")"

pkgbuild --identifier com.nowhrdynamics.progressions.vst3 \
         --version "$VERSION" \
         --root "$STAGING/root" \
         --install-location "/Library/Audio/Plug-Ins/VST3" \
         "$STAGING/Progressions-VST3.pkg"

productbuild --distribution "$HERE/distribution.xml" \
             --package-path "$STAGING" \
             --resources "$HERE" \
             "$STAGING/Progressions-unsigned.pkg"

if [[ -n "${MACOS_INSTALLER_IDENTITY:-}" ]]; then
    echo "Signing the installer as $MACOS_INSTALLER_IDENTITY"
    productsign --sign "$MACOS_INSTALLER_IDENTITY" \
                "$STAGING/Progressions-unsigned.pkg" "$OUTPUT"
else
    echo "No MACOS_INSTALLER_IDENTITY: the installer is unsigned."
    cp "$STAGING/Progressions-unsigned.pkg" "$OUTPUT"
fi

echo "Wrote $OUTPUT"
