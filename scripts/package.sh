#!/bin/bash
# Packaging for 3D-Pinball-macos.
#
#   ./scripts/package.sh --release          PUBLIC build: dist/3D-Pinball-macos-<version>.dmg
#       Contains NO game data and none of the game's artwork; the app asks the user to copy their own game files into
#       ~/Library/Application Support/3D-Pinball-macos on first launch. This is the only build that may be shared.
#
#   ./scripts/package.sh <game-folder>      PERSONAL build: dist/3D Pinball.app + dist/3D-Pinball-personal.dmg
#       Copies your game files (and an icon built from your pinball.exe) into the app. FOR YOUR OWN USE ONLY: it contains
#       the original game's copyrighted data. Never publish it.
#
# Needs: Xcode command line tools, CMake, Homebrew sdl2-compat + sdl3. Nothing is downloaded; temp files stay under build/.
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="0.2.0"; BREW=/opt/homebrew
MODE="personal"; GAME=""
if [ "${1:-}" = "--release" ]; then MODE="release"; else GAME="${1:-../Pinball}"; fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null && cmake --build build >/dev/null
mkdir -p dist build

if [ "$MODE" = "release" ]; then APP="dist/release/3D-Pinball-macos.app"; EXE="3DPinball"; else APP="dist/3D Pinball.app"; EXE="3DPinball"; fi
rm -rf "$APP"; mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
cp build/pinball "$APP/Contents/MacOS/$EXE"; strip -x "$APP/Contents/MacOS/$EXE"
cp "$BREW/opt/sdl2-compat/lib/libSDL2-2.0.0.dylib" "$APP/Contents/Frameworks/"
cp "$BREW/opt/sdl3/lib/libSDL3.0.dylib" "$APP/Contents/Frameworks/libSDL3.dylib"   # sdl2-compat looks for exactly this name
chmod u+w "$APP"/Contents/Frameworks/*.dylib; strip -x "$APP"/Contents/Frameworks/*.dylib 2>/dev/null || true

# relink so the bundle never touches Homebrew
install_name_tool -id @rpath/libSDL2-2.0.0.dylib "$APP/Contents/Frameworks/libSDL2-2.0.0.dylib"
install_name_tool -id @rpath/libSDL3.dylib "$APP/Contents/Frameworks/libSDL3.dylib"
OLD=$(otool -L "$APP/Contents/MacOS/$EXE" | awk '/libSDL2/{print $1}')
install_name_tool -change "$OLD" @rpath/libSDL2-2.0.0.dylib "$APP/Contents/MacOS/$EXE"
install_name_tool -add_rpath @executable_path/../Frameworks "$APP/Contents/MacOS/$EXE"
for l in "$APP"/Contents/Frameworks/*.dylib; do
  otool -L "$l" | awk '/\/opt\/homebrew/{print $1}' | grep -v "$(basename "$l")" | while read d; do install_name_tool -change "$d" "@rpath/$(basename "$d")" "$l"; done || true
done

# ---- icon ----
TMP="$(mktemp -d build/tmp.XXXXXX)"; ICONSET="$TMP/AppIcon.iconset"; mkdir -p "$ICONSET"
if [ "$MODE" = "release" ]; then
  # original artwork drawn by research/tools/make_release_icon.py (not the game's icon)
  python3 -I research/tools/make_release_icon.py "$TMP/base.png" >/dev/null
  for sz in 16:icon_16x16 32:icon_16x16@2x 32:icon_32x32 64:icon_32x32@2x 128:icon_128x128 256:icon_128x128@2x 256:icon_256x256 512:icon_256x256@2x 512:icon_512x512; do
    sips -z ${sz%%:*} ${sz%%:*} "$TMP/base.png" --out "$ICONSET/${sz##*:}.png" >/dev/null; done
  iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/AppIcon.icns"
else
  # built from the icon inside YOUR copy of pinball.exe; skipped if unavailable
  cp -R "$GAME"/*.DAT "$GAME"/*.MID "$GAME"/*.WAV "$APP/Contents/Resources/" 2>/dev/null || true
  if [ -f "$GAME/pinball.exe" ] && PINBALL_DIR="$GAME" python3 -I research/tools/make_icon.py "$TMP" >/dev/null 2>&1; then
    cp "$TMP/src16.png" "$ICONSET/icon_16x16.png"; cp "$TMP/src32.png" "$ICONSET/icon_16x16@2x.png"; cp "$TMP/src32.png" "$ICONSET/icon_32x32.png"
    for sz in 64:icon_32x32@2x 128:icon_128x128 256:icon_128x128@2x 256:icon_256x256; do sips -z ${sz%%:*} ${sz%%:*} "$TMP/src48.png" --out "$ICONSET/${sz##*:}.png" >/dev/null; done
    iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/AppIcon.icns" 2>/dev/null || echo "note: icon not built"
  else echo "note: no icon (needs $GAME/pinball.exe)"; fi
fi
rm -rf "$TMP"

# ---- Info.plist ----
if [ "$MODE" = "release" ]; then NAME="3D-Pinball-macos"; BID="io.github.3d-pinball-macos"; else NAME="3D Pinball"; BID="local.pinball.personal"; fi
cat > "$APP/Contents/Info.plist" <<PL
<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleName</key><string>$NAME</string><key>CFBundleDisplayName</key><string>$NAME</string>
<key>CFBundleIdentifier</key><string>$BID</string><key>CFBundleVersion</key><string>$VERSION</string>
<key>CFBundleShortVersionString</key><string>$VERSION</string><key>CFBundleExecutable</key><string>$EXE</string>
<key>CFBundleIconFile</key><string>AppIcon</string><key>CFBundlePackageType</key><string>APPL</string><key>LSMinimumSystemVersion</key><string>12.0</string>
<key>NSHighResolutionCapable</key><true/><key>LSApplicationCategoryType</key><string>public.app-category.games</string>
</dict></plist>
PL
codesign --force --sign - "$APP/Contents/Frameworks/libSDL2-2.0.0.dylib" "$APP/Contents/Frameworks/libSDL3.dylib" 2>/dev/null
codesign --force --sign - "$APP"   # ad-hoc signature (required to run on arm64)

# ---- disk image ----
ROOT="dist/dmgroot"; rm -rf "$ROOT"; mkdir -p "$ROOT"
if [ "$MODE" = "release" ]; then
  # safety check: a public image must not contain any original game file
  if find "$APP" \( -iname '*.dat' -o -iname '*.wav' -o -iname '*.mid' -o -iname '*.exe' -o -iname '*.bmp' \) | grep -q .; then echo "ERROR: game data found in release bundle"; exit 1; fi
  cp -R "$APP" "$ROOT/"; ln -s /Applications "$ROOT/Applications"
  cp LICENSE "$ROOT/LICENSE.txt"
  { echo "3D-Pinball-macos $VERSION"; echo
    echo "A macOS (Apple Silicon) port of the classic 3D Pinball for Windows - Space Cadet. This app does NOT include the game."
    echo
    echo "1. Drag 3D-Pinball-macos to Applications."
    echo "2. Start it once. It will tell you where to put your game files:"
    echo "     ~/Library/Application Support/3D-Pinball-macos"
    echo "   Copy PINBALL.DAT, FONT.DAT and the .WAV / .MID files from your own copy of the Windows game into that folder."
    echo "3. Start the app again."
    echo
    echo "First launch: macOS may say the app cannot be verified. Right-click the app and choose Open, or run:"
    echo "     xattr -dr com.apple.quarantine /Applications/3D-Pinball-macos.app"
    echo
    echo "Controls: Z / / flippers, Space plunger, X . Up nudge, F2 new game, P pause, F4 full screen, M music, Esc quit."
    echo "Source and documentation: see the project's README."; } > "$ROOT/README.txt"
  { echo "Third-party software bundled in 3D-Pinball-macos.app"; echo
    echo "=== SDL3 (libSDL3.dylib) ==="; cat "$BREW/opt/sdl3/LICENSE.txt"; echo
    echo "=== sdl2-compat (libSDL2-2.0.0.dylib) ==="; cat "$BREW/opt/sdl2-compat/LICENSE.txt"; } > "$ROOT/THIRD-PARTY-NOTICES.txt"
  DMG="dist/3D-Pinball-macos-$VERSION.dmg"; VOL="3D-Pinball-macos $VERSION"
else
  cp -R "$APP" "$ROOT/"; ln -s /Applications "$ROOT/Applications"
  DMG="dist/3D-Pinball-personal.dmg"; VOL="3D Pinball (personal)"
fi
rm -f "$DMG"; hdiutil create -quiet -volname "$VOL" -srcfolder "$ROOT" -ov -format UDZO "$DMG"; rm -rf "$ROOT"
echo "Built: $APP"; echo "Built: $DMG"; du -sh "$APP" "$DMG"
[ "$MODE" = "personal" ] && echo "NOTE: the personal build contains the original game's data. Do not share it."
exit 0
