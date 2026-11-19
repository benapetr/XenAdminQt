# Core Concepts and Code Map

XenAdminQt ports the original C# XenAdmin client to C++/Qt Widgets. The
`xenlib` library owns API workflows, sessions, caches, and network workers;
`xenadmin-ui` owns presentation and user interaction. The original source in
`xenadmin/`, when available, is the reference for behavior and naming.

## Login and sessions

`MainWindow::connectToServer()` collects credentials with `AddServerDialog`,
creates a `XenConnection`, registers it with `Xen::ConnectionsManager`, and
calls `XenConnectionUI::BeginConnect()`. That helper bridges prompts and errors
to `XenConnection::BeginConnect()` and the connection task.

- Connections and monitoring: `src/xenlib/xen/network/connection.*`,
  `connectionsmanager.*`, and `connecttask.h`.
- UI connection helper: `src/xenadmin-ui/network/xenconnectionui.*`.
- Authentication and token ownership: `XenAPI::Session` in
  `src/xenlib/xen/session.{h,cpp}`.
- Session duplication: `XenAPI::Session::DuplicateSession()` creates a separate
  transport sharing the existing token. Duplicates do not own that token and
  must not log out the original session during cleanup.

`XenLib` in `src/xenlib/xenlib.h` is a namespace exposing
`RegisterMetaTypes()`. It no longer provides connection or object-fetch APIs.

## Networking and threading

`src/xenlib/xen/network/connectionworker.{h,cpp}` handles transport I/O on a
worker thread. The static XenAPI bindings call through `XenAPI::Session` and
the transport layer. Synchronous calls still block their calling thread, even
though socket I/O runs elsewhere. Use asynchronous actions for UI-triggered
workflows; avoid blocking API calls on the UI thread.

`XenConnection::SendRequest()` and `SendRequestAsync()` remain as obsolete
legacy entry points. Follow the existing session and action interfaces for
new code.

## Cache and events

Each connection owns a `XenCache` (`src/xenlib/xencache.{h,cpp}`). Initial cache
loading uses `event.from` with an empty token. `EventPoller`
(`src/xenlib/xen/eventpoller.{h,cpp}`) continues from the returned token using a
dedicated duplicate session. Connection code applies event snapshots and
removals and emits cache-change notifications.

Views should prefer typed cached objects:

```cpp
QSharedPointer<VM> vm = connection->GetCache()->ResolveObject<VM>(vmRef);
if (vm && vm->IsValid())
    qDebug() << vm->GetName();
```

Typed objects read current cached properties. Check validity/eviction before
using them. `ResolveObjectData()` exposes raw records when needed;
`opaque_ref` is the canonical reference key. Refresh views through cache
notifications rather than the removed `XenLib::requestObjectData()` API.

The cache stores every class it receives as raw records, but only creates typed
objects for types registered in `XenCache::createObjectForType()`. A class
missing there compiles fine, yet `ResolveObject<T>()` and `GetAll<T>()` return
nothing for it. When adding an object class, register it in every place listed
under "Adding a typed object class" in `.github/copilot-instructions.md`.

## API bindings and workflows

Static bindings in `namespace XenAPI` mirror the C# object classes. Most files
are named `src/xenlib/xen/xenapi/xenapi_<Class>.{h,cpp}`, including
`xenapi_VM`, `xenapi_Host`, `xenapi_Pool`, `xenapi_SR`, and `xenapi_Task`.
`XenRpcAPI` in `src/xenlib/xen/api.{h,cpp}` supplies low-level JSON-RPC helpers.
Keep workflows in actions and use the static bindings from those actions.

```text
UI command/dialog
  → AsyncOperation subclass
  → XenAPI static binding (for example XenAPI::VM::async_start)
  → XenAPI::Session / JSON-RPC transport
  → ConnectionWorker / socket
```

Concrete actions live in `src/xenlib/xen/actions/`. The `AsyncOperation`
constructor registers the action with `OperationManager` automatically.
`RunAsync()` runs the workflow off the UI thread; the base lifecycle creates a
duplicate session, invokes `run()`, and cleans up the owned session. Actions
use `GetSession()`, `SetRelatedTaskRef()`, and `pollToCompletion()` to run and
track XenAPI tasks. See [actions.md](actions.md).

## UI and progress

- `MainWindow`, `MainWindowTreeBuilder`, navigation widgets, and
  `SelectionManager` coordinate tree selection, commands, and tabs.
- Commands under `src/xenadmin-ui/commands/` check selection and allowed
  operations, collect user input, and launch actions.
- Tabs derive from `BaseTabPage`, receive `SetObject(QSharedPointer<XenObject>)`,
  and implement `IsApplicableForObjectType(XenObjectType)`. Page visibility and
  dirty-state hooks control refreshes.
- Dialogs and wizards use Qt Designer `.ui` files.
- `OperationManager` in `src/xenlib/operations/operationmanager.{h,cpp}` tracks
  actions and exposes their state, progress, and errors.
- `EventsPage` in `src/xenadmin-ui/tabpages/eventspage.{h,cpp,ui}` displays
  operation history in the notifications UI.
- `ActionProgressDialog` in
  `src/xenadmin-ui/dialogs/actionprogressdialog.{h,cpp,ui}` displays progress
  for an individual workflow.

For example, `VMStorageTabPage::onIsoComboBoxChanged()` constructs a
`ChangeVMISOAction` and calls `RunAsync()`. The action performs the library
workflow while operation tracking and cache events update the UI.

## Boundaries

| Task | Owner |
|------|-------|
| Login, logout, connection readiness | Connection task, `XenConnection`, `XenAPI::Session`, UI helper |
| Initial state and ongoing updates | Connection cache loading and `EventPoller` |
| Tree and tab reads | Typed objects from the connection's `XenCache` |
| User mutations and long workflows | `AsyncOperation` subclasses |
| Grouped workflows | `MultipleAction`, `ParallelAction`, launchers under `xenlib/operations/` |
| Progress and history | `OperationManager`, `EventsPage`, `ActionProgressDialog` |

When adding source files, update both the relevant qmake project and CMake
source lists. See [testing.md](testing.md) for validation setup.
