# XenAdmin Qt - Packaging Scripts

This directory contains scripts to build native packages for different platforms.

## Prerequisites

### All Platforms
- Qt6 development tools (qmake, etc.)
- C++ compiler toolchain

### Debian/Ubuntu (.deb)
- `dpkg-deb` (usually pre-installed)
- Build dependencies:
  ```bash
  sudo apt-get install qt6-base-dev build-essential freerdp2-dev libwinpr2-dev mesa-common-dev libglu1-mesa-dev libegl1 libxkbcommon0
  ```

### Fedora/RHEL/CentOS (.rpm)
- `rpmbuild` and related tools
- Build dependencies:
  ```bash
  sudo dnf install rpm-build qt6-qtbase-devel freerdp2-devel libwinpr2-devel mesa-libGL-devel mesa-libGLU-devel mesa-libEGL mesa-libEGL-devel libxkbcommon qt6-qtcharts-devel rsync
  ```

### macOS (.dmg) (includes `sips` and `iconutil` for icon conversion)
- macOS 10.15 or later
- Xcode Command Line Tools
- Qt6 for macOS with `macdeployqt`

## Usage

### Build Debian Package

```bash
cd packaging/
./package-deb.sh
```

With custom Qt path:
```bash
./package-deb.sh --qt ~/Qt/6.5.0/gcc_64/bin
```

With custom version:
```bash
./package-deb.sh --version 0.2.0
```

**Output:** `packaging/output/xenadmin-qt_0.1.0_amd64.deb`

**Installation:**
```bash
sudo dpkg -i packaging/output/xenadmin-qt_0.1.0_amd64.deb
sudo apt-get install -f  # Install any missing dependencies
```

### Build Fedora/RHEL Package

```bash
cd packaging/
./package-rpm.sh
```

With custom Qt path:
```bash
./package-rpm.sh --qt ~/Qt/6.5.0/gcc_64/bin
```

With custom version:
```bash
./package-rpm.sh --version 0.2.0
```

**Output:** `packaging/output/xenadmin-qt-0.1.0-1.*.rpm`

The Fedora script now drives `rpmbuild` end-to-end, so it also emits a matching SRPM:
- Binary RPM: `packaging/output/xenadmin-qt-0.1.0-1.*.rpm`
- Source RPM: `packaging/output/xenadmin-qt-0.1.0-1.*.src.rpm`

**Installation:**
```bash
sudo dnf install packaging/output/xenadmin-qt-0.1.0-1.*.rpm
# or
sudo rpm -ivh packaging/output/xenadmin-qt-0.1.0-1.*.rpm
```

### Build macOS DMG

```bash
cd packaging/
./package-dmg.sh
```

With custom Qt path:
```bash
./package-dmg.sh --qt ~/Qt/6.5.0/macos/bin
```

With custom version:
```bash
./package-dmg.sh --version 0.2.0
```

**Output:** `packaging/output/XenAdmin-Qt-0.1.0.dmg`

**Installation:**
1. Double-click the DMG file
2. Drag "XenAdmin Qt" to Applications folder
3. Launch from Applications or Spotlight

**Note:** On first launch, you may need to right-click the app and select "Open" to bypass macOS Gatekeeper.

## Script Options

All scripts support the following options:

- `--qt <path>`: Path to Qt bin directory (e.g., `~/Qt/6.5.0/gcc_64/bin`)
  - If not specified, uses Qt from system PATH
  - Must contain `qmake` and platform-specific deploy tools

- `--version <x.y.z>`: Package version number
  - Default: `0.1.0`
  - Must follow semantic versioning (major.minor.patch)

## Build Process

All scripts follow this general workflow:

1. **Validate environment**: Check for required tools (qmake, rpmbuild, etc.)
2. **Build application**: Run `make -j<ncpus>` in `release/` directory
3. **Create package structure**: Set up platform-specific directory layout
4. **Copy binaries and libraries**: Include xenadmin-qt binary and libxenlib
5. **Bundle dependencies**: Platform-specific dependency handling
6. **Generate metadata**: Create control files, Info.plist, etc.
7. **Create package**: Build .deb, .rpm, or .dmg
8. **Output to**: `packaging/output/`

## Temporary Files

All scripts use `/tmp` for temporary build artifacts:
- Debian: `/tmp/xenadmin-deb-XXXXXX`
- Fedora: `/tmp/xenadmin-rpm-XXXXXX`
- macOS: `/tmp/xenadmin-dmg-XXXXXX`

Temporary directories are automatically cleaned up on exit.

## Parallel Builds

Scripts automatically detect CPU count and use parallel compilation:
- Linux: Uses `nproc` to detect CPU count
- macOS: Uses `sysctl -n hw.ncpu` to detect CPU count
- Runs: `make -j<ncpus>`

## Package Contents

All packages include:
- Main executable: `xenadmin-qt` (with xenlib statically linked)
- Documentation: README.md, LICENSE
- Application icon: Converted from source `.ico` file
- Desktop integration: Application menu entry (Linux)

### Icon Handling

The scripts automatically convert the application icon (`AppIcon.ico`) to the appropriate format:

- **Linux (.deb/.rpm)**: Converts to 48x48 PNG using ImageMagick `convert`
  - If ImageMagick is not installed, a warning is shown but packaging continues
  - Icon placed in: `/usr/share/pixmaps/xenadmin-qt.png`

- **macOS (.dmg)**: Converts to `.icns` bundle with multiple resolutions using `sips` and `iconutil`
  - Creates icon sizes: 16x16, 32x32, 64x64, 128x128, 256x256, 512x512 (including @2x variants)
  - If tools are not available, a warning is shown but packaging continues
  - Icon placed in: `XenAdmin-Qt.app/Contents/Resources/xenadmin.icns`

### Linux Packages
- Desktop file: Integrates with application menus
- Icon file: PNG format in `/usr/share/pixmaps/`

### macOS Package
- Application bundle: Complete .app structure
- Framework bundling: Qt frameworks embedded via `macdeployqt`
- DMG presentation: Custom window layout with Applications symlink

## Troubleshooting

### Qt not found
```bash
Error: qmake not found in PATH
```
**Solution:** Install Qt6 or use `--qt` to specify Qt bin path

### Build fails
```bash
Error: Build failed - xenadmin-qt binary not found
```
**Solution:** Ensure project builds successfully in `release/` directory first:
```bash
cd release/
make clean
make -j12
```

### Missing rpmbuild (Fedora)
```bash
Error: rpmbuild not found
```
**Solution:** Install rpm-build tools:
```bash
sudo dnf install rpm-build rpmdevtools
```

### macOS Gatekeeper blocks app
**Solution:** Right-click the app and select "Open" on first launch, or:
```bash
xattr -cr /Applications/XenAdmin-Qt.app
```

### Wrong Qt version
**Solution:** Specify correct Qt path:
```bash
./package-deb.sh --qt /opt/Qt/6.5.0/gcc_64/bin
```

## Development

### Testing packages locally

**Debian:**
```bash
# Install
sudo dpkg -i packaging/output/xenadmin-qt_*.deb
# Remove
sudo apt-get remove xenadmin-qt
```

**Fedora:**
```bash
# Install
sudo rpm -ivh packaging/output/xenadmin-qt-*.rpm
# Remove
sudo rpm -e xenadmin-qt
```
