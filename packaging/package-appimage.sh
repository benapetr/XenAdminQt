#!/bin/bash
################################################################################
# package-appimage.sh - Build XenAdmin Qt as an AppImage
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config"

PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$PROJECT_ROOT/packaging/output"
BUILD_DIR="$PROJECT_ROOT/packaging/.appimage-build"
APPDIR="$PROJECT_ROOT/packaging/.AppDir"
DESKTOP_FILE_SRC="$PROJECT_ROOT/debian/xenadmin-qt.desktop"
ICON_FILE_SRC="$PROJECT_ROOT/packaging/xenadmin.png"
ICON_SIZE=256

QT_BIN_PATH=""
NCPUS=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

cleanup() {
    if [ -n "${TEMP_ICON_FILE:-}" ] && [ -f "$TEMP_ICON_FILE" ]; then
        rm -f "$TEMP_ICON_FILE"
    fi
}

trap cleanup EXIT

find_image_tool() {
    if command -v magick >/dev/null 2>&1; then
        echo "magick"
        return 0
    fi
    if command -v convert >/dev/null 2>&1; then
        echo "convert"
        return 0
    fi
    if command -v ffmpeg >/dev/null 2>&1; then
        echo "ffmpeg"
        return 0
    fi
    return 1
}

resize_icon() {
    local input_file="$1"
    local output_file="$2"
    local tool="$3"

    case "$tool" in
        magick)
            magick "$input_file" \
                -background none \
                -resize "${ICON_SIZE}x${ICON_SIZE}" \
                -gravity center \
                -extent "${ICON_SIZE}x${ICON_SIZE}" \
                PNG32:"$output_file"
            ;;
        convert)
            convert "$input_file" \
                -background none \
                -resize "${ICON_SIZE}x${ICON_SIZE}" \
                -gravity center \
                -extent "${ICON_SIZE}x${ICON_SIZE}" \
                PNG32:"$output_file"
            ;;
        ffmpeg)
            ffmpeg -y -loglevel error \
                -i "$input_file" \
                -vf "scale=${ICON_SIZE}:${ICON_SIZE}:force_original_aspect_ratio=decrease:flags=lanczos,pad=${ICON_SIZE}:${ICON_SIZE}:(ow-iw)/2:(oh-ih)/2:color=black@0.0" \
                "$output_file"
            ;;
        *)
            echo "Error: unsupported image tool: $tool"
            return 1
            ;;
    esac
}

while [[ $# -gt 0 ]]; do
    case "$1" in
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

if [ -n "$QT_BIN_PATH" ]; then
    export PATH="$QT_BIN_PATH:$PATH"
fi

echo "================================="
echo "Building XenAdmin Qt AppImage"
echo "================================="
echo "CPUs: $NCPUS"
echo "Version: $APP_VERSION"
echo ""

if [ -z "$QT_BIN_PATH" ]; then
    if command -v qmake6 >/dev/null 2>&1; then
        QMAKE_CMD="qmake6"
    elif command -v qmake-qt5 >/dev/null 2>&1; then
        QMAKE_CMD="qmake-qt5"
    elif command -v qmake >/dev/null 2>&1; then
        QMAKE_CMD="qmake"
    else
        echo "Error: qmake not found in PATH."
        echo "Install Qt development tools or use --qt to specify Qt bin path."
        exit 1
    fi
else
    if command -v qmake6 >/dev/null 2>&1; then
        QMAKE_CMD="qmake6"
    elif command -v qmake-qt5 >/dev/null 2>&1; then
        QMAKE_CMD="qmake-qt5"
    elif command -v qmake >/dev/null 2>&1; then
        QMAKE_CMD="qmake"
    else
        echo "Error: qmake not found in specified Qt path: $QT_BIN_PATH"
        exit 1
    fi
fi

if ! command -v linuxdeploy >/dev/null 2>&1; then
    echo "Error: linuxdeploy not found in PATH."
    echo "Install linuxdeploy and re-run."
    exit 1
fi

if ! command -v linuxdeploy-plugin-qt >/dev/null 2>&1; then
    echo "Error: linuxdeploy-plugin-qt not found in PATH."
    echo "Install linuxdeploy-plugin-qt and re-run."
    exit 1
fi

if ! command -v linuxdeploy-plugin-appimage >/dev/null 2>&1; then
    echo "Error: linuxdeploy-plugin-appimage not found in PATH."
    echo "Install linuxdeploy-plugin-appimage and re-run."
    exit 1
fi

if [ ! -f "$DESKTOP_FILE_SRC" ]; then
    echo "Error: Desktop file not found: $DESKTOP_FILE_SRC"
    exit 1
fi

if [ ! -f "$ICON_FILE_SRC" ]; then
    echo "Error: Icon file not found: $ICON_FILE_SRC"
    exit 1
fi

if ! IMAGE_TOOL="$(find_image_tool)"; then
    echo "Error: no supported image resizing tool found in PATH."
    echo "Install one of: ImageMagick ('magick' or 'convert') or ffmpeg."
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
rm -rf "$BUILD_DIR" "$APPDIR"
mkdir -p "$BUILD_DIR" \
         "$APPDIR/usr/bin" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps"

echo "Step 1: Building application..."
cd "$BUILD_DIR"
"$QMAKE_CMD" "$PROJECT_ROOT/src/xenadminqt.pro"
make -j"$NCPUS"

APP_BINARY="$BUILD_DIR/xenadmin-ui/$APP_NAME"
if [ ! -f "$APP_BINARY" ]; then
    echo "Error: Built binary not found: $APP_BINARY"
    exit 1
fi

echo ""
echo "Step 2: Creating AppDir..."
install -Dm755 "$APP_BINARY" "$APPDIR/usr/bin/$APP_NAME"
install -Dm644 "$DESKTOP_FILE_SRC" "$APPDIR/usr/share/applications/$APP_NAME.desktop"
TEMP_ICON_FILE="$(mktemp "$BUILD_DIR/${APP_NAME}-icon-XXXXXX.png")"
echo "Generating temporary ${ICON_SIZE}x${ICON_SIZE} icon using $IMAGE_TOOL..."
resize_icon "$ICON_FILE_SRC" "$TEMP_ICON_FILE" "$IMAGE_TOOL"
install -Dm644 "$TEMP_ICON_FILE" "$APPDIR/usr/share/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps/$APP_NAME.png"

ln -sf usr/share/applications/$APP_NAME.desktop "$APPDIR/$APP_NAME.desktop"
ln -sf usr/share/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps/$APP_NAME.png "$APPDIR/$APP_NAME.png"

echo ""
echo "Step 3: Bundling AppImage..."
cd "$PROJECT_ROOT"
rm -f "$PROJECT_ROOT"/*.AppImage
export QMAKE
QMAKE="$(command -v "$QMAKE_CMD")"

linuxdeploy \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/$APP_NAME.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps/$APP_NAME.png" \
    --plugin qt \
    --output appimage

mapfile -t APPIMAGE_FILES < <(find "$PROJECT_ROOT" -maxdepth 1 -type f -name "*.AppImage" | sort) || true
if [ ${#APPIMAGE_FILES[@]} -eq 0 ]; then
    echo "Error: linuxdeploy did not produce an AppImage."
    exit 1
fi

ARCH="$(uname -m)"
OUTPUT_PATH="$OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${ARCH}.AppImage"
mv "${APPIMAGE_FILES[0]}" "$OUTPUT_PATH"

echo ""
echo "================================="
echo "Build complete!"
echo "================================="
echo "AppImage: $OUTPUT_PATH"
echo ""
echo "To run:"
echo "  chmod +x $OUTPUT_PATH"
echo "  $OUTPUT_PATH"
echo ""
