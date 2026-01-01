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
APP_DISPLAY_NAME="XenAdmin"
APP_BUNDLE="${APP_DISPLAY_NAME}.app"
DMG_NAME="${APP_DISPLAY_NAME}-${APP_VERSION}"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --qt)
            QT_BIN_PATH="$2"
            shift 2
            ;;
        --version)
            APP_VERSION="$2"
            DMG_NAME="${APP_DISPLAY_NAME}-${APP_VERSION}"
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
echo "Building XenAdmin for macOS"
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
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run qmake to generate Makefiles
echo "Running $QMAKE_CMD..."
cd "$PROJECT_ROOT/src"
$QMAKE_CMD xenadminqt.pro -o "$BUILD_DIR/Makefile"

cd "$BUILD_DIR"

# Clean and build
#make clean || true
make -j$NCPUS

BUILT_APP_BUNDLE="$BUILD_DIR/xenadmin-ui/xenadmin-qt.app"
BUILT_BINARY="$BUILD_DIR/xenadmin-ui/xenadmin-qt"

if [ -d "$BUILT_APP_BUNDLE" ]; then
    BUILT_EXECUTABLE="$BUILT_APP_BUNDLE/Contents/MacOS/xenadmin-qt"
    if [ ! -f "$BUILT_EXECUTABLE" ]; then
        echo "Error: Build failed - xenadmin-qt app bundle is missing executable"
        exit 1
    fi
elif [ -f "$BUILT_BINARY" ]; then
    BUILT_EXECUTABLE="$BUILT_BINARY"
else
    echo "Error: Build failed - xenadmin-qt binary/app bundle not found"
    exit 1
fi

echo ""
echo "Step 2: Creating application bundle..."

# Create app bundle structure or copy an existing bundle
BUNDLE_DIR="$TEMP_DIR/$APP_BUNDLE"
if [ -d "$BUILT_APP_BUNDLE" ]; then
    ditto "$BUILT_APP_BUNDLE" "$BUNDLE_DIR"
else
    mkdir -p "$BUNDLE_DIR/Contents/MacOS"
    mkdir -p "$BUNDLE_DIR/Contents/Resources"
    mkdir -p "$BUNDLE_DIR/Contents/Frameworks"
    cp "$BUILT_EXECUTABLE" "$BUNDLE_DIR/Contents/MacOS/"
fi

chmod 755 "$BUNDLE_DIR/Contents/MacOS/xenadmin-qt"
mkdir -p "$BUNDLE_DIR/Contents/Resources"
ICON_SOURCE="$PROJECT_ROOT/packaging/macos/xenadmin.icns"
ICON_BUNDLE_NAME="AppIcon"
ICON_BUNDLE_PATH="$BUNDLE_DIR/Contents/Resources/${ICON_BUNDLE_NAME}.icns"
if [ -f "$ICON_SOURCE" ]; then
    cp "$ICON_SOURCE" "$ICON_BUNDLE_PATH"
else
    echo "Warning: icon file not found at $ICON_SOURCE. Icon will not be included."
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
    <string>$APP_DISPLAY_NAME</string>
    <key>CFBundleDisplayName</key>
    <string>$APP_DISPLAY_NAME</string>
    <key>CFBundleVersion</key>
    <string>$APP_VERSION</string>
    <key>CFBundleShortVersionString</key>
    <string>$APP_VERSION</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleIconFile</key>
    <string>${ICON_BUNDLE_NAME}</string>
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

# Calculate DMG size (app size + 50MB overhead)
APP_SIZE=$(du -sm "$DMG_STAGING" | awk '{print $1}')
DMG_SIZE=$((APP_SIZE + 50))

# Create temporary read-write DMG
TEMP_DMG="$TEMP_DIR/temp.dmg"
hdiutil create -srcfolder "$DMG_STAGING" \
    -volname "$APP_DISPLAY_NAME" \
    -fs HFS+ \
    -fsargs "-c c=64,a=16,e=16" \
    -format UDRW \
    -size ${DMG_SIZE}m \
    "$TEMP_DMG"

echo ""
echo "Step 6: Creating final DMG..."

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
echo "  2. Drag '$APP_DISPLAY_NAME' to Applications folder"
echo "  3. Launch from Applications or Spotlight"
echo ""
echo "Note: On first launch, you may need to right-click the app"
echo "      and select 'Open' to bypass Gatekeeper."
echo ""
