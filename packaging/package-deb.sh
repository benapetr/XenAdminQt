#!/bin/bash
################################################################################
# package-deb.sh - Build and package XenAdmin Qt for Debian/Ubuntu
################################################################################

set -e

# Get script directory and source shared config
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config"

# Default values
QT_BIN_PATH=""
NCPUS=$(nproc)

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --qt)
            QT_BIN_PATH="$2"
            shift 2
            ;;
        --version)
            APP_VERSION="$2"
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

# Detect qmake (try qmake6 first for newer Debian, then qmake)
if [ -z "$QT_BIN_PATH" ]; then
    if command -v qmake6 &> /dev/null; then
        QMAKE_CMD="qmake6"
    elif command -v qmake &> /dev/null; then
        QMAKE_CMD="qmake"
    else
        echo "Error: Neither qmake nor qmake6 found in PATH"
        echo "Please install Qt6 (qt6-base-dev) or use --qt to specify Qt bin path"
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

echo "================================"
echo "Building XenAdmin Qt for Debian"
echo "================================"
echo "Qt command: $QMAKE_CMD ($(which $QMAKE_CMD | xargs dirname))"
echo "CPUs: $NCPUS"
echo "Version: $APP_VERSION"
echo ""

# Get project root directory (parent of packaging/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/release"

# Create temp directory for packaging
TEMP_DIR=$(mktemp -d -t xenadmin-deb-XXXXXX)
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
make clean || true
make -j$NCPUS

if [ ! -f "$BUILD_DIR/xenadmin-ui/xenadmin-qt" ]; then
    echo "Error: Build failed - xenadmin-qt binary not found"
    exit 1
fi

echo ""
echo "Step 2: Creating package structure..."

# Create debian package structure
PKG_DIR="$TEMP_DIR/${APP_NAME}_${APP_VERSION}_amd64"
mkdir -p "$PKG_DIR/DEBIAN"
mkdir -p "$PKG_DIR/usr/bin"
mkdir -p "$PKG_DIR/usr/share/applications"
mkdir -p "$PKG_DIR/usr/share/pixmaps"
mkdir -p "$PKG_DIR/usr/share/doc/$APP_NAME"

# Copy binary
cp "$BUILD_DIR/xenadmin-ui/xenadmin-qt" "$PKG_DIR/usr/bin/"
chmod 755 "$PKG_DIR/usr/bin/xenadmin-qt"
if [ -f "$PROJECT_ROOT/README.md" ]; then
    cp "$PROJECT_ROOT/README.md" "$PKG_DIR/usr/share/doc/$APP_NAME/"
fi
if [ -f "$PROJECT_ROOT/LICENSE" ]; then
    cp "$PROJECT_ROOT/LICENSE" "$PKG_DIR/usr/share/doc/$APP_NAME/copyright"
fi

# Copy icon (pre-rendered PNG)
ICON_SOURCE="$PROJECT_ROOT/packaging/xenadmin.png"
if [ -f "$ICON_SOURCE" ]; then
    cp "$ICON_SOURCE" "$PKG_DIR/usr/share/pixmaps/$APP_NAME.png"
else
    echo "Warning: $ICON_SOURCE not found. Icon will not be included."
fi

# Create desktop file
cat > "$PKG_DIR/usr/share/applications/$APP_NAME.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=XenAdmin Qt
Comment=XenServer/XCP-ng Management Tool
Exec=$APP_NAME
Icon=$APP_NAME
Categories=System;Network;RemoteAccess;
Terminal=false
EOF

echo ""
echo "Step 3: Creating control file..."

# Create control file
cat > "$PKG_DIR/DEBIAN/control" <<EOF
Package: $APP_NAME
Version: $APP_VERSION
Section: net
Priority: optional
Architecture: amd64
Depends: libc6 (>= 2.27), libstdc++6 (>= 8), libqt6core6, libqt6gui6, \
 libqt6widgets6, libqt6network6, libqt6charts6, libfreerdp3-3 | libfreerdp2-2, \
 libfreerdp-client3-3 | libfreerdp-client2-2, libwinpr3-3 | libwinpr2-2, \
 libglu1-mesa, libegl1, libxkbcommon0
Maintainer: $MAINTAINER
Description: $DESCRIPTION
 XenAdmin Qt is a C++/Qt6 rewrite of the original XenAdmin client,
 providing XenServer and XCP-ng management capabilities on Linux,
 macOS, and other Qt-supported platforms.
 .
 This package includes the main application and required libraries.
EOF

echo ""
echo "Step 4: Building .deb package..."

# Build the package (force root ownership metadata for rootless builds)
dpkg-deb --build --root-owner-group "$PKG_DIR"

# Move package to project packaging directory
OUTPUT_DIR="$PROJECT_ROOT/packaging/output"
mkdir -p "$OUTPUT_DIR"
mv "$TEMP_DIR/${APP_NAME}_${APP_VERSION}_amd64.deb" "$OUTPUT_DIR/"

echo ""
echo "================================"
echo "Build complete!"
echo "================================"
echo "Package: $OUTPUT_DIR/${APP_NAME}_${APP_VERSION}_amd64.deb"
echo ""
echo "To install:"
echo "  sudo dpkg -i $OUTPUT_DIR/${APP_NAME}_${APP_VERSION}_amd64.deb"
echo "  sudo apt-get install -f  # Install dependencies if needed"
echo ""
