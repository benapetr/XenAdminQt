# Action Framework Overview

User-triggered mutations and long-running workflows belong in `AsyncOperation`
subclasses under `src/xenlib/xen/actions/`. These mirror the original C#
`AsyncAction` architecture, including progress, cancellation, session
isolation, and operation history.

## Code map

| Responsibility | Location |
|----------------|----------|
| Base operation | `src/xenlib/xen/asyncoperation.{h,cpp}` |
| Concrete workflows | `src/xenlib/xen/actions/<category>/` |
| Global operation tracking | `src/xenlib/operations/operationmanager.{h,cpp}` |
| Composite workflows | `src/xenlib/operations/multipleaction.*`, `parallelaction.*`, `multipleactionlauncher.*` |
| Session duplication | `src/xenlib/xen/session.{h,cpp}` (`XenAPI::Session::DuplicateSession`) |
| Network worker | `src/xenlib/xen/network/connectionworker.{h,cpp}` |
| Events UI | `src/xenadmin-ui/tabpages/eventspage.{h,cpp,ui}` |
| Progress dialog | `src/xenadmin-ui/dialogs/actionprogressdialog.{h,cpp,ui}` |
| UI commands | `src/xenadmin-ui/commands/` |

The C# reference classes include `ActionBase`, `AsyncAction`, `MultipleAction`,
and `ParallelAction` under `xenadmin/XenModel/Actions/`.

## Lifecycle

1. UI code constructs a concrete action with its connection or typed object.
   The `AsyncOperation` constructor automatically calls
   `OperationManager::RegisterOperation(this)`; callers do not need to register
   it again.
2. Connect any UI-specific completion or prompt handling, then call
   `RunAsync()`. Follow existing call sites for ownership and auto-deletion.
3. The base worker lifecycle calls `createSession()` to duplicate the
   connection's current session. Subclasses use `GetSession()` inside `run()`.
4. The subclass calls static XenAPI bindings. For server tasks, use
   `SetRelatedTaskRef()` and `pollToCompletion()`; update description and
   progress through the base class.
5. `OperationManager` tracks state and progress for the notifications UI and
   progress dialogs. History visibility follows the operation's history flags
   and UI filters.
6. The base lifecycle cleans up the duplicate session it owns. Its shared
   token must remain valid for the original connection.

`RunSync()` also exists for callers that need synchronous execution, optionally
with a supplied session. It blocks the caller and should not be used to run a
network workflow on the UI thread.

## Adding an action

1. Find the C# equivalent and preserve its checks, prompts, RBAC requirements,
   task handling, and errors.
2. Add the subclass under `xen/actions/<category>/`; implement `run()` and use
   `GetSession()` with bindings such as `XenAPI::VM::async_start()`.
3. Add missing bindings under `xen/xenapi/` as thin wrappers. Keep workflow
   logic in the action, rather than constructing JSON-RPC in UI code.
4. Update `src/xenlib/xenlib.pro` and `src/xenlib/CMakeLists.txt`.
5. Wire the action into its UI command, dialog, or tab. Use `RunAsync()` and,
   when appropriate, `ActionProgressDialog`.
6. Verify enablement, progress, cancellation, failure reporting, and history.
   Use relevant automated tests and a test server for integration behavior.

VM lifecycle changes, storage/network mutations, host maintenance, and pool
configuration are action workflows. Connection setup, initial cache loading,
event polling, and heartbeat use their own infrastructure. Views normally
read typed cached objects and refresh through cache notifications.

See [core_concepts.md](core_concepts.md) for the connection/cache architecture
and [testing.md](testing.md) for the current test setup.
