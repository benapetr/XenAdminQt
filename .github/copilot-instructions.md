# XenAdmin Qt - AI Agent Instructions

This is a C++/Qt6 rewrite of XenAdmin (XenServer/XCP-ng management tool), porting from C#. The architecture follows the proven C# design while adapting to Qt patterns.

## Project Structure & Architecture

```
src/
├── xenlib/              # Shared library (XenServer API abstraction)
│   ├── xen/            # Connection, Session, API, AsyncOperations, EventPoller
│   └── operations/     # ParallelOperation, MultipleOperation framework
└── xenadmin-ui/        # Qt GUI application
    ├── commands/       # Command pattern (VM start/stop, host ops, etc.)
    ├── dialogs/        # UI dialogs with .ui files
    ├── tabpages/       # BaseTabPage subclasses (General, Storage, Network, Console, Performance, Snapshots)
    └── widgets/        # Custom widgets (QVncClient for VNC console)
```

**Critical Insight**: XenLib contains ALL Xen API business logic. The UI is a thin layer that invokes XenLib methods and displays results.
**API Direction**: Use `namespace XenAPI` static bindings for all XenServer API calls. `xen/api.h` (`XenRpcAPI`) is legacy, low-level plumbing only and should not be used directly in new code.

## Action Framework Expectations
- Prefer deriving from `AsyncOperation` (and companions under `src/xenlib/xen/actions/`) for any XenAPI workflow.
- Launch actions from commands/wizards and surface progress through `OperationProgressDialog`; do **not** add new direct `xenLib()->getAPI()->...` calls in UI code.
- When an action is missing, add a new C++ action mirroring the C# equivalent instead of inlining API logic.

## XenAPI Namespace (Static API Bindings)

All XenServer API bindings live in the **`XenAPI` namespace** with **static methods** organized by object type, matching the C# XenModel/XenAPI structure exactly:

```cpp
// File hierarchy mirrors C#: src/xenlib/xen/xenapi/ ↔ XenModel/XenAPI/
namespace XenAPI
{
    class VM
    {  // Static-only class
        public:
            static QString async_start(XenSession* session, const QString& vm, bool start_paused, bool force);
            static void clean_shutdown(XenSession* session, const QString& vm);
            // ... ~60 more VM methods
    };
        
    class Host { /* static methods */ };
    class Pool { /* static methods */ };
    class SR   { /* static methods */ };
    // ... etc.
}
```

**Key principles:**
- **One namespace, many classes**: All API bindings under `namespace XenAPI`, separate class per object type (VM, Host, Pool, SR, VDI, VIF, VBD, Network, Task, etc.)
- **Static methods only**: Classes have private constructors - they exist purely for namespace organization
- **File hierarchy matches C#**: `src/xenlib/xen/xenapi/VM.{h,cpp}` mirrors `XenModel/XenAPI/VM.cs`
- **1:1 method parity**: Each C# method (`async_start`, `clean_shutdown`, `assert_agile`) has identical Qt signature
- **Actions call XenAPI**: Action classes use `XenAPI::VM::async_start()` instead of building JSON-RPC inline

**Why this structure?**
- Eliminates code duplication (actions no longer build JSON-RPC manually)
- Matches C# exactly for easier porting (1:1 mapping of method calls)
- Extensible (add `XenAPI::Host`, `XenAPI::Pool` as needed)
- Testable (can mock XenAPI layer separately from actions)

**Implementation pattern** (see `src/xenlib/xen/xenapi/VM.cpp`):
```cpp
QString VM::async_start(XenSession* session, const QString& vm, bool start_paused, bool force)
{
    if (!session || !session->isLoggedIn())
        throw std::runtime_error("Not connected to XenServer");
    
    QVariantList params;
    params << session->getSessionId() << vm << start_paused << force;
    
    XenRpcAPI api(session);  // Low-level JSON-RPC client
    QByteArray request = api.buildJsonRpcCall("VM.async_start", params);
    QByteArray response = session->sendApiRequest(request);
    return api.parseJsonRpcResponse(response).toString();  // Returns task ref
}
```

**DO:**
- Add new XenAPI classes as separate files (`xenapi/Host.h`, `xenapi/Pool.h`)
- Use XenAPI methods in actions: `XenAPI::VM::async_start(sess, vmRef, false, false)`
- Match C# signatures exactly (use C# XML doc comments for documentation)
- In all C++ source files give this-> prefix to every member-variable or member-function access that currently lacks it. Do not change non-member references. This makes the code easier to read as it's instantly obvious if we are working with some local scoped variable / function or a class member.

**DON'T:**
- Build JSON-RPC manually in action classes (use XenAPI methods instead)
- Put business logic in XenAPI classes (they're thin wrappers - actions handle workflows)
- Add new direct dependencies on `xen/api.h` in UI or actions; migrate existing ones to `XenAPI` classes
- Mix up `XenRpcAPI` (low-level client) with `namespace XenAPI` (high-level bindings)

## Architecture Patterns

### 1. Worker Thread Pattern (Matches C# XenAdmin)

**All network I/O runs on a background worker thread** (`ConnectionWorker`). This eliminates UI freezing and race conditions:

```cpp
// Connection flow: TCP/SSL on worker thread → XenSession::login() on main thread
ConnectionWorker::run() {
    connectToHostSync();    // Blocks worker thread (not UI)
    sslHandshakeSync();     // Blocks worker thread (not UI)
    emit connectionEstablished();  // Signal main thread
    eventPollLoop();        // Process queued API requests
}

// Main thread queues requests to worker
XenConnection::sendRequest(xmlData) {
    worker->queueRequest(xmlData);
    worker->waitForResponse();  // Blocks until worker processes it
}
```

**Key files**: 
- `src/xenlib/xen/connectionworker.{h,cpp}` - Worker thread implementation
- `src/xenlib/xen/connection.{h,cpp}` - Connection wrapper
- `docs/SIGNAL_FLOW.md` - Complete connection sequence diagram
- `docs/ASYNC_REFACTORING.md` - Why we use this pattern

### 2. XenCache System (Transparent Caching)

All XenServer objects (VMs, Hosts, Pools, SRs) are cached for instant access. Cache is populated on connection and kept synchronized:

```cpp
// Cache is checked automatically - you don't check it manually
xenLib->requestObjectData("vm", vmRef);  
// Signal emitted instantly if cached, or after API call if not

connect(xenLib, &XenLib::objectDataReceived, 
    [](QString type, QString ref, QVariantMap data) {
        // Data came from cache or API - you don't need to care which
    });
```

**Never** manually check cache before making requests. The request methods handle cache lookups transparently.

**Key files**: `src/xenlib/xencache.{h,cpp}`

#### XenCache Object Access (C#-style)

XenCenter (C#) works with typed XenObjects where `opaque_ref` is always present and properties are read from the cache. The Qt port now mirrors this:

- **Preferred**: `ResolveObject(...)` / `ResolveObject<T>(...)` returns `QSharedPointer<XenObject>` (or typed subclass) that lazily reads from cache.
- **Legacy**: `ResolveObjectData(...)` returns raw `QVariantMap` and should be used only for compatibility.

```cpp
// Preferred: typed object
QSharedPointer<Host> host = cache->ResolveObject<Host>("host", hostRef);
if (host && host->isValid())
    qDebug() << host->nameLabel();

// Legacy: raw data
QVariantMap hostData = cache->ResolveObjectData("host", hostRef);
```

**Rules:**
- Use `ResolveObject<T>()` everywhere possible. Only fall back to `ResolveObjectData()` when a typed class does not exist yet.
- Treat `opaque_ref` as the canonical ref key in cache data. Avoid guessing alternate keys (`ref`, `opaqueRef`, etc.).
- Objects are lazy shells: they read current data from `XenCache` on demand.
- On cache eviction, objects are marked `evicted` (check `isEvicted()` / `isValid()`).

### 3. API Call Patterns: Async vs Blocking

XenLib provides **two** ways to make XenAPI calls:

1. **Blocking (Sync)**: `XenAPI` methods - block calling thread until worker completes request
2. **Non-blocking (Async)**: `XenConnection::sendRequestAsync()` - returns immediately, response via signal

#### Threading Model

**All network I/O happens on ConnectionWorker thread**, preventing UI freezes. However:

- **Sync methods** (`api->startVM()`) **block the caller** while waiting for worker response
- **Async pattern** (`connection->sendRequestAsync()`) returns requestId immediately, delivers response via `apiResponse` signal

```
Sync:  UI thread → sendRequest() → [wait for worker] → return result
Async: UI thread → sendRequestAsync() → return requestId → continue execution
                                               ↓
                    Later: apiResponse signal → handle result
```

#### Current Project Pattern: Sync API Methods

The project currently uses **sync `XenAPI` methods** for most operations. This is acceptable because:
- Network I/O still on worker thread (UI doesn't freeze from network latency)
- Operations complete quickly (< 1 second typically)
- Simpler code than full async pattern

```cpp
// Commands use sync pattern (see StartVMCommand, etc.)
bool success = api->startVM(vmRef);
if (success)
{
    this->mainWindow()->showStatusMessage("VM started", 5000);
} else
{
    QMessageBox::warning(this, "Error", "Failed to start VM");
}
```

#### When to Use Async Pattern

Use `sendRequestAsync()` for:
- Custom API calls not in `XenAPI` class
- Bulk data fetching (VMs, hosts, pools) - **already used by XenLib internally**
- Building custom high-level async wrappers

```cpp
// XenLib uses async pattern internally for data fetching:
QString xmlRequest = api->buildXmlRpcCall("VM.get_all_records", params);
int requestId = connection->sendRequestAsync(xmlRequest.toUtf8());
pendingRequests[requestId] = RequestType::GetVirtualMachines;

// Handle response via signal
connect(connection, &XenConnection::apiResponse, this, &XenLib::onConnectionApiResponse);

void XenLib::onConnectionApiResponse(int requestId, const QByteArray& response)
{
    if (!pendingRequests.contains(requestId))
        return;
    
    RequestType type = pendingRequests.take(requestId);
    QVariant data = api->parseXmlRpcResponse(QString::fromUtf8(response));
    
    if (type == GetVirtualMachines)
    {
        emit virtualMachinesReceived(processData(data));
    }
}
```

#### For Long-Running Tasks: Use AsyncOperations

For operations with progress tracking (migration, import/export, snapshots):

```cpp
AsyncOperation* op = new AsyncOperation(xenLib->getAPI(), this);
connect(op, &AsyncOperation::progress, [](int percent) { 
    progressBar->setValue(percent); 
});
connect(op, &AsyncOperation::completed, [](bool success) {
    if (success) { /* update UI */ }
});
op->startVirtualMachine(vmRef);
```

**Key files**: 
- `src/xenlib/xen/connection.{h,cpp}` - sendRequest() vs sendRequestAsync()
- `src/xenlib/xen/connectionworker.cpp` - Worker thread request processing
- `src/xenlib/xenlib.cpp` - Async pattern examples (onConnectionApiResponse)
- `src/xenlib/asyncoperation.{h,cpp}` - Task progress tracking
- `src/xenlib/asyncoperations.{h,cpp}` - High-level async operations
- `src/xenlib/paralleloperation.{h,cpp}` - Parallel execution
- `src/xenlib/multipleoperation.{h,cpp}` - Sequential execution

### 4. Command Pattern (UI Operations)

All user-initiated operations (menu items, context menu, toolbar) use the Command pattern:

```cpp
class StartVMCommand : public Command
{
    bool canRun() const override
    {
        return getSelectedObjectType() == "vm" && 
               xenLib()->getVMPowerState(getSelectedObjectRef()) == "Halted";
    }
    
    void run() override
    {
        AsyncOperation* op = new AsyncOperation(xenLib()->getAPI(), this);
        op->startVirtualMachine(getSelectedObjectRef());
    }
    
    QString menuText() const override { return "Start"; }
};
```

Commands are created by `ContextMenuBuilder::buildContextMenu()` based on selected object type.

**Key files**: 
- `src/xenadmin-ui/commands/command.h` - Base class
- `src/xenadmin-ui/commands/contextmenubuilder.{h,cpp}` - Menu generation
- `src/xenadmin-ui/commands/*command.{h,cpp}` - 40+ command implementations

### 5. Tab Page System

Object-specific tabs inherit from `BaseTabPage`. MainWindow shows applicable tabs based on selection:

```cpp
class StorageTabPage : public BaseTabPage
{
    bool isApplicableForObjectType(const QString& type) const override
    {
        return type == "vm" || type == "host" || type == "sr";
    }
    
    void setXenObject(const QString& type, const QString& ref, const QVariantMap& data) override
    {
        // Update display based on object type
    }
};
```

**Key files**:
- `src/xenadmin-ui/tabpages/basetabpage.{h,cpp}` - Base class
- `src/xenadmin-ui/tabpages/*tabpage.{h,cpp}` - 6 implementations
- `src/xenadmin-ui/mainwindow.cpp::initializeTabPages()` - Tab registration

## UI Conventions

### All dialogs MUST use Qt Designer .ui files

**Do NOT create UI programmatically**. Use Qt Designer:

```cpp
// ✅ CORRECT
class MyDialog : public QDialog {
    Ui::MyDialog *ui;
public:
    MyDialog(QWidget *parent = nullptr) : QDialog(parent), ui(new Ui::MyDialog) {
        ui->setupUi(this);  // Layout from .ui file
        ui->nameLineEdit->setText("Example");  // Access widgets via ui->
    }
};

// ❌ WRONG
QVBoxLayout* layout = new QVBoxLayout(this);  // Don't do this
QLabel* label = new QLabel("Text", this);
layout->addWidget(label);
```

Steps to create new dialog:
1. Create `.ui` file in `src/xenadmin-ui/dialogs/`
2. Add to `xenadmin-ui.pro` FORMS section
3. Build system generates `ui_mydialog.h`
4. Access widgets via `ui->widgetName`

**Key files**: `README.md` section "Creating New Dialogs"

### Wizard Pattern

Multi-page wizards use `QWizard` + `.ui` files for each page:

```cpp
wizard->addPage(new IntroPage());
wizard->addPage(new ConfigPage());
wizard->setWindowTitle("New VM Wizard");
```

**Key files**: `src/xenadmin-ui/dialogs/{newvmwizard,importwizard,exportwizard,newsrwizard,newnetworkwizard}.{h,cpp}`

## Build System

**CRITICAL**: Always build from `release/` folder, never from `src/`:

```bash
cd release/
make -j12 # don't use $(nproc) in scripts, these can't be pre-approved
```

Why? The `release/` folder contains pre-generated qmake files with correct dependencies. Building in `src/` causes link errors.

**Testing**: Unit tests in `src/testconn/` - run with `./run_tests.sh`. See `docs/TESTING.md`.

## XenServer API Patterns

### XML-RPC Structure

All API calls use XML-RPC with session authentication:

```xml
<methodCall>
  <methodName>VM.start</methodName>
  <params>
    <param><value><string>OpaqueRef:abc123</string></value></param>  <!-- session -->
    <param><value><string>OpaqueRef:vm-ref</string></value></param>   <!-- VM ref -->
  </params>
</methodCall>
```

Responses have `Status` (Success/Failure) and `Value` fields. XenLib parses these automatically.

### Object References

All XenServer objects have opaque references: `OpaqueRef:12345678-1234-1234-1234-123456789abc`

Use `XenLib::requestObjectData(type, ref)` to get full record. Cache handles deduplication.

### Common Object Types

- `"vm"` - Virtual machines (running or stopped)
- `"host"` - Physical XenServer hosts
- `"pool"` - Resource pools
- `"sr"` - Storage repositories
- `"network"` - Virtual networks
- `"vbd"` - Virtual block devices (disk attachments)
- `"vdi"` - Virtual disk images
- `"vif"` - Virtual network interfaces
- `"console"` - VM consoles (VNC/RDP)
- `"task"` - Async operation tasks

**Key files**: `docs/xapi.md`, `docs/xapi_list.md` - Complete API documentation

## Common Pitfalls

### ❌ Don't Block the UI Thread

```cpp
// ❌ WRONG
QVariantList vms = xenLib->getVirtualMachines();  // Blocks UI
foreach (QVariant vm, vms) { /* ... */ }

// ✅ CORRECT
xenLib->requestVirtualMachines();
connect(xenLib, &XenLib::virtualMachinesReceived, [](QVariantList vms) {
    foreach (QVariant vm, vms) { /* ... */ }
});
```

### ❌ Don't Manually Check Cache

```cpp
// ❌ WRONG
if (cache->contains("vm", vmRef)) {
    data = cache->resolve("vm", vmRef);
} else {
    xenLib->requestObjectData("vm", vmRef);
}

// ✅ CORRECT
xenLib->requestObjectData("vm", vmRef);  // Automatically uses cache
```

### ❌ Don't Mix Connection Logic with XenSession

`ConnectionWorker` handles TCP/SSL only. `XenSession::login()` handles authentication separately. See `docs/ARCHITECTURE_FIX.md` for why this separation matters.

### ❌ Don't Create UI Programmatically

Use `.ui` files for all layouts. This is enforced project-wide - dialogs without `.ui` files will be rejected.

## Reference C# Codebase

The `xenadmin/` folder contains the original C# implementation. When porting features:

1. Find equivalent in `xenadmin/XenAdmin/` or `xenadmin/XenModel/`
2. Understand the C# pattern (especially connection/threading model)
3. Adapt to Qt patterns (signals/slots instead of events)
4. Maintain same logic flow and error handling

**Example**: C# `XenConnection.cs` → Qt `src/xenlib/xen/connection.{h,cpp}`

## Development Workflow

1. **Add command**: Create `MyCommand : Command` → Register in `ContextMenuBuilder`
2. **Add dialog**: Create `.ui` file → Implement dialog class → Connect signals
3. **Add tab page**: Create `MyTabPage : BaseTabPage` → Register in `MainWindow::initializeTabPages()`
4. **Add XenLib method**: Add to `xenlib.h` → Implement in `xenlib.cpp` → Use `XenAPI` for actual calls
5. **Test**: Build in `release/`, run `./xenadmin-ui/xenadmin-qt`, connect to XenServer

## Key Documentation Files

- `README.md` - Project overview, build instructions, architecture guide
- `src/TODO.md` - Comprehensive feature porting checklist
- `docs/SIGNAL_FLOW.md` - Connection sequence diagram
- `docs/ASYNC_REFACTORING.md` - Why we use worker threads
- `docs/ARCHITECTURE_FIX.md` - Connection/Session separation
- `docs/CONSOLE_IMPLEMENTATION.md` - VNC console details
- `docs/TESTING.md` - Unit test guide
- `docs/xapi.md`, `docs/xapi_list.md` - XenServer API reference

## Project Context

**Project folder name**: `xenadmin_qt` (note the underscore)
**Target platforms**: Linux (primary), Windows/macOS (secondary)
**Qt version**: Qt6
**Build system**: qmake (not CMake)
**Original codebase**: C# WinForms → Qt6 Widgets
**Status**: Core functionality complete (connection, VMs, hosts, storage, commands, wizards, console). Advanced features (HA, WLB, metrics, alerts) in progress.

## Coding style
Uses old-school Microsoft C++ style with braces on new lines. We are also prefixing local member accesses with this-> to improve code readability.

Public members (functions and variables) use PascalCase, private/protected members use camelCase with a trailing underscore for member variables, this makes the scope obvious without context.

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
