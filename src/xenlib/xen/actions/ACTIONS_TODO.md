# AsyncOperation Porting Roadmap

The original C# client relies on a comprehensive action framework (`ActionBase → AsyncAction → MultipleAction/ParallelAction`) that handles RBAC pre-checks, session duplication, grouped execution, audit logging, and UI progress reporting. The Qt rewrite currently exposes a slimmer `AsyncOperation` base and only one concrete action (`ChangeVMISOAction`). This document captures the work needed to realign the C++ implementation with those proven patterns.

---

## Phase 0 – Baseline Audit
- [x] Catalogue which parts of the Qt UI already invoke `AsyncOperation` and which still execute XenAPI calls directly. (see “Current usage” below)
- [x] For each C# action, note required helpers (cache lookups, wizards, command hooks) and whether we still need it. (initial VM lifecycle mapping below; broader coverage pending)
- [x] Decide which enterprise-only features (WLB, DR orchestration, Health Check, Conversion Manager) are out of scope so we can defer them explicitly. (see “Enterprise feature scope” below)

### Phase 0 findings

#### Current usage of `AsyncOperation`
| Area | Behaviour today | Notes |
|------|-----------------|-------|
| `StorageTabPage::onChangeIsoClicked` | Uses `ChangeVMISOAction` and `OperationProgressDialog` | Only production use of `AsyncOperation` in the Qt UI |
| `OperationProgressDialog` | Encapsulates progress UI for `AsyncOperation` | Not yet instantiated anywhere else |

#### Direct XenAPI invocations to migrate onto actions
44 call-sites still invoke `xenLib()->getAPI()` directly. Key hotspots:
- VM lifecycle commands (`commands/vm/*`) and matching toolbar handlers in `mainwindow.cpp`
- Host maintenance commands (`commands/host/*`, `mainwindow.cpp`)
- Pool membership commands (`commands/pool/*`)
- Storage commands (`commands/storage/*`, `mainwindow.cpp`)
- Networking dialogs (`networkingpropertiesdialog.cpp`, `nicstabpage.cpp`)
- Console tab session bootstrap (`consoletabpage.cpp`)

These are the primary candidates to swap over to ported actions once available.

#### VM lifecycle action dependencies (C# reference)
| C# action(s) | Qt entry point(s) today | Key helpers/behaviour in C# | Still required? |
|--------------|------------------------|-----------------------------|-----------------|
| `VMStartAction`, `VMStartOnAction`, `VMResumeAction` | `StartVMCommand`, `ResumeVMCommand`, `MainWindow::start/resume`, `VMOperationCommand` | HA agility checks, `HAUnprotectVMAction`, warning/diagnosis callbacks, async tasks with progress | Yes – need HA dialogs and task polling hooks |
| `VMCleanShutdown`, `VMHardShutdown`, `VMSuspendAction` | `StopVMCommand`, `SuspendVMCommand`, toolbar equivalents | Simple task kick-off + progress strings | Yes – can map directly once base action exists |
| `VMCleanReboot`, `VMHardReboot` | `RestartVMCommand`, `MainWindow::restartVM` | Async reboot tasks, reuse shutdown strings | Yes |
| `VMPause` | `PauseVMCommand`, `Nic/toolbar pause handlers` | Async pause task, minimal extras | Yes |
| `VMUnPause` | `UnpauseVMCommand`, `MainWindow::resumeVM` (currently calls `resumeVM`) | Async unpause | Yes |
| `VMDestroyAction` | `DeleteVMCommand`, `MainWindow::deleteVM` | Orchestrates VDI cleanup, progress, error aggregation | Yes – required before deleting from UI |
| `VMCloneAction` | `CloneVMCommand` | Handles template selection, storage targeting, async clone task, progress text | Yes – depends on cache/template lookup |
| `VMToTemplateAction` | `ConvertVMToTemplateCommand` | Updates `other_config`, handles tasks, emits completion message | Yes |
| `CreateVMAction`, `CreateVMFastAction` | `NewVMWizard`, `NewVMCommand` | Template reading, provisioning, network/disk creation, optional fast clone path | Yes – wizard needs async backend |
| Snapshot actions (`VMSnapshotCreate/Delete/Revert`) | `TakeSnapshotCommand`, `DeleteSnapshotCommand`, `RevertToSnapshotCommand` | Use async snapshot APIs, update metadata, error messaging | Yes – map once async infrastructure ready |

#### Host action dependencies
| C# action(s) | Qt entry point(s) today | Key helpers/behaviour in C# | Still required? |
|--------------|------------------------|-----------------------------|-----------------|
| `HostDisableAction`, `HostEnableAction` | `HostMaintenanceModeCommand`, `MainWindow::enter/exitMaintenanceMode` | Confirms evacuations, updates HA metadata, progress text | Yes – needs evacuation prompts and progress |
| `HostRebootAction`, `HostShutdownAction` | `RebootHostCommand`, `ShutdownHostCommand`, toolbar handlers | Async `host.async_reboot/shutdown`, cancels tasks, updates status bar | Yes |
| `HostEvacuateAction` | (not yet wired) | Ensures VMs migrate, checks SR availability | Required for full maintenance workflow |
| `HostDestroyAction` | (future UI) | Confirms removal, handles pool roles | Needed if we add host destroy |
| Certificate/licensing actions (`HostApplyEditionAction`, etc.) | Wizards/dialogs | Write to host `other_config`, call licensing APIs | Optional depending on scope |

#### Storage actions (SR + VDI + VBD)
| C# action(s) | Qt entry point(s) today | Helpers | Still required? |
|--------------|------------------------|---------|-----------------|
| `SrScanAction`, `SrCreateAction`, `SrIntroduceAction`, `SrReattachAction`, `SrRepairAction`, `SrForgetAction`, `SrDestroyAction` | `NewSRWizard`, `SR` command set (`newsrcommand`, `repairsrcommand`, etc.), `MainWindow` toolbar | Wizard populates device config, uses `SrAction` base to wrap async calls, updates cache on completion | Yes – each wizard button must map to these actions |
| `SrTrimAction`, `SrSetAsDefaultAction` | `setdefaultsrcommand`, `storagetab` context menu | Short async tasks setting pool fields | Yes |
| VDI: `CreateDiskAction`, `DestroyDiskAction`, `MoveVirtualDiskAction`, `MigrateVirtualDiskAction` | Storage tab dialogs (`newvirtualdiskdialog`, `vdipropertiesdialog`) | Need progress & error reporting, updates to cache | Yes |
| VBD: `VbdCreateAndPlugAction`, `VbdEditAction`, `VBDPlug/Unplug` | Attach/detach disk dialog, storage tab actions | Manage plug/unplug tasks, update UI | Yes |

#### Network & VIF actions
| C# action(s) | Qt entry point(s) today | Helpers | Still required? |
|--------------|------------------------|---------|-----------------|
| `ChangeNetworkingAction`, `CreateBondAction`, `DestroyBondAction`, `CreateVlanAction` | `networkingpropertiesdialog`, `nicstabpage`, `NewNetworkWizard` | Validate PIF state, rebuild bonds, restart management interface | Yes – required for editing host NICs |
| `CreateSriovAction`, `DestroySriovAction` | (future advanced UI) | Configure SR-IOV PF/VFs | Optional depending on release |
| VIF actions (`CreateVIFAction`, `PlugVIFAction`, etc.) | Network tab, attach NIC dialog once ported | Manage VIF lifecycle (plug/unplug) | Yes |

#### Pool & Cluster actions
| C# action(s) | Qt entry point(s) today | Helpers | Still required? |
|--------------|------------------------|---------|-----------------|
| `AddHostToPoolAction`, `JoinPoolAction`, `EjectHostFromPoolAction` | `JoinPoolCommand`, `EjectHostFromPoolCommand` | Sets up credentials, updates master/slave roles | Yes |
| `EnableHAAction`, `DisableHAAction`, `SetHaPrioritiesAction` | Future HA settings UI | Coordinates HA heartbeat SR and priorities | Needed for HA feature parity |
| `PoolSyncDatabaseAction`, `PoolDesignateNewMasterAction`, `PoolEmergency...` | Advanced pool maintenance dialogs | Rare operations; mark as later-stage |
| License, WLB, clustering actions | Wizards not yet in Qt | Enterprise scope – defer if out of release |

#### Miscellaneous / Enterprise
- **Disaster recovery (`dr/*`)** – Power-on/off sequences, metadata handling; depends on DR wizards (currently out-of-scope).
- **Workload Balancing (`wlb/*`)**, **Health Check**, **NRPE** – require dedicated UI and backend integration; mark as deferred.
- **PVS (`pvs/*`)**, **GPU (`gpu/*`)**, **Docker/VMAppliance** – specialized features; decide case-by-case.

*Next audit pass should extend this table to storage, networking, and pool actions once the VM set is underway.*

#### Enterprise feature scope
- Workload Balancing (WLB) – **Deferred**: relies on external WLB appliance and extensive UI not in current Qt rewrite.
- Disaster Recovery orchestration (DR actions, metadata VDIs) – **Deferred** for first release; requires full DR wizard parity.
- Health Check service – **Deferred** pending backend integration.
- Conversion Manager (XCM) – **Deferred**; still XML-RPC in C# and requires dedicated appliance.
- Other enterprise-only folders (`pvs`, `wlb`, `healthcheck`, `nrpe`, `xcm`, `statusreport`, `dockercontainer`) should remain out of immediate scope unless requirements change.

---

## Phase 1 – Framework Parity
- [x] Extend `AsyncOperation` with object context (`Pool`, `Host`, `VM`, `SR`, `AppliesTo`) and honour `SuppressHistory` just like `ActionBase`.
- [x] Emit lifecycle events (e.g. via an `OperationManager`) so history panes and notifications can subscribe (`ActionBase.NewAction` equivalent).
- [ ] Implement RBAC/session helpers: pre-flight `apiMethodsToRoleCheck`, sudo elevation, and `DuplicateSession`/`ElevatedSession` parity. *(Session duplication now handled in `AsyncOperation::createSession`; RBAC checks & sudo prompts still outstanding.)*
- [x] Add task rehydration support (`PrepareForEventReloadAfterRestart`) so unfinished operations surface after reconnect.
- [x] Introduce composite operations:
  - Sequential execution (`SequentialAsyncOperation`, analogue to `MultipleAction`). ✓ Already exists as `MultipleOperation`
  - Parallel execution (`ParallelAsyncOperation`). ✓ Already exists as `ParallelOperation`
  - Lightweight wrapper for lambdas (`DelegatedAsyncOperation`). ✓ Implemented
- [x] Build an `AsyncOperationLauncher` that batches per-connection sequentially and across connections in parallel (C# `MultipleActionLauncher`). ✓ `OperationLauncher` verified to match C# 1:1
- [ ] Replace placeholder logging with structured audit logging integrated with the Qt logging categories.
- [x] Provide a reusable progress dialog/dock component that binds to `AsyncOperation` signals (C# `ActionProgressDialog`). *(Note: `OperationProgressDialog` exists; needs verification)*
- [x] Ensure cancellation propagates to child operations and XenAPI tasks (`CancelRelatedTask`, task destruction). ✓ Implemented `cancelRelatedTask()`, `MultipleOperation::onCancel()` propagates to children
- [x] Finalise XenAPI task polling helpers (`pollToCompletion`, `Task.get_record`, `Task.destroy`) with proper error propagation. ✓ Enhanced with suppressFailures, HANDLE_INVALID handling, Task.destroy call
- [x] Port action history/audit pipeline: added `OperationManager`, routed `AsyncOperation` registrations, and surfaced updates via the new `HistoryPage` dock. (Remaining parity items: filters & audit logging strings.)

---

## Phase 2 – Command & UI Integration

### Port UI commands to AsyncOperation

#### VM lifecycle & template commands
- [x] `src/xenadmin-ui/commands/vm/clonevmcommand.cpp` – Replace the blocking `xenLib()->getAPI()->cloneVM` path with `VMCloneAction`, run it via the upcoming `runAction` helper, and let `OperationManager`/History drive refreshes instead of hand-made `QTimer` polling.
- [x] `src/xenadmin-ui/commands/vm/convertvmtotemplatecommand.cpp` – Drive `VMToTemplateAction` (which already sets `other_config["instant"]`) rather than chaining `setVMOtherConfigKey` + `convertVMToTemplate` inline so the conversion shows progress/errors.
- [x] `src/xenadmin-ui/commands/vm/deletevmcommand.cpp` – Queue `VMDestroyAction` (VDI cleanup, task polling, history entry) instead of calling `deleteVM` synchronously; `DeleteTemplateCommand` inherits the same behavior once this is fixed.
- [x] `src/xenadmin-ui/commands/vm/migratevmcommand.cpp` – After the host picker runs, wrap the move in `VMMigrateAction`/`VMCrossPoolMigrateAction`, drop the direct `poolMigrateVM` call, and surface HA pre-check failures through OperationManager.
- [x] `src/xenadmin-ui/commands/vm/changecdisocommand.cpp` – Use `ChangeVMISOAction` for insert/eject (the action already exists and handles VBD lookup), then feed status through `OperationProgressDialog` instead of hand-calling `api->insert/ejectVBD`.
- [x] `src/xenadmin-ui/commands/vm/takesnapshotcommand.cpp` – Instantiate `VMSnapshotCreateAction` for disk/quiesce/memory options so snapshot creation runs asynchronously and errors bubble up consistently.
- [x] `src/xenadmin-ui/commands/vm/deletesnapshotcommand.cpp` – Swap out `xenLib->deleteSnapshot` for `VMSnapshotDeleteAction` to track progress/history and trigger cache refreshes automatically.
- [x] `src/xenadmin-ui/commands/vm/reverttosnapshotcommand.cpp` – Wrap `VMSnapshotRevertAction` (or a delegated async op) around the revert workflow so the VM power cycle is tracked and cancellable.
- [ ] `src/xenadmin-ui/commands/vm/exportvmcommand.cpp` – Once `ExportVmAction` lands, drive it from the wizard selections and replace the placeholder QMessageBox with a call to `showOperationProgress`. *(Deferred: ExportVmAction not yet implemented - requires HTTP download infrastructure)*
- [ ] `src/xenadmin-ui/commands/template/exporttemplatecommand.cpp` – Share the same `ExportVmAction` wiring for template exports so they also appear in the operation history instead of silently finishing. *(Deferred: depends on ExportVmAction)*
- [ ] `src/xenadmin-ui/commands/vm/importvmcommand.cpp` – Feed wizard input into `ImportVmAction`, including HTTP upload progress, rather than short-circuiting with a warning dialog. *(Deferred: ImportVmAction not yet implemented - requires HTTP upload infrastructure)*
- [ ] `src/xenadmin-ui/commands/vm/newvmcommand.cpp` – Stop calling `cloneVM`, `setVMVCPUs`, `setVMMemory`, etc. inside the command; have it launch `CreateVMAction`/`CreateVMFastAction` and rely on OperationManager for status updates. *(Deferred: CreateVMAction not yet ported - 775 lines, requires wizard integration)*
- [ ] `src/xenadmin-ui/commands/vm/newvmfromtemplatecommand.cpp` – Treat this as a thin wrapper that passes the template ref into `CreateVMAction` (instead of re-running the wizard synchronously) so "New VM from template" also queues an action. *(Deferred: depends on CreateVMAction)*

#### Host & pool commands
- [ ] `src/xenadmin-ui/commands/host/hostmaintenancemodecommand.cpp` – Replace the raw `disableHost`/`enableHost` calls with `DisableHostAction` + `EvacuateHostAction` when entering maintenance and `EnableHostAction` when exiting so VM migration progress is tracked.
- [ ] `src/xenadmin-ui/commands/host/reboothostcommand.cpp` – Use `RebootHostAction` (already ported) instead of invoking `rebootHost` synchronously and blocking the UI.
- [ ] `src/xenadmin-ui/commands/host/shutdownhostcommand.cpp` – Queue `ShutdownHostAction` so host power-off shows up in the history dock and cancellation behaves like XenCenter.
- [ ] `src/xenadmin-ui/commands/pool/ejecthostfrompoolcommand.cpp` – Replace `xenLib->getAPI()->ejectFromPool` with `EjectHostFromPoolAction`, which orchestrates VM shutdowns and automatically refreshes the cache.
- [ ] `src/xenadmin-ui/commands/pool/joinpoolcommand.cpp` – Feed the entered credentials into `AddHostToPoolAction` (or a dedicated JoinPoolAction) so the reboot + pool membership change is queued as an AsyncOperation.

#### Storage & network commands
- [ ] `src/xenadmin-ui/commands/storage/detachsrcommand.cpp` – Introduce `SrDetachAction`/a delegated async op that calls `sr.async_detach` and reports completion instead of blocking on `detachSR`.
- [ ] `src/xenadmin-ui/commands/storage/repairsrcommand.cpp` – Swap to `SrRepairAction` (already implemented) for repair requests so the progress dialog is consistent.
- [ ] `src/xenadmin-ui/commands/storage/setdefaultsrcommand.cpp` – Add a delegated AsyncOperation that invokes `Pool.set_default_SR`, registers with OperationManager, and surfaces errors rather than silently calling the API.
- [ ] `src/xenadmin-ui/commands/storage/newsrcommand.cpp` – When the wizard completes, create `SrCreateAction` (or the relevant introduce/attach action) and display progress instead of the current informational toast.
- [ ] `src/xenadmin-ui/commands/network/newnetworkcommand.cpp` – Capture wizard selections and queue `CreateNetworkAction`/`ChangeNetworkingAction` via `runAction`, replacing the current placeholder warning dialog.

### Action helpers & operation feed
- [ ] `src/xenadmin-ui/commands/command.{h,cpp}` – Add `runAction`/`runMultipleActions` helpers that duplicate C#’s `RunAsyncAction`/`RunMultipleActions`: auto-register operations, hook completion, and optionally launch `OperationProgressDialog` so every command stops duplicating boilerplate.
- [ ] `src/xenadmin-ui/tabpages/basetabpage.{h,cpp}` – Expose the same helper (plus accessors for `MainWindow`/`XenLib`) so tab pages and dialogs can queue AsyncOperations without manually touching `OperationManager`.
- [ ] `src/xenadmin-ui/mainwindow.{h,cpp}` – Provide `showOperationProgress`/status-bar plumbing that the helpers can call, and teach the status area to juggle multiple concurrent actions (match C# `statusBarAction` behavior with a queue).
- [ ] `src/xenadmin-ui/dialogs/operationprogressdialog.{h,cpp}` – Extend the dialog so it can be driven by `runAction`/`runMultipleActions` (modal or modeless, grouped operations) and display aggregated descriptions just like XenCenter’s progress UI.
- [ ] `src/xenadmin-ui/operations/operationmanager.{h,cpp}` – Add support for grouping (MultipleAction/ParallelAction equivalents) and ensure helper-launched actions automatically register/deregister, including rehydrated tasks surfaced by `MeddlingActionManager`.
- [ ] `src/xenadmin-ui/widgets/historypage.{h,cpp}` – Make the events dock consume the richer Operation feed (connection/object columns, double-click to open details) so all helper-launched actions appear just as they do in XenCenter’s History pane.

### Wizards & dialogs
- [ ] `src/xenadmin-ui/dialogs/newvmwizard.cpp` – Replace the inline `cloneVM`, `setVMVCPUs`, `setVMMemory`, VBD/VIF destruction, etc. with a sequence of AsyncOperations (`CreateVMAction`, `VbdCreateAndPlugAction`, `CreateVIFAction`, optional `VMStartAction`) triggered through the helper so the wizard simply gathers parameters.
- [ ] `src/xenadmin-ui/dialogs/newnetworkwizard.cpp` – Populate real network/PIF data from the cache and fire `CreateNetworkAction`/`CreateBondAction` (based on the selected type) rather than returning a stub.
- [ ] `src/xenadmin-ui/dialogs/newsrwizard.cpp` – Drive `SrCreateAction`/`SrIntroduceAction` (depending on SR type) when the wizard finishes and remove the demo-only QMessageBox.
- [ ] `src/xenadmin-ui/dialogs/importwizard.cpp` – Feed appliance/network/storage selections into `ImportVmAction`, upload via HTTP, and show `OperationProgressDialog` instead of exiting with “not available”.
- [ ] `src/xenadmin-ui/dialogs/exportwizard.cpp` – Use `ExportVmAction` to export the checked VMs/templates (with manifest/sign/encrypt options) and present progress rather than displaying a placeholder summary.
- [ ] `src/xenadmin-ui/dialogs/hostpropertiesdialog.cpp` – Batch property changes (name, description, tags, other_config) into a delegated AsyncOperation so saving host properties runs asynchronously and updates the operation history.
- [ ] `src/xenadmin-ui/dialogs/networkpropertiesdialog.cpp` – Replace synchronous `setNetworkName/Description/Tags` calls with a delegated action so edits are queued, cancellable, and error messages flow through the helper.
- [ ] `src/xenadmin-ui/dialogs/networkingpropertiesdialog.cpp` – Swap the direct `reconfigurePIF*` calls for `ChangeNetworkingAction` so NIC IP changes move to the action framework and reuse NetworkingActionHelpers.
- [ ] `src/xenadmin-ui/dialogs/poolpropertiesdialog.cpp` – Use a delegated AsyncOperation to update pool name/description/tags/migration compression, matching the C# PoolProperties dialog behavior.
- [ ] `src/xenadmin-ui/dialogs/storagepropertiesdialog.cpp` – Wrap SR name/description/tag edits in an AsyncOperation so long-running XenAPI writes don’t block the UI.
- [ ] `src/xenadmin-ui/dialogs/vmpropertiesdialog.cpp` – Replace `m_xenLib->updateVM` with a delegated op (or future `UpdateVMAction`) that queues the metadata changes and refreshes cache entries when complete.

### Tab page refactors
- [ ] `src/xenadmin-ui/tabpages/storagetabpage.cpp` – Drop the direct `api->setVDINameLabel/Description/resizeVDI` calls inside `onEditButtonClicked` and instead use the planned `VbdEditAction` (or a delegated AsyncOperation) so VDI edits appear in the operation feed.
- [ ] `src/xenadmin-ui/tabpages/nicstabpage.cpp` – Replace `m_xenLib->getAPI()->createBond/destroyBond` with `CreateBondAction`/`DestroyBondAction`, wire them through `runAction`, and disable the UI while the AsyncOperation runs.
- [ ] `src/xenadmin-ui/tabpages/networktabpage.cpp` – Use actions for network creation/deletion (`CreateNetworkAction`, `ChangeNetworkingAction`, destruction helper) instead of calling `createNetwork`, `setNetworkMTU`, and `destroyNetwork` directly; show progress and refresh through OperationManager.

---

## Phase 3 – Core Action Coverage

### ✅ Already ported
- [x] `vm/changevmisoaction.{h,cpp}` – ChangeVMISOAction

### VM lifecycle (highest priority)
- [x] `vm/vmstartaction.{h,cpp}` – VMStartAction
- [x] `vm/vmshutdownaction.{h,cpp}` – VMShutdownAction
- [x] `vm/vmrebootaction.{h,cpp}` – VMRebootAction
- [x] `vm/vmpauseaction.{h,cpp}` – VMPauseAction (includes VMPause and VMUnpause)
- [x] `vm/vmdestroyaction.{h,cpp}` – VMDestroyAction
- [x] `vm/vmcloneaction.{h,cpp}` – VMCloneAction
- [x] `vm/vmtotemplateaction.{h,cpp}` – VMToTemplateAction
- [ ] `vm/createvmaction.{h,cpp}` – CreateVMAction (Deferred - 775 lines, needs wizard integration)
- [x] `vm/createvmfastaction.{h,cpp}` – CreateVMFastAction

### Host management essentials
- [x] `host/reboothostaction.{h,cpp}` – RebootHostAction
- [x] `host/shutdownhostaction.{h,cpp}` – ShutdownHostAction
- [x] `host/enablehostaction.{h,cpp}` – EnableHostAction
- [x] `host/disablehostaction.{h,cpp}` – DisableHostAction
- [x] `host/evacuatehostaction.{h,cpp}` – EvacuateHostAction
- [x] `host/destroyhostaction.{h,cpp}` – DestroyHostAction

---

## Phase 4 – Storage & Networking

### Storage repositories
- [x] `sr/srcreateaction.{h,cpp}` – SrCreateAction
- [x] `sr/srintroduceaction.{h,cpp}` – SrIntroduceAction
- [x] `sr/srreattachaction.{h,cpp}` – SrReattachAction
- [x] `sr/srrepairaction.{h,cpp}` – SrRepairAction
- [x] `sr/srscanaction.{h,cpp}` – SrScanAction
- [x] `sr/srprobeaction.{h,cpp}` – SrProbeAction
- [x] `sr/srtrimaction.{h,cpp}` – SrTrimAction

### Virtual disks & attachments
- [x] `vdi/creatediskaction.{h,cpp}` – CreateDiskAction
- [x] `vdi/detachvirtualdiskaction.{h,cpp}` – DetachVirtualDiskAction
- [x] `vdi/destroydiskaction.{h,cpp}` – DestroyDiskAction
- [x] `vdi/movevirtualdiskaction.{h,cpp}` – MoveVirtualDiskAction
- [x] `vdi/migratevirtualdiskaction.{h,cpp}` – MigrateVirtualDiskAction
- [x] `vbd/vbdcreateandplugaction.{h,cpp}` – VbdCreateAndPlugAction
- [ ] `vbd/vbdeditaction.{h,cpp}` – VbdEditAction

### Networking & VIF
- [x] `network/changenetworkingaction.{h,cpp}` – ChangeNetworkingAction
- [x] `network/createbondaction.{h,cpp}` – CreateBondAction
- [x] `network/destroybondaction.{h,cpp}` – DestroyBondAction
- [x] `network/createsriovaction.{h,cpp}` – CreateSriovAction
- [x] `network/rescanpifsaction.{h,cpp}` – RescanPIFsAction
- [x] `vif/createvifaction.{h,cpp}` – CreateVIFAction
- [x] `vif/deletevifaction.{h,cpp}` – DeleteVIFAction
- [x] `vif/plugvifaction.{h,cpp}` – PlugVIFAction
- [x] `vif/unplugvifaction.{h,cpp}` – UnplugVIFAction
- [x] `vif/updatevifaction.{h,cpp}` – UpdateVIFAction

---

## Phase 5 – Advanced VM Features
- [x] `vm/vmsnapshotcreateaction.{h,cpp}` – VMSnapshotCreateAction
- [x] `vm/vmsnapshotdeleteaction.{h,cpp}` – VMSnapshotDeleteAction
- [x] `vm/vmsnapshotrevertaction.{h,cpp}` – VMSnapshotRevertAction
- [x] `vm/changememorysettingsaction.{h,cpp}` – ChangeMemorySettingsAction
- [x] `vm/changevcpusettingsaction.{h,cpp}` – ChangeVCPUSettingsAction
- [x] `vm/gpuassignaction.{h,cpp}` – GpuAssignAction
- [x] `vm/vmcrosspoolmigrateaction.{h,cpp}` – VMCrossPoolMigrateAction
- [x] `vm/vmmigrateaction.{h,cpp}` – VMMigrateAction
- [x] `vm/vmcopyaction.{h,cpp}` – VMCopyAction
- [ ] `vm/exportvmaction.{h,cpp}` – ExportVmAction *(Deferred: requires HTTP download, file streaming, TAR verification)*
- [ ] `vm/importvmaction.{h,cpp}` – ImportVmAction *(Deferred: requires HTTP upload, wizard integration, VIF migration)*

---

## Phase 6 – Pool, Updates, and Enterprise Features
- [x] `pool/addhosttopoolaction.{h,cpp}` – AddHostToPoolAction *(Simplified: deferred licensing/AD sync/SR cleanup)*
- [x] `pool/createpoolaction.{h,cpp}` – CreatePoolAction *(Simplified: deferred licensing/AD sync/SR cleanup)*
- [x] `pool/ejecthostfrompoolaction.{h,cpp}` – EjectHostFromPoolAction
- [x] `pool/destroypoolaction.{h,cpp}` – DestroyPoolAction
- [x] `pool/enablehaaction.{h,cpp}` – EnableHAAction *(Simplified: basic VM startup options support)*
- [x] `pool/disablehaaction.{h,cpp}` – DisableHAAction
- [x] `pool/emergencytransitiontomasteraction.{h,cpp}` – EmergencyTransitionToMasterAction
- [x] `pool/designatenewmasteraction.{h,cpp}` – DesignateNewMasterAction
- [x] `pool/syncdatabaseaction.{h,cpp}` – SyncDatabaseAction
- [x] `pool/setpoolnameanddescriptionaction.{h,cpp}` – SetPoolNameAndDescriptionAction
- [ ] `updates/*` – Upload/Apply/Clean-up update pipeline
- [ ] Enterprise-specific folders (`vmappliances/*`, `dr/*`, `wlb/*`, `ad/*`, `healthcheck/*`, `nrpe/*`, `pvs/*`, `xcm/*`) – mark as in-scope or explicitly deferred.

---

## Open Questions
- [ ] Do we need a Qt analogue of XenCenter’s Actions pane before wiring the history feed?
- [ ] Which “enterprise” features are required for the Qt rewrite’s first release?
- [ ] Should we automate C# → Qt skeleton generation to minimise transcription errors?
- [ ] How will we test operations (mock XAPI server, CLI harness, integration tests)?

Keep this checklist updated as framework gaps close and more actions are ported to avoid drifting from the behaviour users expect.
