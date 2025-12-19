#!/bin/bash
################################################################################
# package-dmg.sh - Build and package XenAdmin Qt for macOS
################################################################################

set -e

# Get script directory and source shared config
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config"

# Default values
QT_BIN_PATH=""
NCPUS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
APP_BUNDLE="XenAdmin-Qt.app"
DMG_NAME="XenAdmin-Qt-${APP_VERSION}"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --qt)
            QT_BIN_PATH="$2"
            shift 2
            ;;
        --version)
            APP_VERSION="$2"
            DMG_NAME="XenAdmin-Qt-${APP_VERSION}"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--qt /path/to/qt/bin] [--version x.y.z]"
            exit 1
            ;;
    esac
done

# Setup Qt environment
if [ -n "$QT_BIN_PATH" ]; then
    export PATH="$QT_BIN_PATH:$PATH"
fi

# Detect qmake (try qmake6 first, then qmake)
if [ -z "$QT_BIN_PATH" ]; then
    if command -v qmake6 &> /dev/null; then
        QMAKE_CMD="qmake6"
    elif command -v qmake &> /dev/null; then
        QMAKE_CMD="qmake"
    else
        echo "Error: Neither qmake nor qmake6 found in PATH"
        echo "Please install Qt6 or use --qt to specify Qt bin path"
        echo "Example: --qt ~/Qt/6.5.0/macos/bin"
        exit 1
    fi
else
    # If Qt path explicitly specified, prefer qmake from that path
    if command -v qmake &> /dev/null; then
        QMAKE_CMD="qmake"
    else
        echo "Error: qmake not found in specified Qt path: $QT_BIN_PATH"
        exit 1
    fi
fi

# Verify macdeployqt is available
if ! command -v macdeployqt &> /dev/null; then
    echo "Error: macdeployqt not found in PATH"
    echo "Please install Qt6 or use --qt to specify Qt bin path"
    exit 1
fi

# Verify we're on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Error: This script must be run on macOS"
    exit 1
fi

echo "============================="
echo "Building XenAdmin Qt for macOS"
echo "============================="
echo "Qt command: $QMAKE_CMD ($(which $QMAKE_CMD | xargs dirname))"
echo "CPUs: $NCPUS"
echo "Version: $APP_VERSION"
echo ""

# Get project root directory (parent of packaging/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/release"

# Create temp directory for packaging
TEMP_DIR=$(mktemp -d -t xenadmin-dmg-XXXXXX)
trap "rm -rf $TEMP_DIR" EXIT

echo "Step 1: Building application..."
cd "$BUILD_DIR"

# Run qmake to generate Makefiles
echo "Running $QMAKE_CMD..."
cd "$PROJECT_ROOT/src"
$QMAKE_CMD xenadminqt.pro -o "$BUILD_DIR/Makefile"

cd "$BUILD_DIR"

# Clean and build
make clean || true
make -j$NCPUS

if [ ! -f "$BUILD_DIR/xenadmin-ui/xenadmin-qt" ]; then
    echo "Error: Build failed - xenadmin-qt binary not found"
    exit 1
fi

echo ""
echo "Step 2: Creating application bundle..."

# Create app bundle structure
BUNDLE_DIR="$TEMP_DIR/$APP_BUNDLE"
mkdir -p "$BUNDLE_DIR/Contents/MacOS"
mkdir -p "$BUNDLE_DIR/Contents/Resources"
mkdir -p "$BUNDLE_DIR/Contents/Frameworks"

# Copy binary
cp "$BUILD_DIR/xenadmin-ui/xenadmin-qt" "$BUNDLE_DIR/Contents/MacOS/"
chmod 755 "$BUNDLE_DIR/Contents/MacOS/xenadmin-qt"
if [ -f "$PROJECT_ROOT/$APP_ICON_PATH" ]; then
    if command -v sips &> /dev/null && command -v iconutil &> /dev/null; then
        ICONSET="$TEMP_DIR/AppIcon.iconset"
        mkdir -p "$ICONSET"
        # Extract and resize icon to different sizes for .icns
        sips -z 16 16     "$PROJECT_ROOT/$APP_ICON_PATH" --out "$ICONSET/icon_16x16.png" 2>/dev/null || true
        sips -z 32 32     "$PROJECT_ROOT/$APP_ICON_PATH" --out "$ICONSET/icon_16x16@2x.png" 2>/dev/null || true
        sips -z 32 32     "$PROJECT_ROOT/$APP_ICON_PATH" --out "$ICONSET/icon_32x32.png" 2>/dev/null || true
        sips -z 64 64     "$PROJECT_ROOT/$APP_ICON_PATH" --out "$ICONSET/icon_32x32@2x.png" 2>/dev/null || true
        sips -z 128 128   "$PROJECT_ROOT/$APP_ICON_PATH" --out "$ICONSET/icon_128x128.png" 2>/dev/null || true
        sips -z 256 256   "$PROJECT_ROOT/$APP_ICON_PATH" --out "$ICONSET/icon_128x128@2x.png" 2>/dev/null || true
        sips -z 256 256   "$PROJECT_ROOT/$APP_ICON_PATH" --out "$ICONSET/icon_256x256.png" 2>/dev/null || true
        sips -z 512 512   "$PROJECT_ROOT/$APP_ICON_PATH" --out "$ICONSET/icon_256x256@2x.png" 2>/dev/null || true
        sips -z 512 512   "$PROJECT_ROOT/$APP_ICON_PATH" --out "$ICONSET/icon_512x512.png" 2>/dev/null || true
        iconutil -c icns "$ICONSET" -o "$BUNDLE_DIR/Contents/Resources/xenadmin.icns"
    else
        echo "Warning: sips or iconutil not found. Icon will not be included."
    fi
fi

echo ""
echo "Step 3: Creating Info.plist..."

# Create Info.plist
cat > "$BUNDLE_DIR/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>xenadmin-qt</string>
    <key>CFBundleIdentifier</key>
    <string>org.xenadmin.qt</string>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundleDisplayName</key>
    <string>$APP_NAME</string>
    <key>CFBundleVersion</key>
    <string>$APP_VERSION</string>
    <key>CFBundleShortVersionString</key>
    <string>$APP_VERSION</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleIconFile</key>
    <string>xenadmin.icns</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.15</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
    <key>LSApplicationCategoryType</key>
    <string>public.app-category.utilities</string>
</dict>
</plist>
EOF

echo ""
echo "Step 4: Running macdeployqt..."

# Use macdeployqt to bundle Qt frameworks and plugins
macdeployqt "$BUNDLE_DIR" -verbose=2

echo ""
echo "Step 5: Creating DMG..."

# Create temporary DMG mount point
DMG_STAGING="$TEMP_DIR/dmg-staging"
mkdir -p "$DMG_STAGING"

# Copy app bundle to staging
cp -R "$BUNDLE_DIR" "$DMG_STAGING/"

# Create Applications symlink for easy installation
ln -s /Applications "$DMG_STAGING/Applications"

# Copy README if available
if [ -f "$PROJECT_ROOT/README.md" ]; then
    cp "$PROJECT_ROOT/README.md" "$DMG_STAGING/README.txt"
fi

# Calculate DMG size (app size + 50MB overhead)
APP_SIZE=$(du -sm "$DMG_STAGING" | awk '{print $1}')
DMG_SIZE=$((APP_SIZE + 50))

# Create temporary read-write DMG
TEMP_DMG="$TEMP_DIR/temp.dmg"
hdiutil create -srcfolder "$DMG_STAGING" \
    -volname "$APP_NAME" \
    -fs HFS+ \
    -fsargs "-c c=64,a=16,e=16" \
    -format UDRW \
    -size ${DMG_SIZE}m \
    "$TEMP_DMG"

# Mount the temporary DMG
MOUNT_DIR=$(hdiutil attach -readwrite -noverify -noautoopen "$TEMP_DMG" | \
    grep -E '^/dev/' | sed 1q | awk '{print $3}')

# Wait for mount
sleep 2

echo ""
echo "Step 6: Customizing DMG appearance..."

# Set DMG window properties using AppleScript
osascript <<EOF
tell application "Finder"
    tell disk "$APP_NAME"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {100, 100, 700, 500}
        set viewOptions to the icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to 128
        set position of item "$APP_BUNDLE" of container window to {150, 200}
        set position of item "Applications" of container window to {450, 200}
        update without registering applications
        delay 2
    end tell
end tell
EOF

# Unmount the temporary DMG
hdiutil detach "$MOUNT_DIR" || true
sleep 2

# Convert to compressed read-only DMG
OUTPUT_DIR="$PROJECT_ROOT/packaging/output"
mkdir -p "$OUTPUT_DIR"
OUTPUT_DMG="$OUTPUT_DIR/${DMG_NAME}.dmg"

# Remove old DMG if exists
rm -f "$OUTPUT_DMG"

# Create final compressed DMG
hdiutil convert "$TEMP_DMG" \
    -format UDZO \
    -imagekey zlib-level=9 \
    -o "$OUTPUT_DMG"

# Make the DMG readable by all
chmod 644 "$OUTPUT_DMG"

echo ""
echo "============================="
echo "Build complete!"
echo "============================="
echo "Package: $OUTPUT_DMG"
echo "Size: $(du -h "$OUTPUT_DMG" | awk '{print $1}')"
echo ""
echo "To install:"
echo "  1. Double-click the DMG file"
echo "  2. Drag '$APP_NAME' to Applications folder"
echo "  3. Launch from Applications or Spotlight"
echo ""
echo "Note: On first launch, you may need to right-click the app"
echo "      and select 'Open' to bypass Gatekeeper."
echo ""
