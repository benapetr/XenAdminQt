# XenAdmin Qt - Packaging Scripts

This directory contains scripts to build native packages for different platforms.

## Prerequisites

### All Platforms
- Qt6 development tools (qmake, etc.)
- C++ compiler toolchain

### Debian/Ubuntu (.deb)
- `dpkg-buildpackage`/debhelper
- Build dependencies:
  ```bash
  sudo apt-get install build-essential debhelper dpkg-dev pkg-config \
    qtbase5-dev qt6-base-dev libqt5charts5-dev qt6-charts-dev \
    libfreerdp2-dev libfreerdp-dev libwinpr2-dev libwinpr-dev \
    libglu1-mesa-dev libegl1-mesa-dev libxkbcommon-dev
  ```

### Ubuntu Notes
On Ubuntu 22.04/24.04 the Qt dev packages may be in `universe`. If you see
missing `qtbase5-dev`/`qt6-base-dev`/`qt6-charts-dev`, enable `universe` first:
```bash
sudo apt-get install software-properties-common
sudo add-apt-repository universe
sudo apt-get update
```
If `qt6-charts-dev` is still unavailable on Ubuntu 22.04, build with Qt5
(`qtbase5-dev` + `libqt5charts5-dev`) or install Qt6/Qt6 Charts from a PPA.

### Fedora/RHEL/CentOS (.rpm)
- `rpmbuild` and related tools
- Build dependencies:
  ```bash
  sudo dnf install rpm-build rsync pkgconf-pkg-config \
    qt6-qtbase-devel qt6-qtcharts-devel \
    mesa-libGL-devel mesa-libGLU-devel mesa-libEGL-devel libxkbcommon-devel
  ```
  - Alma/Rocky/RHEL 8: use Qt5 (`qt5-qtbase-devel` + `qt5-qtcharts-devel`)
  - FreeRDP/WinPR support is optional (`freerdp-devel` + `winpr-devel`)

### macOS (.dmg) (includes `sips` and `iconutil` for icon conversion)
- macOS 10.15 or later
- Xcode Command Line Tools
- Qt6 for macOS with `macdeployqt`

### Windows (.zip)
- Windows 10 or later
- PowerShell 5.1 or later
- Qt6 for Windows (MSVC or MinGW build)
- Visual Studio Build Tools 2019+ (for MSVC) or MinGW (bundled with Qt)
- Optional: Qt static build for monolithic .exe

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

**Notes:**
- Uses `debhelper`/`dpkg-shlibdeps` so runtime dependencies are auto-detected via `${shlibs:Depends}` and `${misc:Depends}`
- The build selects `qmake6` when available, otherwise `qmake`

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

### Build Windows Package

```powershell
cd packaging
.\package-win.ps1 -QtPath C:\Qt\6.5.0\msvc2019_64
```

With static Qt build (creates monolithic .exe):
```powershell
.\package-win.ps1 -QtPath C:\Qt\6.5.0\msvc2019_64_static -Static
```

With custom version:
```powershell
.\package-win.ps1 -QtPath C:\Qt\6.5.0\msvc2019_64 -Version 0.2.0
```

Using MinGW compiler:
```powershell
.\package-win.ps1 -QtPath C:\Qt\6.5.0\mingw_64 -Compiler mingw
```

**Output:** `packaging/output/xenadmin-qt-0.1.0-windows.zip`

**Installation:**
1. Extract the ZIP file to a directory
2. Run `xenadmin-qt.exe`

**Notes:**
- For static builds, ensure you have a static Qt build installed (e.g., from Qt official installer with static option)
- MSVC builds require Visual Studio Build Tools or Visual Studio installed
- MinGW builds use the MinGW toolchain bundled with Qt
- Static builds create a single .exe file with all dependencies embedded (~20-40 MB)
- Dynamic builds include Qt DLLs and are smaller (~5-10 MB exe + ~15-30 MB of DLLs)

## Script Options

### Linux/macOS Scripts

All bash scripts support the following options:

- `--qt <path>`: Path to Qt bin directory (e.g., `~/Qt/6.5.0/gcc_64/bin`)
  - If not specified, uses Qt from system PATH
  - Must contain `qmake` and platform-specific deploy tools

- `--version <x.y.z>`: Package version number
  - Default: `0.1.0`
  - Must follow semantic versioning (major.minor.patch)

### Windows Script

The PowerShell script supports the following options:

- `-QtPath <path>`: Path to Qt installation directory (e.g., `C:\Qt\6.5.0\msvc2019_64`)
  - **Required** - must be specified
  - Must contain `bin\qmake.exe` and optionally `bin\windeployqt.exe`

- `-Version <x.y.z>`: Package version number
  - Default: `0.0.1`
  - Must follow semantic versioning (major.minor.patch)

- `-BuildType <type>`: Build type (Release or Debug)
  - Default: `Release`
  - Determines optimization level and debug symbols

- `-Compiler <type>`: Compiler toolchain (msvc or mingw)
  - Default: `msvc`
  - MSVC requires Visual Studio Build Tools
  - MinGW uses toolchain bundled with Qt

- `-Jobs <n>`: Number of parallel build jobs
  - Default: Number of CPU cores
  - For MinGW builds (MSVC nmake doesn't support parallel builds)

- `-Static`: Build static executable
  - Requires Qt static build
  - Creates monolithic .exe with all dependencies embedded
  - Results in larger executable but no external DLL dependencies

- `-Help`: Show help message with examples

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

All scripts use temporary directories for temporary build artifacts:
- Debian: `/tmp/xenadmin-deb-XXXXXX`
- Fedora: `/tmp/xenadmin-rpm-XXXXXX`
- macOS: `/tmp/xenadmin-dmg-XXXXXX`
- Windows: `$env:TEMP\xenadmin-win-XXXXXX` (handled by .NET temp directory)

Temporary directories are automatically cleaned up on exit.

## Parallel Builds

Scripts automatically detect CPU count and use parallel compilation:
- Linux: Uses `nproc` to detect CPU count
- macOS: Uses `sysctl -n hw.ncpu` to detect CPU count
- Windows: Uses `Get-CimInstance` to detect CPU count
- Runs: `make -j<ncpus>` (MinGW on Windows) or `nmake` (MSVC on Windows)

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

### Windows Package
- Executable: `xenadmin-qt.exe`
- Qt libraries: Bundled via `windeployqt` (dynamic build) or embedded (static build)
- ZIP archive: Self-contained portable application
- No installer required: Extract and run

## Troubleshooting

### Windows-specific Issues

#### Visual Studio not found
```
Error: nmake.exe not found
```
**Solution:** Run from Visual Studio Developer Command Prompt, or install Visual Studio Build Tools:
- Download from: https://visualstudio.microsoft.com/downloads/
- Select "Desktop development with C++" workload

#### Qt static build not available
**Solution:** Build Qt statically or use the Qt online installer:
1. Run Qt installer
2. Select Qt version (e.g., 6.5.0)
3. Choose "MSVC 2019 64-bit (static)" or similar static option
4. Complete installation
5. Use `-QtPath C:\Qt\6.5.0\msvc2019_64_static -Static`

#### windeployqt not found (dynamic builds)
**Solution:** Ensure Qt bin directory is in PATH or specified via `-QtPath`:
```powershell
.\package-win.ps1 -QtPath C:\Qt\6.5.0\msvc2019_64
```

#### Build directory issues
**Solution:** Clean the build directory:
```powershell
cd release
nmake clean
cd ..
.\packaging\package-win.ps1 -QtPath C:\Qt\6.5.0\msvc2019_64
```

### Cross-platform Issues

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
