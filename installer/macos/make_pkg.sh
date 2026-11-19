#!/usr/bin/env bash
# Builds dist/AlphaMonster-<version>-macOS.pkg (installer) and a .zip with the raw bundles.
set -euo pipefail
VERSION="${1:-1.0.0}"
OUT="build/Source/MasterPlugin/AlphaMonsterMaster_artefacts/Release"
MIX="build/Source/MixApp/AlphaMonsterMix_artefacts/Release/Alpha Monster Mix.app"
WORK="$(mktemp -d)"
mkdir -p dist "$WORK/pkgs"

stage() { # stage <root> <dest dir> <bundle>
  mkdir -p "$1$2"
  ditto "$3" "$1$2/$(basename "$3")"
}

stage "$WORK/au"   "/Library/Audio/Plug-Ins/Components" "$OUT/AU/Alpha Monster Master.component"
stage "$WORK/vst3" "/Library/Audio/Plug-Ins/VST3"       "$OUT/VST3/Alpha Monster Master.vst3"
stage "$WORK/apps" "/Applications/Alpha Monster"        "$OUT/Standalone/Alpha Monster Master.app"
stage "$WORK/apps" "/Applications/Alpha Monster"        "$MIX"

build_component() { # name id root
  pkgbuild --analyze --root "$3" "$WORK/$1.plist"
  # never let the installer "relocate" bundles to an old copy somewhere else
  /usr/libexec/PlistBuddy -c "Print" "$WORK/$1.plist" >/dev/null
  n=$(/usr/libexec/PlistBuddy -c "Print" "$WORK/$1.plist" | grep -c "RootRelativeBundlePath" || true)
  for ((i=0; i<n; i++)); do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/$1.plist" || true
  done
  pkgbuild --root "$3" --component-plist "$WORK/$1.plist" --identifier "$2" --version "$VERSION" \
           --install-location / "$WORK/pkgs/$1.pkg"
}

build_component au   com.alphamonster.master.au   "$WORK/au"
build_component vst3 com.alphamonster.master.vst3 "$WORK/vst3"
build_component apps com.alphamonster.apps        "$WORK/apps"

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>Alpha Monster ${VERSION}</title>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true"/>
  <choices-outline>
    <line choice="apps"/>
    <line choice="au"/>
    <line choice="vst3"/>
  </choices-outline>
  <choice id="apps" title="Alpha Monster Mix + Master (app)" description="App Alpha Monster Mix con il modello IA e Alpha Monster Master standalone, in /Applications/Alpha Monster">
    <pkg-ref id="com.alphamonster.apps"/>
  </choice>
  <choice id="au" title="Alpha Monster Master - Audio Unit" description="Per Logic Pro, GarageBand, Final Cut Pro, Ableton, Studio One...">
    <pkg-ref id="com.alphamonster.master.au"/>
  </choice>
  <choice id="vst3" title="Alpha Monster Master - VST3" description="Per Cubase, Ableton, FL Studio, Reaper, Studio One, DaVinci Resolve, Premiere...">
    <pkg-ref id="com.alphamonster.master.vst3"/>
  </choice>
  <pkg-ref id="com.alphamonster.apps">apps.pkg</pkg-ref>
  <pkg-ref id="com.alphamonster.master.au">au.pkg</pkg-ref>
  <pkg-ref id="com.alphamonster.master.vst3">vst3.pkg</pkg-ref>
</installer-gui-script>
XML

productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" "dist/AlphaMonster-${VERSION}-macOS.pkg"

# zip with the bundles for manual installation
ZIPROOT="$WORK/zip/Alpha Monster ${VERSION} macOS"
mkdir -p "$ZIPROOT"
ditto "$OUT/AU/Alpha Monster Master.component" "$ZIPROOT/Alpha Monster Master.component"
ditto "$OUT/VST3/Alpha Monster Master.vst3" "$ZIPROOT/Alpha Monster Master.vst3"
ditto "$OUT/Standalone/Alpha Monster Master.app" "$ZIPROOT/Alpha Monster Master.app"
ditto "$MIX" "$ZIPROOT/Alpha Monster Mix.app"
cp docs/INSTALL.md "$ZIPROOT/README.md"
cp docs/INSTALLAZIONE.md "$ZIPROOT/LEGGIMI.md"
(cd "$WORK/zip" && ditto -c -k --keepParent "Alpha Monster ${VERSION} macOS" "$OLDPWD/dist/AlphaMonster-${VERSION}-macOS.zip")
