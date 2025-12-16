# Action Framework Overview

This project mirrors the original C# XenCenter action infrastructure (based on `ActionBase`/`AsyncAction`) while rewriting it in C++/Qt. This note spells out the moving parts, how they map across languages, and when to reach for the framework.

## Key Concepts

- **Async operations are for user workflows.** Anything the user triggers that mutates state or might take long (clone VM, change ISO, pool maintenance) should derive from `AsyncOperation` (C++), just like `AsyncAction` (C#).
- **Sessions are duplicated per operation.** Every action spins up its own `XenSession`/`XenConnection` pair so network I/O happens on a dedicated worker thread—exactly the pattern used by XenCenter.
- **History & notifications live alongside actions.** The C# client pushes each action into the Events pane. Our port replicates this via `OperationManager` and the Events dock.
- **Non-mutating data fetches (login, tree population, cache refresh) do *not* use actions.** Those are handled directly by `XenLib` utilities just like XenCenter’s `ConnectionsManager` and cache loaders.

## File Map

### C++ (Qt port)

| Responsibility | Location |
|----------------|----------|
| Base action class (progress, cancellation, session duplication) | `src/xenlib/xen/asyncoperation.h/.cpp` |
| Concrete actions (ported from C# hierarchy) | `src/xenlib/xen/actions/…` (e.g., `vm/changevmisoaction.{h,cpp}`) |
| Global operation tracking & history feed | `src/xenadmin-ui/operations/operationmanager.{h,cpp}` |
| UI history/events pane | `src/xenadmin-ui/widgets/historypage.{h,cpp}` (docked in `MainWindow`) |
| Progress dialog (per action) | `src/xenadmin-ui/dialogs/operationprogressdialog.{h,cpp}` |
| Single-use async commands (e.g., ISO swap) | Typically constructed inside UI components (`StorageTabPage::onIsoComboBoxChanged`) |

Other infrastructure:
- Network worker thread: `src/xenlib/xen/connectionworker.{h,cpp}`
- Session duplication: `src/xenlib/xen/session.cpp` (`duplicateSession`)
- Command layer building blocks (menu/tool actions): `src/xenadmin-ui/commands/…`

### C# (reference implementation)

| Responsibility | Location | Notes |
|----------------|----------|-------|
| `ActionBase`/`AsyncAction` hierarchy | `xenadmin/XenModel/Actions/ActionBase.cs`, `AsyncAction.cs` | Original source of progress, RBAC, session duplication, audit logging |
| Composite actions (`MultipleAction`, `ParallelAction`) | `xenadmin/XenModel/Actions/MultipleAction.cs`, `ParallelAction.cs` | Orchestrate grouped operations |
| Session duplication logic | `xenadmin/XenModel/Network/XenConnection.cs`, `Session.cs` | `DuplicateSession` mirrors the C++ implementation |
| History/Events pane | `xenadmin/XenAdmin/TabPages/HistoryPage.cs`, `MainWindow.cs` | Feeds the Notifications → Events UI |
| Per-action progress dialog | `xenadmin/XenAdmin/Dialogs/ActionProgressDialog.cs` |

## Lifecycle of an Action (C++)

1. UI code constructs a subclass of `AsyncOperation` (e.g., `ChangeVMISOAction`).
2. **OperationManager** is told about it (`OperationManager::registerOperation`).
3. `runAsync()` duplicates the current session (see `AsyncOperation::createSession()`), providing a fresh `XenConnection`/`ConnectionWorker` for the action.
4. `AsyncOperation::run()` (implemented in subclasses) issues JSON-RPC calls, optionally polls asynchronous XenAPI tasks (`pollToCompletion`), updates progress/description, and sets result/error.
5. `OperationManager` listens for `stateChanged`, `progressChanged`, etc., and updates the Events dock (`HistoryPage`).
6. On completion, the duplicate session disconnects; the original login remains untouched.

## When to Use Actions

Use `AsyncOperation` subclasses for:
- VM lifecycle: start/stop/reboot, clone, migrate, convert to template, snapshots
- Host or pool operations: maintenance mode, join/eject, HA settings
- Storage/network mutations: attach/detach SRs, change PIFs, set ISO, create bonds
- Any workflow that should show progress, allow cancellation, or run off the UI thread

Avoid actions for:
- **Login/logoff** flows (`XenLib::connectToServer`) — handled synchronously before the UI is populated
- **Initial data population** (tree view, tab caches) — `XenLib` caches and `ConnectionsManager` provide direct data fetches
- **Periodic refresh/polling** (EventPoller, heartbeat) — these have dedicated infrastructure

## Implementing a New Action (C++)

1. Add `MyAction.{h,cpp}` under `src/xenlib/xen/actions/<category>/` replicating the C# behaviour.
2. Derive from `AsyncOperation`; implement `run()`; use `createSession()` to obtain the duplicate.
3. Update `xenlib.pro` to compile the new files.
4. Register the action in UI code: construct it, call `OperationManager::instance()->registerOperation(action);`, connect any UI-specific signals, then `action->runAsync()`.
5. If you need a progress dialog, instantiate `OperationProgressDialog` with the action.

## Release Checklist

- Ensure required C# dependencies (RBAC pre-checks, audit strings) are ported for the action.
- Wire up the new action in the command/menu layer or relevant UI.
- Confirm it appears in the Events dock and, if applicable, updates the status bar.
- If the action surfaces user prompts (dialogs, warnings), port the corresponding UI.

By keeping this mapping close to the original XenCenter logic we maintain parity, simplify ports from C#, and make the behaviour predictable for developers transitioning between the two codebases.
