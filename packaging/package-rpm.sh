#!/bin/bash
################################################################################
# package-rpm.sh - Build and package XenAdmin Qt for Fedora/RHEL/CentOS
################################################################################

set -e

# Get script directory and source shared config
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config"

# Default values
QT_BIN_PATH=""
NCPUS=$(nproc)
RELEASE="1"

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

# Detect qmake (try qmake6 first, then qmake)
if [ -z "$QT_BIN_PATH" ]; then
    if command -v qmake6 &> /dev/null; then
        QMAKE_CMD="qmake6"
    elif command -v qmake &> /dev/null; then
        QMAKE_CMD="qmake"
    else
        echo "Error: Neither qmake nor qmake6 found in PATH"
        echo "Please install Qt6 (qt6-qtbase-devel) or use --qt to specify Qt bin path"
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

# Resolve absolute qmake path for use inside rpmbuild
QMAKE_PATH="$(command -v "$QMAKE_CMD")"

# Verify rpmbuild is installed
if ! command -v rpmbuild &> /dev/null; then
    echo "Error: rpmbuild not found"
    echo "Please install rpm-build: sudo dnf install rpm-build"
    exit 1
fi

# Require rsync for staging the source archive
if ! command -v rsync &> /dev/null; then
    echo "Error: rsync not found"
    echo "Please install rsync before running this script."
    exit 1
fi

echo "==============================="
echo "Building XenAdmin Qt for Fedora"
echo "==============================="
echo "Qt command: $QMAKE_CMD ($(dirname "$QMAKE_PATH"))"
echo "CPUs: $NCPUS"
echo "Version: $APP_VERSION-$RELEASE"
echo ""

# Get project root directory (parent of packaging/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Create temp directory for packaging
TEMP_DIR=$(mktemp -d -t xenadmin-rpm-XXXXXX)
trap "rm -rf $TEMP_DIR" EXIT

echo "Step 1: Preparing source archive..."

# Create RPM build directories
RPM_BUILD_ROOT="$TEMP_DIR/rpmbuild"
mkdir -p "$RPM_BUILD_ROOT"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

SOURCE_STAGE_DIR="$TEMP_DIR/source"
SOURCE_DIR_NAME="${APP_NAME}-${APP_VERSION}"
SOURCE_ROOT="$SOURCE_STAGE_DIR/$SOURCE_DIR_NAME"
mkdir -p "$SOURCE_ROOT"

rsync -a \
    --exclude ".git" \
    --exclude "packaging/output" \
    --exclude "packaging/output/*" \
    "$PROJECT_ROOT/" "$SOURCE_ROOT/"

# Ensure we have a clean release directory for out-of-source builds
rm -rf "$SOURCE_ROOT/release"
mkdir -p "$SOURCE_ROOT/release"

SOURCE_TARBALL="$RPM_BUILD_ROOT/SOURCES/$SOURCE_DIR_NAME.tar.gz"
tar -C "$SOURCE_STAGE_DIR" -czf "$SOURCE_TARBALL" "$SOURCE_DIR_NAME"

echo ""
echo "Step 2: Creating RPM spec file..."

SPEC_PATH="$RPM_BUILD_ROOT/SPECS/$APP_NAME.spec"
cat > "$SPEC_PATH" <<EOF
%global qmake_cmd $QMAKE_PATH
%global debug_package %{nil}

Name:           $APP_NAME
Version:        $APP_VERSION
Release:        $RELEASE%{?dist}
Summary:        $DESCRIPTION

License:        GPLv2+
URL:            https://github.com/xenadmin/xenadmin-qt
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtcharts-devel
BuildRequires:  freerdp2-devel
BuildRequires:  libwinpr2-devel
BuildRequires:  mesa-libGL-devel
BuildRequires:  mesa-libGLU-devel
BuildRequires:  mesa-libEGL-devel
BuildRequires:  libxkbcommon-devel
Requires:       qt6-qtbase >= 6.0
Requires:       qt6-qtbase-gui >= 6.0
Requires:       qt6-qtcharts >= 6.0

%description
XenAdmin Qt is a C++/Qt6 rewrite of the original XenAdmin client,
providing XenServer and XCP-ng management capabilities on Linux,
macOS, and other Qt-supported platforms.

This package includes the main application and required libraries.

%prep
%autosetup

%build
mkdir -p release
pushd src
%{qmake_cmd} xenadminqt.pro -o ../release/Makefile
popd
%make_build -C release

%install
rm -rf %{buildroot}
install -Dm755 release/xenadmin-ui/xenadmin-qt %{buildroot}%{_bindir}/$APP_NAME
install -Dm644 README.md %{buildroot}%{_docdir}/$APP_NAME/README.md
install -Dm644 LICENSE %{buildroot}%{_docdir}/$APP_NAME/LICENSE

install -d %{buildroot}%{_datadir}/applications
cat <<'DESKTOP' > %{buildroot}%{_datadir}/applications/$APP_NAME.desktop
[Desktop Entry]
Type=Application
Name=XenAdmin Qt
Comment=XenServer/XCP-ng Management Tool
Exec=$APP_NAME
Icon=$APP_NAME
Categories=System;Network;RemoteAccess;
Terminal=false
DESKTOP

install -d %{buildroot}%{_datadir}/pixmaps
if [ -f packaging/xenadmin.png ]; then
    install -Dm644 packaging/xenadmin.png %{buildroot}%{_datadir}/pixmaps/$APP_NAME.png
else
    echo "Warning: packaging/xenadmin.png not found. Icon will not be included."
fi

%files
%license %{_docdir}/$APP_NAME/LICENSE
%doc %{_docdir}/$APP_NAME/README.md
%{_bindir}/$APP_NAME
%{_datadir}/applications/$APP_NAME.desktop
%{_datadir}/pixmaps/$APP_NAME.png

%changelog
* $(date '+%a %b %d %Y') $MAINTAINER - $APP_VERSION-$RELEASE
- Initial RPM package
EOF

echo ""
echo "Step 3: Building RPM and SRPM..."

rpmbuild --define "_topdir $RPM_BUILD_ROOT" \
         -ba "$SPEC_PATH"

# Move packages to project packaging directory
OUTPUT_DIR="$PROJECT_ROOT/packaging/output"
mkdir -p "$OUTPUT_DIR"
cp "$RPM_BUILD_ROOT/RPMS/x86_64/${APP_NAME}-${APP_VERSION}-${RELEASE}"*.rpm "$OUTPUT_DIR/"
cp "$RPM_BUILD_ROOT/SRPMS/${APP_NAME}-${APP_VERSION}-${RELEASE}"*.src.rpm "$OUTPUT_DIR/"

echo ""
echo "==============================="
echo "Build complete!"
echo "==============================="
echo "Binary RPM: $OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${RELEASE}*.rpm"
echo "Source RPM: $OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${RELEASE}*.src.rpm"
echo ""
echo "To install:"
echo "  sudo dnf install $OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${RELEASE}*.rpm"
echo "  # or"
echo "  sudo rpm -ivh $OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${RELEASE}*.rpm"
echo ""
