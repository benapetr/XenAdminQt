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

# Resolve distro info for dependency hints
DISTRO_ID=""
DISTRO_VERSION=""
if [ -r /etc/os-release ]; then
    . /etc/os-release
    DISTRO_ID="${ID:-}"
    DISTRO_VERSION="${VERSION_ID:-}"
fi

echo "==============================="
echo "Building XenAdmin Qt for RPM"
echo "==============================="
echo "CPUs: $NCPUS"
echo "Version: $APP_VERSION-$RELEASE"
echo ""

# Helper to check package availability in repos (best-effort)
pkg_available() {
    local pkg="$1"
    if command -v dnf >/dev/null 2>&1; then
        dnf -q list --available "$pkg" >/dev/null 2>&1
        return $?
    fi
    if command -v yum >/dev/null 2>&1; then
        yum -q list available "$pkg" >/dev/null 2>&1
        return $?
    fi
    return 1
}

# Decide Qt major version (prefer Qt6 when available)
QT_MAJOR=0
if [ -n "$QT_BIN_PATH" ]; then
    if command -v qmake6 >/dev/null 2>&1; then
        QT_MAJOR=6
    elif command -v qmake-qt5 >/dev/null 2>&1; then
        QT_MAJOR=5
    fi
else
    if command -v qmake6 >/dev/null 2>&1; then
        QT_MAJOR=6
    elif command -v qmake-qt5 >/dev/null 2>&1; then
        QT_MAJOR=5
    fi
fi
if [ "$QT_MAJOR" -eq 0 ]; then
    if pkg_available qt6-qtbase-devel; then
        QT_MAJOR=6
    elif pkg_available qt5-qtbase-devel; then
        QT_MAJOR=5
    fi
fi

# Preflight dependency check (prints install command but does not install)
if command -v rpm >/dev/null 2>&1; then
    missing=()
    missing_groups=()
    optional_groups=()

    require_pkg() {
        local pkg="$1"
        if ! rpm -q "$pkg" >/dev/null 2>&1; then
            missing+=("$pkg")
        fi
    }

    require_one_of() {
        local group=("$@")
        local found=""
        local pkg
        for pkg in "${group[@]}"; do
            if rpm -q "$pkg" >/dev/null 2>&1; then
                found="yes"
                break
            fi
        done
        if [ -z "$found" ]; then
            missing_groups+=("${group[*]}")
        fi
    }

    optional_one_of() {
        local group=("$@")
        local found=""
        local pkg
        for pkg in "${group[@]}"; do
            if rpm -q "$pkg" >/dev/null 2>&1; then
                found="yes"
                break
            fi
        done
        if [ -z "$found" ]; then
            optional_groups+=("${group[*]}")
        fi
    }

    require_pkg rpm-build
    require_pkg rpmdevtools
    require_pkg rsync
    require_pkg git
    require_pkg pkgconf-pkg-config
    require_pkg mesa-libGL-devel
    require_pkg mesa-libGLU-devel
    require_pkg mesa-libEGL-devel
    require_pkg libxkbcommon-devel

    if [ "$QT_MAJOR" -eq 5 ]; then
        require_pkg qt5-qtbase-devel
        require_pkg qt5-qtcharts-devel
    else
        require_pkg qt6-qtbase-devel
        require_pkg qt6-qtcharts-devel
    fi

    optional_one_of freerdp-devel freerdp2-devel
    optional_one_of winpr-devel libwinpr2-devel

    if [ ${#missing[@]} -gt 0 ] || [ ${#missing_groups[@]} -gt 0 ]; then
        echo "Missing build dependencies detected."
        echo ""
        if command -v dnf >/dev/null 2>&1; then
            echo "Install the following packages and re-run:"
            if [ ${#missing[@]} -gt 0 ]; then
                echo "  sudo dnf install ${missing[*]}"
            fi
            if [ ${#missing_groups[@]} -gt 0 ]; then
                echo ""
                echo "Choose one package from each group:"
                for group in "${missing_groups[@]}"; do
                    for pkg in $group; do
                        echo "  sudo dnf install $pkg"
                    done
                    echo ""
                done
            fi
        elif command -v yum >/dev/null 2>&1; then
            echo "Install the following packages and re-run:"
            if [ ${#missing[@]} -gt 0 ]; then
                echo "  sudo yum install ${missing[*]}"
            fi
            if [ ${#missing_groups[@]} -gt 0 ]; then
                echo ""
                echo "Choose one package from each group:"
                for group in "${missing_groups[@]}"; do
                    for pkg in $group; do
                        echo "  sudo yum install $pkg"
                    done
                    echo ""
                done
            fi
        else
            echo "Install the following packages (or equivalent) and re-run:"
            for pkg in "${missing[@]}"; do
                echo "  - $pkg"
            done
            for group in "${missing_groups[@]}"; do
                echo "  - one of: $group"
            done
        fi
        if [ "$QT_MAJOR" -eq 5 ] && printf '%s\n' "${missing[@]}" | grep -q '^qt5-qtcharts-devel$'; then
            echo ""
            if [ "$DISTRO_ID" = "rhel" ]; then
                echo "Hint: qt5-qtcharts-devel is in CodeReady Builder."
                echo "  sudo subscription-manager repos --enable codeready-builder-for-rhel-9-$(arch)-rpms"
            elif [ "$DISTRO_ID" = "almalinux" ] || [ "$DISTRO_ID" = "rocky" ]; then
                echo "Hint: qt5-qtcharts-devel is typically in CRB/EPEL."
                echo "  sudo dnf install -y dnf-plugins-core"
                echo "  sudo dnf config-manager --set-enabled crb"
                echo "  sudo dnf install epel-release"
            fi
        fi
        echo ""
        exit 1
    fi

    if [ ${#optional_groups[@]} -gt 0 ]; then
        echo "Optional build dependencies missing (RDP support will be disabled):"
        for group in "${optional_groups[@]}"; do
            echo "  - one of: $group"
        done
        echo ""
    fi
fi

# Detect qmake (prefer Qt6, then Qt5)
if [ -z "$QT_BIN_PATH" ]; then
    if command -v qmake6 >/dev/null 2>&1; then
        QMAKE_CMD="qmake6"
    elif command -v qmake-qt5 >/dev/null 2>&1; then
        QMAKE_CMD="qmake-qt5"
    elif command -v qmake >/dev/null 2>&1; then
        QMAKE_CMD="qmake"
    else
        echo "Error: qmake not found in PATH."
        echo "Install the Qt development packages noted above or use --qt to specify Qt bin path."
        exit 1
    fi
else
    # If Qt path explicitly specified, prefer qmake from that path
    if command -v qmake6 >/dev/null 2>&1; then
        QMAKE_CMD="qmake6"
    elif command -v qmake >/dev/null 2>&1; then
        QMAKE_CMD="qmake"
    elif command -v qmake-qt5 >/dev/null 2>&1; then
        QMAKE_CMD="qmake-qt5"
    else
        echo "Error: qmake not found in specified Qt path: $QT_BIN_PATH"
        exit 1
    fi
fi

# Resolve absolute qmake path for use inside rpmbuild
QMAKE_PATH="$(command -v "$QMAKE_CMD")"

echo "Qt command: $QMAKE_CMD ($(dirname "$QMAKE_PATH"))"
if [ "$QT_MAJOR" -eq 5 ]; then
    echo "Qt major: 5"
elif [ "$QT_MAJOR" -eq 6 ]; then
    echo "Qt major: 6"
fi
echo ""

# Get project root directory (parent of packaging/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Always use ~/rpmbuild (containers typically have /tmp on small tmpfs)
RPM_TOPDIR="$HOME/rpmbuild"
if command -v rpmdev-setuptree >/dev/null 2>&1; then
    rpmdev-setuptree >/dev/null 2>&1 || true
else
    mkdir -p "$RPM_TOPDIR"
fi
TEMP_DIR=$(mktemp -d -p "$RPM_TOPDIR" xenadmin-rpm-XXXXXX)
trap "rm -rf $TEMP_DIR" EXIT

echo "Step 1: Preparing source archive..."

# Create RPM build directories
RPM_BUILD_ROOT="$RPM_TOPDIR"
mkdir -p "$RPM_BUILD_ROOT"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

SOURCE_STAGE_DIR="$TEMP_DIR/source"
SOURCE_DIR_NAME="${APP_NAME}-${APP_VERSION}"
SOURCE_ROOT="$SOURCE_STAGE_DIR/$SOURCE_DIR_NAME"
mkdir -p "$SOURCE_ROOT"

SOURCE_TARBALL="$RPM_BUILD_ROOT/SOURCES/$SOURCE_DIR_NAME.tar.gz"

if ! command -v git >/dev/null 2>&1; then
    echo "Error: git not found."
    echo "Please install git (e.g., sudo dnf install git) and re-run."
    exit 1
fi
if ! git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "Error: $PROJECT_ROOT is not a git repository."
    exit 1
fi

git -C "$PROJECT_ROOT" archive --format=tar --prefix "$SOURCE_DIR_NAME/" HEAD | gzip -n > "$SOURCE_TARBALL"

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

%if 0%{?qt_major} == 5
BuildRequires:  qt5-qtbase-devel
BuildRequires:  qt5-qtcharts-devel
%else
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtcharts-devel
%endif
BuildRequires:  mesa-libGL-devel
BuildRequires:  mesa-libGLU-devel
BuildRequires:  mesa-libEGL-devel
BuildRequires:  libxkbcommon-devel
BuildRequires:  pkgconf-pkg-config

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
         --define "qt_major $QT_MAJOR" \
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
