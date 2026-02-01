# XenAdminQt C++/Qt6 Rewrite

**IMPORTANT** This is an alpha version right now - many things work, but many things do not. Use at your own risk.

This is the C++/Qt6 rewrite of the original C# XenAdmin codebase, converted using AI assistance, but hand-fixed, manually reviewed and post-processed with many manual fixes. The fixes are still being done - the goal is to get this codebase into perfection.

The idea is to first port the entire existing C# codebase preserving all naming and logic (same naming for easier verification of what we already ported) - once we get a 1:1 port that works same as original client, we will move to next phase where we add new features and modify existing functionality.

The goal of this project is to get original thick XenAdmin client to work on non-Microsoft platforms (Mostly macOS and GNU/Linux - but any platform supported by Qt might work).

## What is XenAdmin

XenAdmin is a graphical desktop frontend for xapi - https://github.com/xapi-project which is a Xen API framework that is powering XCP-ng as well as Citrix Hypervisor platform (XenServer).

It connects to xapi over HTTP/S protocol using JSON-RPC API calls, provides access to host and VM consoles via XSVNC as well as RRDP performance metrics provided by xapi.

That means it can be used as a graphical frontend to directly manage standalone XCP-ng / XenServer hosts as well as clusters.

# Building
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

## Linux dependencies:

```
# EL based (rpm):
sudo dnf install qt6-qtbase-devel rpm-build qt6-qtbase-devel freerdp2-devel libwinpr2-devel mesa-libGL-devel mesa-libGLU-devel mesa-libEGL mesa-libEGL-devel libxkbcommon qt6-qtcharts-devel rsync

# Debian based:
sudo apt-get install qt6-base-dev build-essential freerdp2-dev libwinpr2-dev mesa-common-dev libglu1-mesa-dev libegl1 libxkbcommon0
```
