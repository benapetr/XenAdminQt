# XenAdminQt C++/Qt6 Rewrite

**IMPORTANT** This is a beta version right now - many things work, but many things do not. Use at your own risk.

This is the C++/Qt6 rewrite of the original C# XenAdmin codebase, converted using AI assistance, but hand-fixed, manually reviewed and post-processed with many manual fixes. The fixes are still being done - the goal is to get this codebase into perfection.

The idea is to first port the entire existing C# codebase preserving all naming and logic (same naming for easier verification of what we already ported) - once we get a 1:1 port that works same as original client, we will move to next phase where we add new features and modify existing functionality.

The goal of this project is to get original thick XenAdmin client to work on non-Microsoft platforms (Mostly macOS and GNU/Linux - but any platform supported by Qt might work).

## What is XenAdmin

XenAdmin is a graphical desktop frontend for xapi - https://github.com/xapi-project which is a Xen API framework that is powering XCP-ng as well as Citrix Hypervisor platform (XenServer).

It connects to xapi over HTTP/S protocol using JSON-RPC API calls, provides access to host and VM consoles via XSVNC as well as RRDP performance metrics provided by xapi.

That means it can be used as a graphical frontend to directly manage standalone XCP-ng / XenServer hosts as well as clusters.

# Building

The primary build system is qmake. Open `src/xenadminqt.pro` in Qt Creator,
select a Qt6 or Qt5 kit with Qt Charts installed, and build the project.
Use a build directory outside `src/`.

CMake is an alternative introduced for the RISC-V cross-build pipeline. It is
not required for the normal Qt Creator or packaging workflow.

See [packaging readme](packaging/README.md) for details

There is a packaging folder that is used to create packages for Linux (.deb and .rpm) as well as macOS

Both variants support similar arguments, most importantly the `--qt <path to qt's bin>`

For example (macOS):

```
cd packaging

# Replace the Qt version with whatever you have installed
./package-dmg.sh --qt ~/Qt/6.7.2/macos/bin/
```

### Optional: build without crypto support

If you need to compile without OpenSSL/CommonCrypto/BCrypt (master password feature will be disabled), use this flag:

```
qmake CONFIG+=no_crypto
```

## Linux dependencies

### Debian/Ubuntu

For a Qt6 build:

```bash
sudo apt-get install build-essential pkg-config \
  qt6-base-dev qt6-charts-dev libssl-dev zlib1g-dev \
  libglu1-mesa-dev libegl1-mesa-dev libxkbcommon-dev
```

For a Qt5 build, use this alternative instead:

```bash
sudo apt-get install build-essential pkg-config \
  qtbase5-dev libqt5charts5-dev libssl-dev zlib1g-dev \
  libglu1-mesa-dev libegl1-mesa-dev libxkbcommon-dev
```

Use `qmake6` for Qt6 or the Qt5 `qmake` for Qt5. Keep Qt Base and Qt Charts
on the same major version. Both Qt versions are supported by the qmake projects.

Native `.deb` packaging additionally requires `debhelper` and `dpkg-dev`.
For the alternative CMake build, install `cmake` (3.22 or newer) in addition
to the Qt6 dependencies above; CMake does not support Qt5 in this project.

RDP console support is optional; VNC remains available without FreeRDP.
For Debian 12 / Ubuntu 22.04, the optional FreeRDP 2 development packages are:

```bash
sudo apt-get install freerdp2-dev libwinpr2-dev
```

The qmake build currently detects FreeRDP 2 only. CMake probes FreeRDP 3 first,
then FreeRDP 2; a version-3-only installation does not enable RDP in qmake builds.
Package names vary by distribution release.

### Fedora/RHEL/CentOS

See the [RPM prerequisites](packaging/README.md#fedorarhelcentos-rpm) for Qt5
and Qt6 package options and the additional packaging tools.

See the [packaging prerequisites](packaging/README.md#prerequisites) for AppImage
and other platform requirements.
