# XenAdmin Qt - AI Agent Instructions

This is a C++/Qt Widgets port of the C# XenAdmin client for XCP-ng/XenServer.
Preserve the original naming, workflow, and error handling while adapting to Qt.
The project is alpha software; implementation does not imply complete parity or
validation against a live server.

## Project Structure

- `src/xenadmin-ui/`: GUI application, commands, dialogs, tab pages, navigation,
  settings, notifications, and `ConsoleView/` console widgets.
- `src/xenlib/`: shared business logic, typed Xen objects, cache, search, alerts,
  import/export helpers, and operation tracking.
- `src/xenlib/xen/actions/`: user workflows grouped by object/category.
- `src/xenlib/xen/xenapi/`: static `XenAPI` bindings, generally named
  `xenapi_<Class>.{h,cpp}` (for example `xenapi_VM.h`).
- `src/xenlib/xen/network/`: connections, workers, HTTP transport, certificates,
  heartbeat, and `ConnectionsManager`.
- `tests/`: qmake QtTest projects for the library and UI helpers.
- `packaging/`, `debian/`, `toolchains/`: packaging and platform builds.
- `xenadmin/`: original C# reference source, when available locally.

## Architecture and API Boundaries

Keep Xen API business logic in `xenlib`. UI commands and dialogs collect input,
construct library actions, and display results.

- Use static bindings such as `XenAPI::VM::async_start()` for API calls in
  actions. `XenRpcAPI` in `src/xenlib/xen/api.{h,cpp}` is low-level JSON-RPC
  plumbing; do not introduce direct dependencies on it in UI or actions.
- Bindings are thin wrappers. Put multi-step workflows in `AsyncOperation`
  subclasses and mirror the corresponding C# action when porting.
- Sessions are `XenAPI::Session`, declared in `src/xenlib/xen/session.h`.
  `XenLib` in `xenlib.h` is now a namespace exposing `RegisterMetaTypes()`,
  not a connection or data-fetching facade.

### Connections, sessions, and threading

`MainWindow::connectToServer()` uses `AddServerDialog`, adds a `XenConnection`
to `Xen::ConnectionsManager`, and calls `XenConnectionUI::BeginConnect()`.
The UI helper delegates to `XenConnection::BeginConnect()`.

`ConnectionWorker` handles transport I/O on a worker thread. Synchronous API
calls still block their caller: keep blocking work off the UI thread. The
legacy `XenConnection::SendRequest()` and `SendRequestAsync()` entry points are
marked obsolete; use existing session/binding/action patterns for new code.

Actions use `XenAPI::Session::DuplicateSession()` through the base operation
lifecycle. A duplicate has a separate transport and shares the session token;
its cleanup must not invalidate the original login. Event polling also uses a
dedicated duplicate session.

### Cache and typed objects

Each `XenConnection` owns a `XenCache`. Initial loading uses `event.from` with
an empty token; `EventPoller` continues from the returned token, and connection
code applies updates to the cache.

Prefer typed access:

```cpp
QSharedPointer<VM> vm = connection->GetCache()->ResolveObject<VM>(vmRef);
if (vm && vm->IsValid())
    qDebug() << vm->GetName();
```

- Use `ResolveObject<T>()` and typed object properties where possible.
- Use `ResolveObjectData()` for raw records only when necessary.
- `opaque_ref` is the canonical reference key in cache records.
- Objects read current cached data; check validity/eviction before use.
- Use typed formatting helpers rather than duplicating display logic.
- Let cache signals refresh views. There is no `XenLib::requestObjectData()`.

### Action lifecycle and progress

Derive user mutations and long-running workflows from `AsyncOperation`.
Its constructor automatically calls `OperationManager::RegisterOperation()`;
UI code does not need to register it again.

Implement `run()` using `GetSession()` and static XenAPI bindings. Use
`SetRelatedTaskRef()` and `pollToCompletion()` for server tasks, and update
progress/description through the base class. Start from UI code with
`RunAsync()`; the base lifecycle creates and cleans up the duplicate session.
Follow existing actions for cancellation, RBAC checks, and error handling.

- Base: `src/xenlib/xen/asyncoperation.{h,cpp}`
- Tracking: `src/xenlib/operations/operationmanager.{h,cpp}`
- Composite workflows: `multipleaction.*`, `parallelaction.*`, and
  `multipleactionlauncher.*` in `src/xenlib/operations/`
- History UI: `src/xenadmin-ui/tabpages/eventspage.{h,cpp,ui}`
- Progress dialog: `src/xenadmin-ui/dialogs/actionprogressdialog.{h,cpp,ui}`

Connection setup, initial cache loading, event polling, and heartbeat have
separate infrastructure; they are not user mutation actions.

### Commands and tabs

Menu/toolbar operations use `Command` subclasses under
`src/xenadmin-ui/commands/`. Check selected objects and allowed operations in
`CanRun()`, and construct workflows in `Run()`. Follow existing commands for
multi-selection, prompts, and progress handling.

`SelectionManager`, `ContextMenuBuilder`, and `MainWindow` coordinate selection
and command enablement. Tab pages derive from `BaseTabPage`, receive a typed
object through `SetObject(QSharedPointer<XenObject>)`, and implement
`IsApplicableForObjectType(XenObjectType)`. Use `OnPageShown()`,
`OnPageHidden()`, and dirty-state handling for updates. Register new pages in
`MainWindow` alongside existing pages.

## UI Conventions

All new dialogs and layouts must use Qt Designer `.ui` files. Instantiate the
generated UI with `setupUi(this)` and access widgets through `ui->`.
Multi-page wizards use `QWizard` and Designer forms; follow the existing wizard
structure. Add new source/header/form entries to the corresponding qmake and
CMake build files.

## Build and Testing

Keep generated build files outside `src/`. For the existing local qmake build:

```bash
cd release/
make -j12
```

To configure a fresh qmake build from the repository root (use the qmake for
your installed Qt; it may be named `qmake6`):

```bash
mkdir -p build
cd build
qmake ../src/xenadminqt.pro
make -j12
```

Qt 6 is the primary target, with Qt 5 compatibility in the qmake projects.
CMake is also supported and requires Qt 6:

```bash
cmake -S . -B build-cmake
cmake --build build-cmake -j12
```

See `packaging/README.md` for dependencies and platform packaging, and
`.github/workflows/build.yml` for CI configurations. Keep qmake and CMake
source lists in sync. qmake supports `CONFIG+=no_crypto` and
`CONFIG+=no_zlib`; CMake exposes `XENADMIN_NO_ZLIB`.

QtTest projects live in `tests/xenlib/` and `tests/xenadmin-ui/`; see
`docs/testing.md` for setup and current limitations. Run relevant checks for
code changes. Live-server behavior requires integration testing.

## Development Workflow

1. Find the corresponding C# implementation under `xenadmin/XenAdmin/` or
   `xenadmin/XenModel/` when porting a feature.
2. Add missing API bindings in `xen/xenapi/` and workflows in `xen/actions/`.
3. Wire commands, Designer forms, and tab pages into the UI.
4. Update both build systems for new files.
5. Build and run appropriate tests; validate server workflows against a test
   XCP-ng/XenServer environment when available.

## Documentation

- `README.md`: project overview and build entry points.
- `docs/core_concepts.md`: current architecture and code map.
- `docs/actions.md`: action lifecycle and implementation guidance.
- `docs/testing.md`: QtTest setup and limitations.
- `docs/status.md`: implementation status and historical validation notes.
- `CHANGELOG.md`: user-visible changes.
- `docs/xapi.md`, `docs/xapi_list.md`: XAPI reference.
- `packaging/README.md`: dependencies and package scripts.

## Coding style
Uses old-school Microsoft C++ style with braces on new lines. We are also prefixing local member accesses with this-> to improve code readability.

Public members (functions and variables) use PascalCase, private/protected members use camelCase for member variables and functions, this makes the scope obvious without context.

Example:
```cpp
class MyClass
{
    public:
        void MyFunction()
        {
            this->myMember = 5;
            if (this->myMember > 0)
            {
                this->doSomething();
            } else // else should be on same line as closing brace to save space
            {
                // Handle else case
            }

            // Simple statements don't need braces
            if (this->myMember == 0)
                this->myMember = 1;
        }
    private:
        int myMember;
        void doSomething() { /* ... */ }
};
```
