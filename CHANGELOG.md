# Changelog

Notable user-visible changes to XenAdminQt are documented here. The project is
still beta software; entries focus on features, behavior changes, bug fixes,
and supported build or packaging targets. Pure refactoring and routine cleanup
are omitted.

## [Unreleased]

### Added

Manage vApps dialog, New vApp wizard and Assign to vApp menu; vApp properties now include the VMs and their startup sequence
Alerts for servers with a missing or duplicate iSCSI IQN, with an Edit IQN link that opens the server's properties
Pool and standalone server properties have C#'s Clustering page for enabling corosync clustering (needed for GFS2 storage)
Pool and standalone server properties have C#'s NRPE monitoring page (xapi 23.27.0 or later, local root or Pool Admin)
VM properties have C#'s Cloud-Config Parameters page for CoreOS VMs and templates on pools with container support
VM properties have C#'s USB page, with the Attach USB dialog, for passing host USB devices through to HVM VMs
SR properties have C#'s Read Caching page for storage types that support it
Saved searches: save the current search to a pool (XenCenter-compatible, so XenCenter sees it too), delete it, pick saved and built-in searches from the Search tab and navigation pane, and export or import .xensearch files (File > Import Search...).
- Search editor: custom fields can be searched, as in C#. Each custom field
  appears as a condition (text fields with contains/starts with/...; date
  fields with before/after/on and recent periods), and the list follows custom
  fields as they are added or removed.
- Search editor: conditions on related objects work, as in C#. Choosing
  "Pool", "Server", "VM", "Network", "SR", "Virtual Disk", "VM Appliance" or a
  folder adds a condition for that object on the same line, including "Is"
  to pick one object. Switching a condition to "All/Any/None of the
  following" keeps it as the first entry, a new "Select a filter..." row
  appears as rows are used, and conditions that stop applying are removed
  when "Search for" changes.
- Email notifications page in Pool Properties (and Server Properties for a
  standalone server): send performance alert emails to an address through an
  SMTP server, in English, Chinese or Japanese, as in C#.
- vTPM manager (VM > Trusted Platform Module Manager..., the VM right-click
  menu and the General tab's Trusted Platform Module row): attach, remove and
  inspect vTPMs of UEFI VMs, as in C#.
- General tab: VMs show their boot mode (BIOS, UEFI or UEFI Secure Boot, in red
  when the pool lacks UEFI certificates), servers booting over multipath show
  how many boot paths are active, and running VMs list their read-cached disks
  or why read caching is off, as in C#.
- General tab: the Tags and Folder rows have "View tag..." and "View folder..."
  right-click items that show a search for that tag or folder, as in C#.
- The title bar shows "Filters are ON" or "Filters are OFF" on the Alerts and
  Events pages, as in C#.
- Quick Create (Templates menu and template context menu), as in C#: creates a
  VM named "<template> (n)" from an instant template and starts it straight
  away. The menu item used to show "Not Implemented". While the VM is being
  set up it is kept out of the tree, as the New VM wizard now also does.
- Full screen consoles, as in C#: the Fullscreen button or the configured
  shortcut (Ctrl+Alt by default, Ctrl+Alt+F, F12 or Ctrl+Enter) shows the
  console on the whole screen with C#'s connection bar (console name, pin and
  exit buttons; unpinned it slides away until the mouse touches the top edge)
  and a fading "Press ... to exit full screen mode" hint. The default Ctrl+Alt
  shortcut previously did nothing.
- Undocked VM consoles have C#'s lifecycle button in the header bar, with Shut
  Down, Reboot, Suspend, Force Shut Down and Force Reboot (whichever are
  currently possible); host consoles offer Shut Down and Reboot of the server.
- The client now writes an application log file, like C#'s, and Help > View
  Application Log Files opens its folder (the multipath alert's View Log Files
  link does the same). A new Logging page in Options can turn the log off,
  include debug messages, and cap its size (10 MB by default, spread over the
  log and two backups). Messages are written in batches every few seconds to
  keep disk writes low, and console session cookies are masked.
- Added C#'s status bar buttons for tasks in progress, failed tasks and alerts.
  Each shows its count only when it is above zero and opens the Events or
  Alerts page when clicked.
- Added C# fix actions to the alert Actions menu: HA Settings for HA alerts,
  Repair Storage for failed storage plugs, Alarm Settings for performance alarms
  (opening the Alerts page of the affected host, VM, or SR), and Install
  Certificate for expiring host certificates. The menu also gained Copy, and
  Dismiss is only offered when the alert can be dismissed.
- Added the snapshot schedule panel to the Snapshots tab, showing whether the VM
  is part of a snapshot schedule.
- Alerts now show C#'s friendly titles and descriptions (for example "host1:
  Server failed" / "Server 'host1' has failed.") instead of raw message codes,
  and the Location column shows the pool or server name instead of an object
  UUID. Performance alarms have their own titles, such as "CPU Usage Alarm".
- Failed login attempt alerts now summarise the attempts: totals, attempts from
  unknown sources, and the most common sources with IP address, attempt count,
  last attempt time, username, originator and user agent.
- Disk data merge (leaf coalesce) alerts now name the disk and its VM, for
  example "Data merge for VDI 'disk0' of VM 'vm1' has completed."

### Fixed

Connecting to a pool member whose pool is already connected now says so instead of offering to connect to the coordinator, and the offer uses XenCenter's wording
Connection errors no longer report a wrong password when the server gave no reason, no longer repeat an earlier attempt's reason, and declining to connect to the pool coordinator shows no extra error
Tasks on a server, or on its VMs and storage, now show when filtering events by the server's pool
The console's Enable Remote Desktop button now asks the VM to turn Remote Desktop on
After a server joins a pool its separate connection is removed, and the pool's default storage is cleared if it was local storage, as XenCenter does
Search results sort by the chosen column's value, as in XenCenter, instead of by cell text, and the order applies to groups too; searches over several pools no longer list results once per pool
Rebooting servers now warns when a server runs a storage virtual appliance, and a failed reboot is reported in Events instead of a separate message box
The Take VM Snapshot dialog no longer has the quiesce option; servers since XenServer 7.6 don't support quiesced snapshots
The Take VM Snapshot dialog hides quiesced snapshots on servers that no longer support them and explains unavailable snapshot types by the VM's tools state, like C#
The yellow notes in network properties are readable in dark mode
On macOS, dialog fields stretch to the window width and forms are left-aligned, as on Windows and Linux, instead of narrow boxes in the middle
The memory and disk size search conditions use XenCenter's "bigger than", "smaller than" and "is exactly"
Older saved searches with conditions such as "Pool is", "Server is", "Network uses" or "Disks attached to" load into the search editor instead of an empty condition
The folder breadcrumb above search results shortens long paths to fit, uses XenCenter's separator icon, and clicking a parent folder searches it
Folders use XenCenter's folder icon in the tree, search and menus, and the Folders button and New Folder command have an icon again
Ctrl+F puts the cursor in the navigation pane's search box, not just the tree
Consoles are rebuilt when the automatic switch to Remote Desktop is turned on or off in Options
The search panel's "Search for" list shows an icon for each object type
The HA column in search results shows each VM's restart priority, a pool's failure tolerance and the heartbeat SR instead of always "--"
The Migrate to Server, Start on Server and Resume on Server submenus in the main VM menu on macOS no longer stay stuck at "Updating..."
After New Pool, the servers that joined are no longer kept as separate connections in the tree and the server list, as in XenCenter
Alerts can only be dismissed by users whose role allows deleting messages, as in XenCenter, instead of failing on the server
Selecting pools and standalone servers together offers Disconnect, Reconnect and Edit Tags in the context menu, as XenCenter does, and Disconnect then disconnects all of them
Failed snapshot schedule alerts and Workload Balancing placement reasons show readable error messages instead of raw XenAPI error codes
Changing an SR-IOV network to another NIC, VLAN or to an internal network removes its SR-IOV configuration, as XenCenter does, instead of leaving it behind
Switch to Graphical Console works for VMs running their own VNC server, as in XenCenter: the app finds the server, asks for its password when needed (remembered per VM, saved with the session) and goes back to the text console if you cancel; the VNC client now answers password challenges correctly and no longer stalls when the server's handshake arrives in pieces
Consoles check the user's role like XenCenter: a user whose role may not open a console sees which roles may, instead of a console that fails; the host console opens dom0 and the driver domain console its storage driver domain on hosts that have one; Open SSH Console uses the logged-in user's name instead of root
Back and Forward return to tag and folder searches, as XenCenter does, and clicking the selected object in the tree while a search is shown brings its tabs back
Changing a network's MTU, bond mode or load balancing algorithm in its properties is saved again, and the network is replugged so a new MTU takes effect, as XenCenter does; the MTU can be changed while VMs with I/O drivers are running
Opening a console or performance graphs on a pool member of an XCP-ng 8.2 pool no longer asks to trust the member's default certificate
Rebooting or restarting the toolstack of a pool coordinator, or installing a server certificate, now drops and reconnects the connection straight away, as XenCenter does, instead of treating it as live until the event stream failed; operations on a connection lost unexpectedly are cancelled
Install Guest Tools inserts the guest tools ISO (XCP-ng: guest-tools.iso) into the VM's CD drive, repairing an unplugged tools SR first, and shows the install commands for Linux guests; it used to show a "Not Implemented" message
Custom fields work like XenCenter's: they are defined pool-wide (New Custom Field... and Delete on the Custom Fields page update the pool's list, and deleting a field clears its values), the page shows every defined field with a text or date-and-time editor, and the General tab lists only defined fields
New VM from Snapshot opens the New VM wizard with the chosen snapshot selected, instead of the first template
Crash when clicking a group in the navigation tree of a saved search, and possible crashes in the Tags, Folders and vApps views or while filtering them with the search box
- Search conditions read the same values as C#, so conditions on values held
  by related records now match: Operating System, Start Time, Tools Status,
  Memory and HA Restart Priority never matched before. Enum conditions offer
  the C# choices (for example "Fully optimized" for tools status), and date
  conditions offer "is today", "was yesterday", "is this week", "was last
  week", "is before", "is after" and "exactly matches", matched as in C#.
- Search "Group by" works as in C#: grouping by operating system, power state,
  tools status, HA restart priority, read caching, Windows Update capable,
  network, SR, virtual disk, VM, vApp and custom fields now groups by those
  (before, most of these silently grouped by type). Only groupings relevant
  to what is searched for are offered, related-object groupings keep their
  pool/server/VM order, buttons can be dragged to reorder, and a search's
  grouping is shown when it is opened.
- The "Tags" search condition lists the tags in use; its list was always
  empty.
- Searching by a date custom field set in XenCenter now matches; the stored
  date format was not recognised.
- The search editor lists conditions by their names ("Name", "Power State",
  "Pool") instead of internal identifiers, and no longer lists "In Any
  Appliance" twice.
- Search conditions that look at related objects now work, as in C#: "Pool",
  "Server", "VM", "Networks", "Storage" and "Disks" conditions never matched
  (or matched the object itself), and an "All of" group returned nothing while
  one of its rows was still "Select a filter...".
- The client now builds objects for every server class it receives, as C#
  does. Sixteen classes were never turned into objects, so code that looked
  them up got nothing. Among the effects: vApp Start/Shut Down could not find
  their vApp, vApps were missing from views that list them, "Reclaim freed
  space" was never offered for storage that supports it, and clustering and
  pool-join checks saw no clustered servers.
- Pool and Server Properties show the same pages as C#: Security only before
  XenServer 8 (Stockholm), Live Patching only before the Cloud release,
  Networking only with the vSwitch backend, and no Performance Alerts page for
  pools. A standalone server's properties now include these pool-wide pages
  and its GPU page, which the server dialog left out.
- The General tab now shows the Trusted Platform Module row for VMs with a
  vTPM. The client received vTPM records but never turned them into objects,
  so VMs always looked as if they had no vTPM.
- The General tab's read caching status now uses the per-server state the
  storage manager reports, as C# does; it could say "Enabled" when the VM was
  not using read caching.
- VM > Send Ctrl+Alt+Del is now only enabled while the Console tab shows a
  running VM, as in C#.
- The Memory tab now also updates when a VM's guest agent reports changes
  (ballooning support, the Edit button), batches bursts of changes into one
  redraw, skips redraws while hidden and, on a server, hides VMs that are
  hidden in the tree, as in C#.
- If the New VM wizard fails while creating a VM, the half-created VM and its
  disks are now deleted, as in C#, instead of being left behind.
- While a server is being added to or removed from a pool it is hidden from the
  tree, as in C#, and shown again if the operation fails.
- Starting the VM at the end of the New VM wizard now shows the HA warning and,
  if the VM can't boot, the boot diagnosis, like VM > Start. The start no longer
  depends on the wizard's own progress, which could cancel it once the wizard
  finished.
- Deleting a VM together with its snapshots now also deletes the snapshots'
  own disks, as in C#.
- Templates created from a snapshot are now marked as instant templates, as in
  C#, so Quick Create is offered for them.
- VM > Uninstall VM now deletes the VM through the usual delete confirmation
  (choosing which disks and snapshots to remove), as in C#, instead of showing
  a "Not Implemented" message.
- The console tab now follows changes like C#: the CD drive list updates when
  drives are added or removed, host and storage driver domain consoles pick up
  renames, the SSH button follows IP address changes after the VM boots, and
  "Click here to start it" / "resume it" is only offered when a host that can
  run the VM is enabled, updating as hosts are enabled or disabled. Deleting a
  VM whose console is undocked closes the undocked window.
- Undocked consoles: closing the window now puts the console back into the
  Console tab instead of leaving it in a hidden window, minimizing pauses the
  console again, a remembered position that is now off-screen (for example on a
  disconnected monitor) is ignored in favour of a centred window, the "Preserve
  scale when console is undocked" option takes effect, and an open undocked
  console no longer keeps the application running after the main window closes.
- Opening a console on a pool member no longer asks to accept that member's
  security certificate. Certificates the pool itself reports for its hosts are
  trusted automatically, since they come over the already accepted connection.
- VM consoles are faster and closer to C#: screen updates use the compact
  CopyRect, RRE and CoRRE encodings instead of raw pixels only and are drawn row
  by row; the guest's mouse cursor is shown as the local pointer instead of
  being drawn into the picture; and HVM guests get physical key scan codes
  (QEMU extended key events) so non-US layouts and AltGr behave like the guest
  expects. Keys held when the console loses focus are released.
- VM consoles now follow guest resolution changes, e.g. while a VM boots. The
  console only told the server it understood raw pixel updates, so the server
  could not announce a new screen size and the console kept the old one,
  showing only the top part of the screen until the tab was reopened.
- Options > Consoles "Enable clipboard and printer redirection" now controls
  clipboard sharing with VNC consoles, as in C#. The clipboard was always shared
  in both directions, and the option was disabled in builds without FreeRDP.
  The console's clipboard text is also no longer written to the debug log.
- Security: server TLS certificates were never checked. Every connection (API,
  console, import/export, performance data) accepted any certificate, so an
  impersonated server would have received the login password. Certificates are
  now handled as in C#: ones trusted by the system are accepted; any other is
  trusted on first use after a "New Security Certificate" prompt, remembered
  per server, and a later change triggers a "Security Certificate Changed"
  warning. The Options > Security settings for both warnings now take effect.
  Expect one prompt per server the first time you connect after updating.
- OVF/OVA and disk image imports now keep the new VM hidden until it is fully
  set up, as in C#, and destroy the partly imported VM when an import is
  cancelled or fails part-way (OVF/OVA imports could leave it behind).
- The notifications button and its Alerts and Events entries now show unread
  counts again, e.g. "Alerts (3)" and "Events (2 errors)" with the Events error
  icon; the counts were never updated. The title of an open Alerts or Events
  page follows the count.
- The Alerts, Events and Updates entries in the notifications pane and the
  title bar of those pages now show C#'s icons. They used desktop theme icons,
  which are missing on macOS and Windows, and the title bar referenced images
  that did not exist.
- View > Show Hidden Objects now works in the tree and searches. Hidden VMs,
  SRs, disks and networks are left out unless it is enabled (they used to be
  always shown, grayed), and, as in C#, internal and temporary objects, memory
  and tools SRs, missing or unmanaged disks, vTPM templates when vTPM is not
  licensed, and VMs or local SRs whose server is offline are always left out.
- The VM network tab now uses the VM's own limit on network interfaces (from
  its template's restrictions) instead of always allowing 7, and shows the C#
  error when the limit is reached.
- The VM network tab now shows interfaces on the guest installer network when
  hidden objects are shown, as in C#.
- VM reboot and shutdown operations now record the VM's server and pool, so
  their Events history entries show where they ran, and use C#'s titles such
  as "Rebooting VM 'vm1' on 'host1'" and "Suspending VM 'vm1'".
- Reducing vCPUs on a running VM now fails with C#'s message, which includes
  the current vCPU count.
- Fixed dismissed alerts reappearing after reconnecting. Dismissing now deletes
  the messages on the server, in one call on xapi 22.19.0 and newer; alerts
  reappear if the deletion fails. Alerts can no longer be dismissed while their
  connection is disconnected.
- Repair Storage now handles several selected storage repositories at once.
- The Scheduled snapshots view option is now hidden only when the license does
  not allow snapshot schedules.

- Restored fixed-height navigation buttons and their icons, with buttons moving
  progressively into the bottom icon row as the tree splitter is dragged down.

- Fixed Attach Disk showing unmanaged snapshot-chain base disks and other
  ineligible disks. Matched C# sharing, read-only, HVM, and storage-accessibility
  rules, with eligibility refreshed as disk and storage metadata changes.

- Restored OS icons, template category labels, and C# ordering in the New VM
  wizard: custom templates first, newer names first within each category, and
  hidden templates last. Explicit template selection now also accepts UUIDs.

- Fixed snapshots marked as templates showing template icons and appearing as
  templates in Infrastructure view; snapshot classification now takes precedence.

- Fixed built-in and custom templates sharing a cached icon, and matched C#'s
  built-in template detection using the presence of the `default_template` marker.

- Restored tree double-click actions for starting or resuming VMs, creating VMs
  from templates, reconnecting disconnected hosts, and powering on eligible hosts.

- Fixed new and attached VM disks skipping free device positions, and restored
  the reboot notification when a newly created disk cannot be hot-plugged.

- Restored the HA warning when starting or resuming protected VMs that cannot
  run on every host, with confirmation before clearing their HA restart priority.

- Fixed recovery from closed API connections in long-running sessions and
  restored heartbeat monitoring after connection and reconnection.
- Fixed failed host eligibility checks being treated as successful, which could
  incorrectly enable unavailable hosts in Start on Server and related menus.
- Fixed several storage and VM settings operations using the primary connection
  instead of their action's dedicated session.
- Fixed Home Server operations with multiple VMs selected to check and use each
  VM's own home server instead of the first selected VM's home server.
- Fixed potential crashes from host eligibility checks finishing after a VM
  operation menu was closed or reopened.

## [v0.1.0-beta] - 2026-11-14

### Added

- Added an optional snapshot exportability setting that clears `is_a_template`
  after creation for use with `xe vm-export`. Template creation from these
  snapshots explicitly marks the clone as a template.
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
