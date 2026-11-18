#!/usr/bin/env bash
# Builds KamustIA-macOS.pkg, which installs the VST3 and AU bundles system-wide.
# Run from the repository root after the CMake build has finished.
set -euo pipefail

VERSION="1.0.0"
ART="build/Kamust_IA_artefacts/Release"
STAGE="build/pkg-stage"
OUT="KamustIA-macOS.pkg"

rm -rf "$STAGE"
mkdir -p "$STAGE/vst3/Library/Audio/Plug-Ins/VST3"
mkdir -p "$STAGE/au/Library/Audio/Plug-Ins/Components"

cp -R "$ART/VST3/Kamust IA.vst3" "$STAGE/vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$ART/AU/Kamust IA.component" "$STAGE/au/Library/Audio/Plug-Ins/Components/"

pkgbuild --root "$STAGE/vst3" \
         --identifier com.kamust.kamustia.vst3 \
         --version "$VERSION" \
         --install-location / \
         "$STAGE/vst3.pkg"

pkgbuild --root "$STAGE/au" \
         --identifier com.kamust.kamustia.au \
         --version "$VERSION" \
         --install-location / \
         "$STAGE/au.pkg"

productbuild --package "$STAGE/vst3.pkg" \
             --package "$STAGE/au.pkg" \
             "$OUT"

echo "Created $OUT"
