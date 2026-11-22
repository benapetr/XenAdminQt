# Status of code migration from original C#

This is a beta port. The implementation notes below reflect the current
source and [changelog](../CHANGELOG.md). They are not a fresh live-server test
report. Historical testing and issue lists are retained below for follow-up;
revalidate them before treating them as current release guarantees or defects.

## Implemented; parity and integration validation still needed

- Infrastructure/object trees include disks, snapshots, and networks.
- Folder and tag organization views, including drag-and-drop, are present.
- High Availability management and an HA tab are present.
- Performance tab and metric-update infrastructure are present.
- VM/appliance import and export wizards include XVA, OVF/OVA, gzip, and URL
  import workflows.
- Host networking tabs, network wizards/properties, and the New SR wizard are
  present. Storage backends still need integration coverage.
- VM-to-template conversion and creation from templates have commands and
  workflows.

Implementation presence does not establish complete C# parity. Exercise these
features against supported server versions and retain outstanding work as
specific reproducible gaps.

## Intentional differences from C#

Things the C# XenAdmin has that this port leaves out on purpose. When comparing the
two codebases, don't report these as missing or port them back.

- **Quiesced (VSS) snapshots.** C#'s Take Snapshot dialog has a "Quiesce" checkbox
  (`SnapshotType.QUIESCED_DISK`, `VM.snapshot_with_quiesce`) that it only shows for
  servers older than XenServer 7.6 (Quebec). xapi removed the call in Quebec: it is
  marked removed in `xen-api/ocaml/idl/datamodel_vm.ml` and always fails with
  `VM_SNAPSHOT_WITH_QUIESCE_NOT_SUPPORTED`. The port offers disk-only and disk and
  memory snapshots only. The `VM_SNAPSHOT_WITH_QUIESCE_*` error texts are kept,
  because current servers can still return them.
- **Quiesced snapshot schedules.** For the same reason, C#'s New Snapshot Schedule
  wizard and schedule properties have a "Quiesce the VM before taking snapshot" checkbox
  (`vmss_type.snapshot_with_quiesce`), shown only before XenServer 7.6. xapi still has
  the type but needs the VSS feature and VMs that support quiesced snapshots, so it can't
  work on current servers. The port offers disk-only and disk and memory schedules. A
  schedule that already has the quiesced type keeps it, with a note, until another type
  is chosen (`PolicySnapshotTypePage`), and the Assign to Snapshot Schedule menu leaves
  such schedules out, as C# does when the VMs can't take quiesced snapshots.

## Previously reported unfinished work

- Search tab options panel: review remaining parity gaps.
- Host network wizards/properties and New SR workflows: finish any remaining
  gaps identified during integration testing.

## Previously reported polish items
* Console - it works most of the time, but there are random scaling issues during boot, RDP behavior needs validation (an RDP client implementation is present)
* UI - menus and toolbar buttons sometime don't refresh on events (unpause -> still shows force shutdown)
* Menu items - some of them may still be missing, but they are mostly fine now
* Events not showing progress bar for each item

## Integration testing backlog
* VM disk resize
* VM disk move
* VM cross pool migration
* Properties of Hosts, VMs and Pools
* Options
* Maintenance mode
* New SR wizard - HBA, iSCSI, NVMEoF, CIFS, NFS

## Historically reported working and tested
* Add server
* Connection to pool - redirect from slave to master
* Connection persistence
* Basic VM controls (start / reboot / shutdown)
* New VM wizard
* Clone VM
* VM deleting
* VM live migration
* Pause / Unpause
* Suspend / Resume
* Snapshots
* VM disk management (create / delete / attach / detach)
* SR attach / detach / repair stack
* CD management
* Grouping and search filtering (clicking hosts / VMs in tree view top level)
* Tree view (infrastructure / objects)
* Events history
* Network tab (VM)
* General tab
* Host command -> Restart toolstack
* Memory tabs
* New pool wizard
* NIC tab - bonding

## Previously reported issues to reproduce
* When quitting the app sometimes the "pending ops" window appears that's empty
* SR in list of VDI we see "base copy" which is hidden in original version
