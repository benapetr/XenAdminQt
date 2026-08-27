# Changelog

Notable user-visible changes to XenAdminQt are documented here. The project is
still alpha software; entries focus on features, behavior changes, bug fixes,
and supported build or packaging targets. Pure refactoring and routine cleanup
are omitted.

## [Unreleased]

### Added

- Added VM and appliance import/export wizards, including XVA, OVF/OVA, gzip,
  URL-based imports, OVF validation, and the supporting upload/download flows.
- Added host TLS certificate installation and management.
- Added an AppImage packaging script and CMake build files.
- Added ARM64 and RISC-V cross-builds and Debian release packages to CI.
- Added advanced and simplified CPU/memory modes to the New VM wizard, with the
  selected mode remembered between runs.
- Added clickable object references to XenCache Explorer.
- Added adjustable, persistent font sizing to the debug window.

### Changed

- Reworked the New SR wizard, including an NFS version selector, clearer empty
  field descriptions, and a Test Connection action only where XAPI supports it.
- Made dialogs more compact and improved tree expansion-state preservation and
  tree refresh performance.
- Improved context-menu property actions, including template properties.
- Improved the reconnect menu and connection-in-progress presentation.
- Deduplicated IP addresses in the networking copy menu and tree view.
- Join Pool is no longer offered for hosts that are already pooled or restricted
  from pooling.
- Removed the deprecated Qt 5 macOS CI build while retaining Qt 5 compatibility
  fixes for other supported builds.

### Fixed

- Fixed removing a disconnected host, including deletion of its saved profile
  and all corresponding live connection objects.
- Fixed a crash caused by tabs retaining objects from a removed connection.
- Fixed Enter Maintenance Mode enablement and state handling.
- Fixed snapshot-page refresh/selection glitches.
- Fixed reconnecting hosts appearing connected before their initial cache was
  ready.
- Fixed Windows builds and several Qt 5 build regressions.

## [v0.0.6-alpha] - 2026-04-03

### Added

- Added VM template copying.
- Added support for selecting a dedicated migration network through the
  `xo:migrationNetwork` pool setting.
- Added the generated XAPI method reference documentation.

### Changed

- Completed pool-join validation rules.
- Removed the redundant manual refresh action from the live snapshots view.
- Added pool management-interface information and sorted hosts in storage
  details.

### Fixed

- Fixed VM creation failing when more than 16 VDIs were requested.
- Fixed migration over a dedicated network failing with `VDI_NOT_IN_MAP`.
- Fixed udev-backed SRs being incorrectly hidden as tool-created SRs.
- Fixed host CPU usage display and the advanced pool settings layout.
- Fixed intermittent VNC failures caused by discarded buffered tunnel data and
  enabled retry handling after unexpected console disconnects.

## [v0.0.5-alpha] - 2026-02-27

### Added

- Added folders and tags to the organization tree, including drag-and-drop where
  supported.
- Added High Availability management and GPU configuration/status pages.
- Added certificates and pool CPU information to the General tab.
- Added Linux console special-key controls.
- Added RBAC checks to most actions.
- Added CSV export to table context menus and the search panel.
- Added multi-select deletion for snapshots and disks, and multi-disk move
  support including online disk migration.

### Changed

- Made property changes asynchronous so dialogs no longer freeze while applying
  common edits.
- Added MB/GB selection to memory dialogs.
- Expanded the tree with disks, snapshots, and networks; improved grouping,
  icons, context menus, multi-selection persistence, and scroll preservation.
- Enabled sorting throughout tables and improved SR picker ordering and refresh
  behavior.
- Hid snapshot/template-inapplicable tabs and reduced event-list noise by
  default, with a Show All Events option.

### Fixed

- Fixed host and VM uptime parsing and display.
- Fixed New VM affinity-host storage selection and GPU-page crashes.
- Fixed SR allocation and multipath information.
- Fixed VM-to-template UI freezes and unsafe cross-thread UI callbacks.
- Fixed HBA LUN discovery in the New SR wizard.
- Fixed ghost or never-finishing tasks in the event viewer, especially on Qt 5.

## [v0.0.4-alpha] - 2026-02-10

### Added

- Added RPM and Enterprise Linux packaging support.
- Added encrypted credential storage protected by a master password, with
  independent settings for saved servers, saved passwords, and auto-reconnect.
- Added command-line options and a build option to disable cryptography.
- Completed the New Network wizard, including bond creation.
- Added missing server commands for password changes and control-domain memory.
- Added an initial live performance metrics view.
- Expanded the New SR wizard and added proxy, console, display, and confirmation
  settings.

### Changed

- Completed network and VDI property dialogs and improved host, VM, pool, memory,
  NIC, event, and notification views.
- Added multi-selection and sorting to storage/network tables and standardized
  size formatting.
- Made operation tracking cover actions consistently so progress is visible in
  the event view.
- Updated SR filtering in the VM storage picker to account for the VM's home
  host.

### Fixed

- Fixed infrastructure-tree sorting by name rather than opaque reference.
- Fixed event polling, recovery-mode boot, pool creation/join behavior, and host
  power/performance controls.
- Fixed missing VM and server menu entries and restored accidentally removed
  menus.
- Fixed Qt 5 and Windows build regressions.

## [v0.0.3-alpha] - 2026-01-24

### Added

- Added migration-aware VM context menus and a cross-pool migration wizard.
- Added VM move/copy dialogs and the Reclaim Freed Space storage command.
- Added missing host power actions, WLB integration, and host RBAC rules.
- Added basic unit-test infrastructure.

### Changed

- Reworked tree and toolbar multi-selection around a central selection manager.
- Brought VM and host memory views, General tabs, property dialogs, and context
  menus closer to the original XenAdmin behavior.
- Added resident-VM memory details and improved ballooning controls.
- Showed homeless VMs directly under their pool.
- Updated Debian packaging for Debian 12/13 and Ubuntu, including distribution
  names in package artifacts.

### Fixed

- Fixed VNC key auto-repeat and dark-mode property-dialog colors.
- Fixed VM CPU property hints, session/NIC tabs, and several previously unwired
  menus.
- Fixed rare shutdown crashes from actions completing after the main window was
  destroyed, plus related Qt 5 callback crashes.

## [v0.0.2-alpha] - 2026-01-10

### Added

- Added universal macOS DMG and Windows packaging support.
- Added cross-pool VM migration with a dedicated SR picker.
- Added the SR repair flow, Restart Toolstack command, and missing View menu
  options.
- Added a XenCache Explorer for inspecting low-level cached XAPI state.
- Expanded the New VM wizard with diskless VM creation, network/disk context
  menus, and corrected disk/CD provisioning.

### Changed

- Reworked snapshots to match the original client, including multi-selection.
- Rebuilt the navigation tree using the original search/grouping model, with
  alphabetical child sorting.
- Completed network properties and improved network/NIC editing.
- Completed more of the login flow, including server version, user, and role
  information.

### Fixed

- Fixed migration-wizard crashes, VM-interface property activation, and default
  SR icons.
- Fixed SR detach/attach behavior and several incorrect API calls.
- Fixed an infinite loop involving a selected disconnected host and hardened
  asynchronous operation ownership against memory corruption.
- Fixed host shutdown crashes, guest-tools reporting, toolbar refresh, and ISO SR
  discovery.

## [v0.0.1-alpha] - 2026-01-01

### Added

- Published the first working alpha of the C++/Qt XenAdmin port.
- Added parallel server connections and a XenObject-based connection/cache model.
- Added search, events, alerts, notifications, friendly XAPI errors, and status
  reporting for failed actions.
- Added host, VM, storage, memory, console, pool, and application preference
  views, plus the New SR and VM properties dialogs.
- Added Linux and macOS packaging scripts and CI builds for Qt 5 and Qt 6.

### Changed

- Aligned login, tab, tree, command, storage, search, and property-dialog behavior
  with the original C# XenAdmin client.
- Reorganized API bindings into the XenAPI namespace and replaced many raw cache
  maps/references with typed objects.

### Fixed

- Fixed VM start behavior from the console, RRD NaN handling, preference colors,
  property-dialog icons, and macOS packaging.
- Added compatibility for the older FreeRDP/XRDP stack shipped by Debian 12.

[Unreleased]: https://github.com/benapetr/XenAdminQt/compare/v0.0.6-alpha...HEAD
[v0.0.6-alpha]: https://github.com/benapetr/XenAdminQt/compare/v0.0.5-alpha...v0.0.6-alpha
[v0.0.5-alpha]: https://github.com/benapetr/XenAdminQt/compare/v0.0.4-alpha...v0.0.5-alpha
[v0.0.4-alpha]: https://github.com/benapetr/XenAdminQt/compare/v0.0.3-alpha...v0.0.4-alpha
[v0.0.3-alpha]: https://github.com/benapetr/XenAdminQt/compare/v0.0.2-alpha...v0.0.3-alpha
[v0.0.2-alpha]: https://github.com/benapetr/XenAdminQt/compare/v0.0.1-alpha...v0.0.2-alpha
[v0.0.1-alpha]: https://github.com/benapetr/XenAdminQt/releases/tag/v0.0.1-alpha
