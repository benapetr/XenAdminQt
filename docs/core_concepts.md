# Core Concepts and Code Map

This Qt port mirrors the original XenCenter architecture: a background library (`xenlib`) that owns sessions, caching, and network workers; a UI layer (`xenadmin-ui`) that renders tree tabs, dialogs, and commands; and an action framework for user workflows. This document gives new contributors a high‑level map.

## Login & Session Management

- **Entry point:** `MainWindow::connectToServer()` → `XenLib::connectToServer()` (C# analogue: `ConnectionsManager.Connect`).
- **Session:** `src/xenlib/xen/session.{h,cpp}` wraps the JSON-RPC `session.login_with_password`. A login creates one `XenConnection` (with `ConnectionWorker` thread), stores the session token, and wires up `XenAPI` wrappers.
- **Duplication:** Actions and background pollers call `XenSession::duplicateSession()` to open a parallel connection using the same token. Duplicates never call `logout()`; they just disconnect the worker when done (same as C#’s `DuplicateSession`).
- **Authentication failures / prompts:** Routed through `MainWindow::onAuthenticationFailed` (see `connectdialog` for UI).

## Connection Worker & Networking

- `src/xenlib/xen/connectionworker.{h,cpp}` is the worker thread that owns the TCP/SSL socket. All network I/O runs there so the UI thread never blocks. Queue requests with `XenConnection::sendRequest` / `sendRequestAsync`.
- The C# equivalent is `XenModel/Network/Connection`, implemented with a background thread for each duplicate session.

## Cache & Data Retrieval

- **Cache:** `src/xenlib/xencache.{h,cpp}` keeps VMs/Hosts/SRs/etc. hydrated. On login, `XenLib` fetches the canonical "get_all_records" for the core classes and populates the cache. The tree UI and tab pages read from this cache (mirroring XenCenter's `Cache` and `ConnectionsManager`).
- **Event polling:** `src/xenlib/xen/eventpoller.{h,cpp}` duplicates the session and runs `event.from` to keep the cache fresh.
- **UI fetches:** For one-off reads (e.g. Storage tab needs the latest VDI list) we still call `XenLib::requestObjectData`, which consults the cache and, if stale, issues a direct XenAPI call. That matches XenCenter's model: background data uses direct API, not actions.

## XenAPI Namespace (Static API Bindings)

All XenServer API bindings are organized in the **`XenAPI` namespace** with static methods, mirroring the C# XenModel/XenAPI structure exactly. This provides a clean separation between low-level JSON-RPC (handled by `XenRpcAPI` class) and high-level API bindings (static methods in `namespace XenAPI`).

### Architecture Overview

```
Action Layer        →  XenAPI::VM::async_start()
                       XenAPI::Host::reboot()
                       XenAPI::Pool::join()
                          ↓
API Binding Layer   →  namespace XenAPI { class VM { static methods } }
                          ↓
Transport Layer     →  XenRpcAPI (builds JSON-RPC, parses responses)
                          ↓
Network Layer       →  XenSession → ConnectionWorker → TCP/SSL
```

### File Organization

The file hierarchy mirrors C# exactly:

| Qt Path | C# Path | Purpose |
|---------|---------|---------|
| `src/xenlib/xen/xenapi/VM.{h,cpp}` | `XenModel/XenAPI/VM.cs` | VM operations (start, shutdown, suspend, migrate) |
| `src/xenlib/xen/xenapi/Host.{h,cpp}` | `XenModel/XenAPI/Host.cs` | Host operations (reboot, shutdown, maintenance mode) |
| `src/xenlib/xen/xenapi/Pool.{h,cpp}` | `XenModel/XenAPI/Pool.cs` | Pool operations (join, eject, HA configuration) |
| `src/xenlib/xen/xenapi/SR.{h,cpp}` | `XenModel/XenAPI/SR.cs` | Storage operations (scan, attach, detach) |
| `src/xenlib/xen/xenapi/Task.{h,cpp}` | `XenModel/XenAPI/Task.cs` | Task polling (get_progress, get_status, cancel) |

### Code Example

**C# Original** (`XenModel/XenAPI/VM.cs`):
```csharp
namespace XenAPI
{
    public static class VM
    {
        public static XenRef<Task> async_start(Session session, string _vm, bool _start_paused, bool _force)
        {
            return session.JsonRpcClient.async_vm_start(session.opaque_ref, _vm, _start_paused, _force);
        }
    }
}
```

**Qt Port** (`src/xenlib/xen/xenapi/VM.h`):
```cpp
namespace XenAPI
{
    class XENLIB_EXPORT VM
    {
    public:
        /// <summary>
        /// Start the specified VM.  This function can only be called with the VM is in the Halted State.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="vm">The VM to start</param>
        /// <param name="start_paused">Instantiate VM in paused state if set to true.</param>
        /// <param name="force">Attempt to force the VM to start. If this flag is false then the VM may fail pre-boot safety checks (e.g. if the CPU the VM last booted on looks substantially different to the current one)</param>
        /// <returns>Task reference</returns>
        static QString async_start(XenSession* session, const QString& vm, bool start_paused, bool force);
        
        // ~60 more VM methods...
        
    private:
        VM();  // Private constructor - static-only class
        ~VM();
    };
}
```

**Usage in Action** (`src/xenlib/xen/actions/vm/vmstartaction.cpp`):
```cpp
void VMStartAction::doAction(int start, int end)
{
    XenSession* sess = session();
    VM* vmObj = vm();
    
    // Call static XenAPI method (matches C# exactly)
    QString taskRef = XenAPI::VM::async_start(sess, vmObj->opaqueRef(), false, false);
    
    setRelatedTaskRef(taskRef);
    pollToCompletion(taskRef, start, end);
}
```

### Key Principles

1. **One namespace, many classes**: All API bindings under `namespace XenAPI`, separate class per object type
2. **Static methods only**: Classes have private constructors - they exist purely for namespace organization
3. **1:1 method parity**: Each C# method has an identical Qt signature (preserving C# XML doc comments)
4. **Actions call XenAPI**: Action classes use `XenAPI::VM::async_start()` instead of building JSON-RPC inline
5. **Thin wrappers**: XenAPI methods just validate session, build params, call `XenRpcAPI`, return result

### Adding New API Classes

When porting a feature that needs new API calls:

1. **Check if C# uses XenAPI class**: Look in `xenadmin/XenModel/XenAPI/` for the method
2. **Create matching Qt class**: Add `src/xenlib/xen/xenapi/ClassName.{h,cpp}`
3. **Copy method signatures**: Use C# XML doc comments and exact parameter names
4. **Implement pattern**: Validate session → build params → call XenRpcAPI → return result
5. **Update build**: Add to `src/xenlib/xenlib.pro` SOURCES/HEADERS
6. **Use in actions**: Call `XenAPI::ClassName::method()` instead of inline JSON-RPC

### XenRpcAPI vs XenAPI Namespace

**Don't confuse these two:**

- **`XenRpcAPI`** (class in `src/xenlib/xen/api.{h,cpp}`): Low-level JSON-RPC client - builds requests, parses responses, handles session tokens
- **`namespace XenAPI`** (in `src/xenlib/xen/xenapi/*.{h,cpp}`): High-level static API bindings - provides typed methods matching C# XenAPI

```cpp
// XenRpcAPI - low-level (used internally by XenAPI namespace)
XenRpcAPI api(session);
QByteArray request = api.buildJsonRpcCall("VM.async_start", params);
QByteArray response = session->sendApiRequest(request);
QVariant result = api.parseJsonRpcResponse(response);

// XenAPI namespace - high-level (used by actions)
QString taskRef = XenAPI::VM::async_start(session, vmRef, false, false);
```

Actions and UI code should **only use the `XenAPI` namespace**, never call `XenRpcAPI` directly.

## Action Framework (Mutation Workflows)

- Concrete classes live under `src/xenlib/xen/actions/…` (e.g., `vm/changevmisoaction.{h,cpp}`). They inherit from `AsyncOperation`, which duplicates the session, runs the workflow, tracks progress, and emits state changes.
- UI code should use actions for user-triggered operations (start/stop VM, migrate host, attach SR, etc.). See `docs/actions.md` for full details.
- The history/events UI is provided by `OperationManager` + `HistoryPage` (see next section).

## Operation Tracking & Progress

- **Manager:** `src/xenadmin-ui/operations/operationmanager.{h,cpp}` subscribes to every `AsyncOperation`. It records title, status, progress, and errors.
- **History UI:** `src/xenadmin-ui/widgets/historypage.{h,cpp}` shows the operation log in an “Events” dock (`MainWindow` adds a View → Show Events toggle).
- **Progress dialog:** `src/xenadmin-ui/dialogs/operationprogressdialog.{h,cpp}` reuses the same signals to display per-operation progress (equivalent to XenCenter’s `ActionProgressDialog`).

## UI Layer Structure

- **Main window:** `src/xenadmin-ui/mainwindow.{h,cpp}` drives the splitter layout, tree widget, tab widget (see `BaseTabPage` derivatives under `tabpages/`), and status bar.
- **Commands:** `src/xenadmin-ui/commands/` contains the menu/toolbar actions. Each command pulls selected objects, constructs an action, registers it with `OperationManager`, and kicks it off.
- **Dialogs & Wizards:** `src/xenadmin-ui/dialogs/` (Qt Designer `.ui` files) handle user interaction; they call into `XenLib` or construct actions as needed.

## Typical Flow (e.g. “Change VM ISO”)

1. User selects a VM in the tree; Storage tab shows attached CD drives using cached data.
2. User picks a new ISO. `StorageTabPage::onIsoComboBoxChanged` constructs `ChangeVMISOAction`, calls `OperationManager::registerOperation`, hooks completion signals, and runs `action->runAsync()`.
3. The action duplicates the session, issues `VBD.async_eject/insert`, polls until success/failure.
4. `OperationManager` updates the Events dock while the existing progress dialog (optional) displays state.
5. When the action finishes, the tab refreshes the cache view via direct API (`requestObjectData`).

## When to Use What

| Task | Implementation | Notes |
|------|----------------|-------|
| Login/logout | Direct JSON-RPC in `XenLib`/`XenSession` | Actions are **not** used |
| Tree/tab population | Cached data in `XenLib` (`requestVirtualMachines`, etc.) | Direct API; background event poll keeps cache fresh |
| User operations (mutations, long-running workflows) | `AsyncOperation` subclasses (see `docs/actions.md`) | Always register with `OperationManager` |
| Event history | `HistoryPage` dock | Shows every registered action |
| Command handling | `commands/…` classes | Build actions or invoke direct reads |

## Cross-Referencing with C#

| C++ Component | C# Counterpart |
|---------------|----------------|
| `AsyncOperation` | `AsyncAction` (XenModel/Actions) |
| `OperationManager` + `HistoryPage` | `ConnectionsManager.History`, `HistoryPage` (Notifications) |
| `XenSession::duplicateSession` | `Session.DuplicateSession` |
| `ConnectionWorker` | `XenConnection` background thread |
| Cache / event poller | `Cache` + `EventPoller` |

Keep these boundaries in mind when adding features; they’ll keep the port aligned with the original XenCenter behaviour.
