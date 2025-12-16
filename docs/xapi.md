# XenAPI (XAPI) Documentation — JSON-RPC Edition

## Table of Contents
1. [Overview](#overview)
2. [JSON-RPC Protocol](#json-rpc-protocol)
3. [Authentication Flow](#authentication-flow)
4. [Opaque References](#opaque-references)
5. [Object Model](#object-model)
6. [Common API Patterns](#common-api-patterns)
7. [Error Handling](#error-handling)
8. [Console Access](#console-access)
9. [Example Communications](#example-communications)

---

## Overview

The XenAPI (XAPI) is the management API for XenServer and XCP-ng hypervisors. Our C++ rewrite communicates with the platform via **JSON-RPC 2.0** over HTTP/HTTPS, providing a comprehensive interface to manage virtual machines, hosts, storage, networking, and other hypervisor resources.

### Key Characteristics

- **Protocol**: JSON-RPC 2.0 over HTTP/HTTPS (port 443 for HTTPS, port 80 for HTTP)
- **Transport**: Synchronous request/response model
- **Authentication**: Session-based with opaque session tokens
- **Encoding**: UTF-8 JSON
- **Object References**: Opaque strings that uniquely identify resources

---

## JSON-RPC Protocol

XenAPI endpoints follow JSON-RPC 2.0 semantics. Every RPC call is a JSON object with `jsonrpc`, `method`, `params`, and `id`. Responses echo the `id` and contain either a `result` object or an `error` object.

### Request Format

```json
{
  "jsonrpc": "2.0",
  "method": "class.method",
  "params": [
    "session_id",
    "object_ref"
    // Additional parameters as needed
  ],
  "id": "42"
}
```

### Response Format

Successful responses include a `result` struct with `Status` and `Value` keys for parity with the legacy XML-RPC contract exposed by XAPI.

```json
{
  "jsonrpc": "2.0",
  "id": "42",
  "result": {
    "Status": "Success",
    "Value": {
      // Actual return value here
    }
  }
}
```

### Error Response Format

Failures return a JSON-RPC `error` object that wraps the familiar XAPI error payload inside its `data` member. The top-level `code` is always `1` (JSON-RPC application error) while `message` repeats the XAPI error string for human readability.

```json
{
  "jsonrpc": "2.0",
  "id": "42",
  "error": {
    "code": 1,
    "message": "XenAPI Failure",
    "data": {
      "Status": "Failure",
      "ErrorDescription": [
        "ERROR_CODE",
        "Additional info"
      ]
    }
  }
}
```

### JSON Types in XAPI

| JSON Type | XenAPI Usage | Example |
|-----------|--------------|---------|
| `string` | Text, UUIDs, OpaqueRefs | `"OpaqueRef:12345"` |
| `number` | Integers, memory sizes, doubles | `1073741824` |
| `boolean` | True/false flags | `true` |
| `array` | Lists of values | `["OpaqueRef:1", "OpaqueRef:2"]` |
| `object` | Key-value maps, records | `{"uuid": "...", "name_label": "VM1"}` |
| `null` | Explicitly unset optional values | `null` |

Binary payloads (for example, certificates) are base64-encoded strings. Timestamps are ISO 8601 strings (`"2025-10-09T12:00:00Z"`).

---

## Authentication Flow

XenAPI uses session-based authentication. Every API call (except login) requires a valid session ID string passed in the first position of the `params` array.

### 1. Login

**Method**: `session.login_with_password`

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "session.login_with_password",
  "params": [
    "root",
    "password123",
    {
      "version": "1.0",
      "originator": "xenadmin_qt"
    }
  ],
  "id": "login-1"
}
```

**Response**:
```json
{
  "jsonrpc": "2.0",
  "id": "login-1",
  "result": {
    "Status": "Success",
    "Value": "OpaqueRef:session-uuid"
  }
}
```

### 2. Subsequent Calls

Pass the received session ID as the first argument in `params` when invoking other methods.

```json
{
  "jsonrpc": "2.0",
  "method": "VM.get_record",
  "params": [
    "OpaqueRef:session-uuid",
    "OpaqueRef:vm-uuid"
  ],
  "id": "vm-record"
}
```

### 3. Logout

**Method**: `session.logout`

```json
{
  "jsonrpc": "2.0",
  "method": "session.logout",
  "params": [
    "OpaqueRef:session-uuid"
  ],
  "id": "logout-1"
}
```

---

## Opaque References

Opaque references (`OpaqueRef:*`) uniquely identify XAPI objects. They remain string values in JSON-RPC. When dealing with collections, expect arrays of strings; when fetching records, expect objects that map property names to JSON-compatible values.

- **Acquiring refs**: Methods such as `VM.get_all` return `["OpaqueRef:...", ...]`.
- **Using refs**: Pass refs back as positional parameters, e.g. `["OpaqueRef:session", "OpaqueRef:vm"]`.
- **Record structs**: `VM.get_record` returns an object with camelCase or snake_case keys mirroring the XAPI schema.

Cache refs carefully—the server expects them verbatim, including letter casing.

---

## Object Model

Objects (VM, SR, Host, Pool, etc.) map directly to JSON objects. Nested relationships (e.g. `VBDs`, `VIFs`) are represented as arrays of opaque refs. Maps such as `other_config` are plain JSON objects (`{ "key": "value" }`). Enumerations are simple strings (`"Running"`, `"Halted"`).

When deserializing into C++ structures, preserve:

- Any field that was a struct in XML-RPC becomes a JSON object.
- Numeric fields use 64-bit integers or doubles depending on the API contract.
- Date fields remain ISO 8601 strings; parse as needed.

---

## Common API Patterns

- **Get record**: `Class.get_record(session, opaqueRef)`
- **List refs**: `Class.get_all(session)`
- **Async operations**: Methods prefixed with `async_` respond immediately with a Task ref. Poll via `task.get_record`.
- **Key-value updates**: `Class.add_to_other_config(session, opaqueRef, key, value)` expects the key/value as additional elements in `params`.
- **Bulk fetches**: `Class.get_all_records` returns a JSON object keyed by refs, each value is a full record.

Example of `VM.add_to_other_config`:

```json
{
  "jsonrpc": "2.0",
  "method": "VM.add_to_other_config",
  "params": [
    "OpaqueRef:session-uuid",
    "OpaqueRef:vm-uuid",
    "backup",
    "weekly"
  ],
  "id": "vm-other-config"
}
```

---

## Error Handling

Handle both network-level errors and JSON-RPC/XAPI failures:

1. **Transport errors**: HTTP status codes ≥ 400 or malformed JSON. Reconnect or surface as connection faults.
2. **JSON-RPC errors**: Presence of the `error` object. Inspect `error.data.ErrorDescription` for the XAPI specifics.
3. **Async task failures**: Poll `task.get_record`; failed tasks contain `error_info` mirroring `ErrorDescription`.

Sample error payload:

```json
{
  "jsonrpc": "2.0",
  "id": "vm-start",
  "error": {
    "code": 1,
    "message": "XenAPI Failure",
    "data": {
      "Status": "Failure",
      "ErrorDescription": [
        "VM_BAD_POWER_STATE",
        "OpaqueRef:vm-uuid",
        "Running",
        "Halted"
      ]
    }
  }
}
```

Map `ErrorDescription[0]` to C++ enums or error codes as needed, and surface helpful diagnostics in the UI.

---

## Console Access

Console sessions are established by invoking `console.get_record` to retrieve connection metadata, then initiating an HTTP(S) or VNC session depending on the console `protocol`. The JSON response matches the XML schema but arrives as an object:

```json
{
  "jsonrpc": "2.0",
  "method": "console.get_record",
  "params": [
    "OpaqueRef:session-uuid",
    "OpaqueRef:console-uuid"
  ],
  "id": "console-record"
}
```

Successful `result.Value` contains:

```json
{
  "protocol": "rfb",
  "location": "https://host.example.tld/remote-console?ref=...",
  "VM": "OpaqueRef:vm-uuid",
  "other_config": {}
}
```

Use `location` together with session cookies when establishing the remote console tunnel.

---

## Example Communications

### Fetch host metrics

```json
{
  "jsonrpc": "2.0",
  "method": "host.get_metrics",
  "params": [
    "OpaqueRef:session-uuid",
    "OpaqueRef:host-uuid"
  ],
  "id": "host-metrics"
}
```

Response:

```json
{
  "jsonrpc": "2.0",
  "id": "host-metrics",
  "result": {
    "Status": "Success",
    "Value": "OpaqueRef:host-metrics-uuid"
  }
}
```

### Retrieve event batch

```json
{
  "jsonrpc": "2.0",
  "method": "event.from",
  "params": [
    "OpaqueRef:session-uuid",
    [
      "VM",
      "host",
      "pool"
    ],
    "timestamp-token"
  ],
  "id": "event-from"
}
```

Response (truncated):

```json
{
  "jsonrpc": "2.0",
  "id": "event-from",
  "result": {
    "Status": "Success",
    "Value": {
      "events": [
        {
          "operation": "mod",
          "ref": "OpaqueRef:vm-uuid",
          "snapshot": {
            "uuid": "...",
            "power_state": "Running"
          },
          "timestamp": "2025-10-09T12:00:00Z"
        }
      ],
      "from_token": "timestamp-token",
      "to_token": "timestamp-token+1"
    }
  }
}
```

This document mirrors the legacy XML-RPC guide located in `docs/xmlrpc/xapi.md`. Consult that version when cross-referencing legacy behavior or field nomenclature.

