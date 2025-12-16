# XenAdminQt C++/Qt6 Rewrite

**IMPORTANT** This is an alpha version right now - many things work, but many things do not. Use at your own risk.

This is the C++/Qt6 rewrite of the original C# XenAdmin codebase, converted using AI assistance, but hand-fixed, manually reviewed and post-processed with many manual fixes. The fixes are still being done - the goal is to get this codebase into perfection.

The idea is to first port the entire existing C# codebase preserving all naming and logic (same naming for easier verification of what we already ported) - once we get a 1:1 port that works same as original client, we will move to next phase where we add new future and modify existing functionality.

The goal of this project is to get original thick XenAdmin client to work on non-Microsoft platforms (Mostly macOS and GNU/Linux - but any platform supported by Qt might work).

## Project Structure

```
src/
├── xenlib/             # Shared library for low-level Xen functionality
│   ├── xen/            # Xen API abstractions (connection, session, API calls)
│   ├── utils/          # Utility classes (encryption, JSON helpers)
│   └── xenlib.pro      # Library build configuration
├── xenadmin-ui/        # Qt-based UI application
│   ├── dialogs/        # UI dialogs (connect, debug console, etc.)
│   ├── widgets/        # Custom widgets (empty currently)
│   ├── commands/       # Command pattern implementation for operations
│   └── xenadmin-ui.pro # UI application build configuration
└── xenadminqt.pro      # Top-level project configuration
```

