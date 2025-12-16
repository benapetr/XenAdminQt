# XenAPI Function Reference — JSON-RPC Edition

This document provides a comprehensive list of XenAPI functions organized by object type. It mirrors the legacy XML-RPC reference preserved under `docs/xmlrpc/xapi_list.md`, but all examples and guidance are updated for JSON-RPC 2.0.

## Table of Contents

- [Session Management](#session-management)
- [VM (Virtual Machine)](#vm-virtual-machine)
- [Host](#host)
- [Pool](#pool)
- [SR (Storage Repository)](#sr-storage-repository)
- [VDI (Virtual Disk Image)](#vdi-virtual-disk-image)
- [VBD (Virtual Block Device)](#vbd-virtual-block-device)
- [Network](#network)
- [VIF (Virtual Network Interface)](#vif-virtual-network-interface)
- [Console](#console)
- [Task](#task)
- [Event](#event)
- [PBD (Physical Block Device)](#pbd-physical-block-device)
- [PIF (Physical Network Interface)](#pif-physical-network-interface)
- [Other Objects](#other-objects)

---

## Session Management

Authentication and session lifecycle management. The session ID must be the first positional element in every `params` array after login.

| Function | Purpose |
|----------|---------|
| `session.login_with_password` | Authenticate and create a new session |
| `session.logout` | End the current session |
| `session.get_all_subject_identifiers` | Get list of all user identifiers |
| `session.get_this_host` | Get the host this session is connected to |
| `session.get_this_user` | Get the user for this session |
| `session.change_password` | Change password for the session user |
| `session.slave_local_login_with_password` | Login to pool slave (deprecated) |

**Example**:
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
  "id": "login"
}
```

---

## VM (Virtual Machine)

Virtual machine management and operations.

### Query Functions

| Function | Purpose |
|----------|---------|
| `VM.get_all` | Get list of all VM OpaqueRefs |
| `VM.get_all_records` | Get all VMs with their complete records |
| `VM.get_by_uuid` | Find VM by UUID |
| `VM.get_by_name_label` | Find VMs by name |
| `VM.get_record` | Get complete VM record |
| `VM.get_uuid` | Get VM's UUID |
| `VM.get_name_label` | Get VM's display name |
| `VM.get_name_description` | Get VM's description |
| `VM.get_power_state` | Get current power state (Running, Halted, etc.) |
| `VM.get_is_a_template` | Check if VM is a template |
| `VM.get_resident_on` | Get host where VM is running |
| `VM.get_affinity` | Get preferred host for VM |
| `VM.get_memory_target` | Get target memory allocation |
| `VM.get_memory_static_max` | Get maximum static memory |
| `VM.get_memory_dynamic_max` | Get maximum dynamic memory |
| `VM.get_VCPUs_at_startup` | Get number of VCPUs |
| `VM.get_VCPUs_max` | Get maximum VCPUs allowed |
| `VM.get_consoles` | Get list of console OpaqueRefs |
| `VM.get_VIFs` | Get list of network interfaces |
| `VM.get_VBDs` | Get list of virtual block devices |
| `VM.get_snapshots` | Get list of snapshots |
| `VM.get_metrics` | Get VM metrics OpaqueRef |
| `VM.get_guest_metrics` | Get guest metrics OpaqueRef |

### Modification Functions

| Function | Purpose |
|----------|---------|
| `VM.set_name_label` | Set VM display name |
| `VM.set_name_description` | Set VM description |
| `VM.set_memory_static_max` | Set maximum static memory |
| `VM.set_memory_dynamic_max` | Set maximum dynamic memory |
| `VM.set_VCPUs_at_startup` | Set number of VCPUs at startup |
| `VM.set_VCPUs_max` | Set maximum VCPUs |
| `VM.set_affinity` | Set preferred host |
| `VM.add_to_other_config` | Add key-value to other_config |
| `VM.remove_from_other_config` | Remove key from other_config |
| `VM.add_tags` | Add tag to VM |
| `VM.remove_tags` | Remove tag from VM |

### Power State Operations

| Function | Purpose |
|----------|---------|
| `VM.start` | Start a halted VM |
| `VM.start_on` | Start VM on specific host |
| `VM.pause` | Pause a running VM |
| `VM.unpause` | Resume a paused VM |
| `VM.clean_shutdown` | Gracefully shutdown VM (requires guest tools) |
| `VM.shutdown` | Shutdown VM (tries clean, then hard) |
| `VM.clean_reboot` | Gracefully reboot VM |
| `VM.hard_shutdown` | Force power off VM |
| `VM.hard_reboot` | Force restart VM |
| `VM.suspend` | Suspend VM to disk |
| `VM.resume` | Resume a suspended VM |
| `VM.resume_on` | Resume VM on specific host |

### Snapshot & Clone Operations

| Function | Purpose |
|----------|---------|
| `VM.snapshot` | Create snapshot of VM |
| `VM.snapshot_with_quiesce` | Create snapshot with filesystem quiesce |
| `VM.clone` | Create full copy of VM |
| `VM.copy` | Copy VM to different SR |
| `VM.revert` | Revert VM to snapshot state |
| `VM.checkpoint` | Create checkpoint (snapshot with memory) |

### Advanced Operations

| Function | Purpose |
|----------|---------|
| `VM.create` | Create new VM from record |
| `VM.destroy` | Permanently delete VM |
| `VM.provision` | Provision VM disks from template |
| `VM.import` | Import VM from XVA file |
| `VM.export` | Export VM to XVA file |
| `VM.migrate` | Live migrate VM to another host |
| `VM.pool_migrate` | Migrate VM to host in pool |
| `VM.send_sysrq` | Send system request key to VM |
| `VM.send_trigger` | Send power trigger to VM |
| `VM.assert_can_boot_here` | Check if VM can boot on host |
| `VM.compute_memory_overhead` | Calculate memory overhead |
| `VM.maximise_memory` | Optimize memory allocation |

### Async Versions

Most operations have `async_*` versions that return a Task OpaqueRef instead of blocking: `VM.async_start`, `VM.async_shutdown`, `VM.async_clone`, etc.

---

## Host

Physical hypervisor host management.

### Query Functions

| Function | Purpose |
|----------|---------|
| `host.get_all` | Get all host OpaqueRefs |
| `host.get_all_records` | Get all hosts with records |
| `host.get_by_uuid` | Find host by UUID |
| `host.get_by_name_label` | Find host by name |
| `host.get_record` | Get complete host record |
| `host.get_name_label` | Get host name |
| `host.get_hostname` | Get network hostname |
| `host.get_address` | Get IP address |
| `host.get_enabled` | Check if host is enabled |
| `host.get_resident_VMs` | Get VMs running on host |
| `host.get_memory_total` | Get total physical memory |
| `host.get_memory_free` | Get free memory |
| `host.get_cpu_info` | Get CPU information |
| `host.get_software_version` | Get XenServer version |
| `host.get_capabilities` | Get host capabilities |

### Modification Functions

| Function | Purpose |
|----------|---------|
| `host.set_name_label` | Set host display name |
| `host.set_name_description` | Set host description |
| `host.add_to_other_config` | Add to other_config |
| `host.set_hostname` | Set network hostname |
| `host.set_address` | Set IP address |

### Power & Maintenance Operations

| Function | Purpose |
|----------|---------|
| `host.disable` | Disable host (prevent new VMs) |
| `host.enable` | Enable host |
| `host.shutdown` | Shutdown host |
| `host.reboot` | Reboot host |
| `host.evacuate` | Move all VMs off host |
| `host.power_on` | Power on host (Wake-on-LAN) |

### Management Operations

| Function | Purpose |
|----------|---------|
| `host.apply_edition` | Apply license edition |
| `host.restart_agent` | Restart XenServer agent |
| `host.emergency_ha_disable` | Disable HA in emergency |
| `host.assert_can_evacuate` | Check if host can be evacuated |
| `host.get_vms_which_prevent_evacuation` | Get VMs blocking evacuation |
| `host.get_system_status_capabilities` | Get status-report capabilities |

---

## Pool

Resource pool management (cluster of hosts).

### Query Functions

| Function | Purpose |
|----------|---------|
| `pool.get_all` | Get all pool OpaqueRefs |
| `pool.get_by_uuid` | Find pool by UUID |
| `pool.get_record` | Get pool record |
| `pool.get_name_label` | Get pool name |
| `pool.get_master` | Get pool master host |
| `pool.get_default_SR` | Get default storage repository |
| `pool.get_suspend_image_SR` | Get SR for suspend images |
| `pool.get_crash_dump_SR` | Get SR for crash dumps |

### Modification Functions

| Function | Purpose |
|----------|---------|
| `pool.set_name_label` | Set pool name |
| `pool.set_name_description` | Set pool description |
| `pool.set_default_SR` | Set default SR |
| `pool.set_suspend_image_SR` | Set suspend image SR |
| `pool.set_crash_dump_SR` | Set crash dump SR |

### Pool Operations

| Function | Purpose |
|----------|---------|
| `pool.join` | Join host to pool |
| `pool.eject` | Remove host from pool |
| `pool.emergency_transition_to_master` | Promote host to master |
| `pool.emergency_reset_master` | Reset master designation |
| `pool.recover_slaves` | Recover slave connections |
| `pool.designate_new_master` | Designate new pool master |
| `pool.ha_enable` | Enable High Availability |
| `pool.ha_disable` | Disable High Availability |
| `pool.sync_database` | Synchronize database |
| `pool.certificate_install` | Install SSL certificate |
| `pool.certificate_uninstall` | Remove SSL certificate |

---

## SR (Storage Repository)

Storage repository management.

### Query Functions

| Function | Purpose |
|----------|---------|
| `SR.get_all` | Get all SR OpaqueRefs |
| `SR.get_all_records` | Get all SRs with records |
| `SR.get_by_uuid` | Find SR by UUID |
| `SR.get_by_name_label` | Find SR by name |
| `SR.get_record` | Get SR record |
| `SR.get_name_label` | Get SR name |
| `SR.get_type` | Get SR type (nfs, lvmoiscsi, etc.) |
| `SR.get_content_type` | Get content type |
| `SR.get_physical_size` | Get total capacity |
| `SR.get_physical_utilisation` | Get used space |
| `SR.get_VDIs` | Get virtual disks in SR |
| `SR.get_PBDs` | Get physical connections |
| `SR.get_shared` | Check if SR is shared |

### Modification Functions

| Function | Purpose |
|----------|---------|
| `SR.set_name_label` | Set SR name |
| `SR.set_name_description` | Set SR description |
| `SR.set_physical_size` | Set physical size |
| `SR.set_shared` | Set shared flag |

### SR Operations

| Function | Purpose |
|----------|---------|
| `SR.create` | Create new storage repository |
| `SR.destroy` | Destroy SR |
| `SR.forget` | Forget SR (remove from database) |
| `SR.scan` | Scan SR for VDIs |
| `SR.probe` | Probe for storage devices |
| `SR.update` | Update SR metadata |
| `SR.enable_database_replication` | Enable DB replication |
| `SR.disable_database_replication` | Disable DB replication |

---

## VDI (Virtual Disk Image)

Virtual disk management.

### Query Functions

| Function | Purpose |
|----------|---------|
| `VDI.get_all` | Get all VDI OpaqueRefs |
| `VDI.get_all_records` | Get all VDIs with records |
| `VDI.get_by_uuid` | Find VDI by UUID |
| `VDI.get_by_name_label` | Find VDI by name |
| `VDI.get_record` | Get VDI record |
| `VDI.get_name_label` | Get VDI name |
| `VDI.get_virtual_size` | Get virtual disk size |
| `VDI.get_physical_utilisation` | Get actual space used |
| `VDI.get_type` | Get VDI type (System, User, etc.) |
| `VDI.get_SR` | Get containing SR |
| `VDI.get_VBDs` | Get VBDs using this VDI |
| `VDI.get_read_only` | Check if read-only |
| `VDI.get_sharable` | Check if sharable |

### Modification Functions

| Function | Purpose |
|----------|---------|
| `VDI.set_name_label` | Set VDI name |
| `VDI.set_name_description` | Set VDI description |
| `VDI.set_virtual_size` | Set virtual size (resize) |
| `VDI.set_read_only` | Set read-only flag |
| `VDI.set_sharable` | Set sharable flag |

### VDI Operations

| Function | Purpose |
|----------|---------|
| `VDI.create` | Create new virtual disk |
| `VDI.destroy` | Delete virtual disk |
| `VDI.snapshot` | Create snapshot of VDI |
| `VDI.clone` | Clone VDI |
| `VDI.copy` | Copy VDI to another SR |
| `VDI.resize` | Resize virtual disk |
| `VDI.resize_online` | Resize disk while VM running |
| `VDI.introduce` | Introduce existing VDI |
| `VDI.forget` | Forget VDI |
| `VDI.update` | Update VDI metadata |

---

## VBD (Virtual Block Device)

Virtual disk attachment to VMs.

### Query Functions

| Function | Purpose |
|----------|---------|
| `VBD.get_all` | Get all VBD OpaqueRefs |
| `VBD.get_record` | Get VBD record |
| `VBD.get_VM` | Get VM this VBD belongs to |
| `VBD.get_VDI` | Get VDI this VBD uses |
| `VBD.get_device` | Get device name (e.g., "xvda") |
| `VBD.get_mode` | Get mode (RO/RW) |
| `VBD.get_type` | Get type (Disk/CD) |
| `VBD.get_bootable` | Check if bootable |
| `VBD.get_currently_attached` | Check if currently attached |

### Modification Functions

| Function | Purpose |
|----------|---------|
| `VBD.set_mode` | Set read/write mode |
| `VBD.set_type` | Set type (Disk/CD) |
| `VBD.set_bootable` | Set bootable flag |

### VBD Operations

| Function | Purpose |
|----------|---------|
| `VBD.create` | Create VBD (attach disk to VM) |
| `VBD.destroy` | Destroy VBD (detach disk) |
| `VBD.plug` | Hot-plug VBD |
| `VBD.unplug` | Hot-unplug VBD |
| `VBD.insert` | Insert CD/DVD |
| `VBD.eject` | Eject CD/DVD |

---

## Network

Virtual network management.

### Query Functions

| Function | Purpose |
|----------|---------|
| `network.get_all` | Get all network OpaqueRefs |
| `network.get_all_records` | Get all networks with records |
| `network.get_by_uuid` | Find network by UUID |
| `network.get_by_name_label` | Find network by name |
| `network.get_record` | Get network record |
| `network.get_name_label` | Get network name |
| `network.get_bridge` | Get bridge name |
| `network.get_VIFs` | Get VIFs on this network |
| `network.get_PIFs` | Get PIFs on this network |

### Modification Functions

| Function | Purpose |
|----------|---------|
| `network.set_name_label` | Set network name |
| `network.set_name_description` | Set network description |
| `network.add_to_other_config` | Add to other_config |

### Network Operations

| Function | Purpose |
|----------|---------|
| `network.create` | Create new network |
| `network.destroy` | Destroy network |
| `network.attach` | Attach network to all hosts |
| `network.pool_introduce` | Introduce network to pool |

---

## VIF (Virtual Network Interface)

Virtual network interface management.

### Query Functions

| Function | Purpose |
|----------|---------|
| `VIF.get_all` | Get all VIF OpaqueRefs |
| `VIF.get_record` | Get VIF record |
| `VIF.get_VM` | Get VM this VIF belongs to |
| `VIF.get_network` | Get network this VIF is on |
| `VIF.get_device` | Get device number |
| `VIF.get_MAC` | Get MAC address |
| `VIF.get_currently_attached` | Check if currently attached |

### Modification Functions

| Function | Purpose |
|----------|---------|
| `VIF.set_MAC` | Set MAC address |
| `VIF.set_locking_mode` | Set IP locking mode |

### VIF Operations

| Function | Purpose |
|----------|---------|
| `VIF.create` | Create VIF (attach network interface) |
| `VIF.destroy` | Destroy VIF |
| `VIF.plug` | Hot-plug VIF |
| `VIF.unplug` | Hot-unplug VIF |

---

## Console

VM console access.

### Query Functions

| Function | Purpose |
|----------|---------|
| `console.get_all` | Get all console OpaqueRefs |
| `console.get_record` | Get console record |
| `console.get_protocol` | Get protocol (rfb/RDP) |
| `console.get_location` | Get console URI |
| `console.get_VM` | Get VM for this console |

### Console Operations

| Function | Purpose |
|----------|---------|
| `console.create` | Create console |
| `console.destroy` | Destroy console |

**Note**: After retrieving console location, use HTTP CONNECT tunneling to establish the VNC/RDP session (see `docs/xapi.md`).

---

## Task

Asynchronous task management.

### Query Functions

| Function | Purpose |
|----------|---------|
| `task.get_all` | Get all task OpaqueRefs |
| `task.get_record` | Get task record |
| `task.get_name_label` | Get task name |
| `task.get_status` | Get status (pending/success/failure) |
| `task.get_progress` | Get progress (0.0 to 1.0) |
| `task.get_result` | Get task result |
| `task.get_error_info` | Get error information |

### Task Operations

| Function | Purpose |
|----------|---------|
| `task.cancel` | Cancel running task |
| `task.destroy` | Destroy task record |

---

## Event

Event monitoring system.

### Event Functions

| Function | Purpose |
|----------|---------|
| `event.register` | Register for event notifications |
| `event.unregister` | Unregister from events |
| `event.next` | Get next batch of events |
| `event.from` | Get events from specific token |
| `event.inject` | Inject custom event |

**Usage Pattern**:
1. Call `event.register` with class names to monitor
2. Receive token
3. Loop calling `event.from` with token
4. Process events
5. Update token
6. Call `event.unregister` when done

---

## PBD (Physical Block Device)

Physical SR connections to hosts.

### Query Functions

| Function | Purpose |
|----------|---------|
| `PBD.get_all` | Get all PBD OpaqueRefs |
| `PBD.get_record` | Get PBD record |
| `PBD.get_host` | Get host |
| `PBD.get_SR` | Get SR |
| `PBD.get_currently_attached` | Check if attached |

### PBD Operations

| Function | Purpose |
|----------|---------|
| `PBD.create` | Create PBD |
| `PBD.destroy` | Destroy PBD |
| `PBD.plug` | Attach SR to host |
| `PBD.unplug` | Detach SR from host |

---

## PIF (Physical Network Interface)

Physical network interface management.

### Query Functions

| Function | Purpose |
|----------|---------|
| `PIF.get_all` | Get all PIF OpaqueRefs |
| `PIF.get_record` | Get PIF record |
| `PIF.get_device` | Get device name (eth0, etc.) |
| `PIF.get_network` | Get network |
| `PIF.get_host` | Get host |
| `PIF.get_MAC` | Get MAC address |
| `PIF.get_IP` | Get IP address |
| `PIF.get_currently_attached` | Check if attached |

### PIF Operations

| Function | Purpose |
|----------|---------|
| `PIF.create_VLAN` | Create VLAN interface |
| `PIF.destroy` | Destroy PIF |
| `PIF.reconfigure_ip` | Reconfigure IP settings |
| `PIF.plug` | Bring up interface |
| `PIF.unplug` | Bring down interface |

---

## Other Objects

### VM_guest_metrics

Guest-reported metrics (requires XenServer Tools).

| Function | Purpose |
|----------|---------|
| `VM_guest_metrics.get_record` | Get guest metrics |
| `VM_guest_metrics.get_os_version` | Get OS information |
| `VM_guest_metrics.get_memory` | Get memory usage |
| `VM_guest_metrics.get_disks` | Get disk usage |
| `VM_guest_metrics.get_networks` | Get network info |

### VM_metrics

Hypervisor-reported VM metrics.

| Function | Purpose |
|----------|---------|
| `VM_metrics.get_record` | Get VM metrics |
| `VM_metrics.get_memory_actual` | Get actual memory |
| `VM_metrics.get_VCPUs_number` | Get VCPU count |
| `VM_metrics.get_VCPUs_utilisation` | Get CPU usage |
| `VM_metrics.get_start_time` | Get VM start time |

### Host_metrics

Host performance metrics.

| Function | Purpose |
|----------|---------|
| `host_metrics.get_record` | Get host metrics |
| `host_metrics.get_memory_total` | Get total memory |
| `host_metrics.get_memory_free` | Get free memory |

### VMPP (VM Protection Policy)

Backup and snapshot scheduling.

| Function | Purpose |
|----------|---------|
| `VMPP.create` | Create protection policy |
| `VMPP.destroy` | Destroy policy |
| `VMPP.protect_now` | Run backup immediately |

### VMSS (VM Snapshot Schedule)

Snapshot scheduling.

| Function | Purpose |
|----------|---------|
| `VMSS.create` | Create snapshot schedule |
| `VMSS.destroy` | Destroy schedule |
| `VMSS.snapshot_now` | Create snapshot now |

### Bond

Network bonding (NIC teaming).

| Function | Purpose |
|----------|---------|
| `Bond.create` | Create network bond |
| `Bond.destroy` | Destroy bond |
| `Bond.get_record` | Get bond record |

### VLAN

Virtual LAN configuration.

| Function | Purpose |
|----------|---------|
| `VLAN.create` | Create VLAN |
| `VLAN.destroy` | Destroy VLAN |
| `VLAN.get_record` | Get VLAN record |

### Tunnel

Network tunnel management.

| Function | Purpose |
|----------|---------|
| `tunnel.create` | Create tunnel |
| `tunnel.destroy` | Destroy tunnel |

### Role

RBAC (Role-Based Access Control).

| Function | Purpose |
|----------|---------|
| `role.get_all` | Get all roles |
| `role.get_by_name_label` | Find role by name |
| `role.get_permissions` | Get role permissions |

### Subject

User and group management.

| Function | Purpose |
|----------|---------|
| `subject.create` | Create user/group |
| `subject.destroy` | Delete user/group |
| `subject.add_to_roles` | Assign role |
| `subject.remove_from_roles` | Remove role |

### Message

System messages and alerts.

| Function | Purpose |
|----------|---------|
| `message.get_all` | Get all messages |
| `message.get_record` | Get message details |
| `message.destroy` | Dismiss message |

---

## Common Patterns

### Pattern: Get All Objects

```json
{
  "jsonrpc": "2.0",
  "method": "CLASS.get_all",
  "params": [
    "SESSION_ID"
  ],
  "id": "request-id"
}
```

### Pattern: Get Object Record

```json
{
  "jsonrpc": "2.0",
  "method": "CLASS.get_record",
  "params": [
    "SESSION_ID",
    "OBJECT_REF"
  ],
  "id": "request-id"
}
```

### Pattern: Get Specific Field

```json
{
  "jsonrpc": "2.0",
  "method": "CLASS.get_FIELD",
  "params": [
    "SESSION_ID",
    "OBJECT_REF"
  ],
  "id": "request-id"
}
```

### Pattern: Set Field Value

```json
{
  "jsonrpc": "2.0",
  "method": "CLASS.set_FIELD",
  "params": [
    "SESSION_ID",
    "OBJECT_REF",
    "NEW_VALUE"
  ],
  "id": "request-id"
}
```

### Pattern: Perform Action

```json
{
  "jsonrpc": "2.0",
  "method": "CLASS.ACTION",
  "params": [
    "SESSION_ID",
    "OBJECT_REF",
    "EXTRA_PARAM_1",
    "EXTRA_PARAM_2"
  ],
  "id": "request-id"
}
```

### Pattern: Async Operation

```json
{
  "jsonrpc": "2.0",
  "method": "CLASS.async_ACTION",
  "params": [
    "SESSION_ID",
    "OBJECT_REF"
  ],
  "id": "request-id"
}
```

Returns a Task OpaqueRef that can be monitored for completion via `task.get_record`.

---

## Notes

### Async Operations

Most long-running operations have `async_*` variants that:
- Return immediately with a Task OpaqueRef
- Allow monitoring progress via `task.get_progress`
- Support cancellation via `task.cancel`
- Provide final status via `task.get_result`

### Error Handling

JSON-RPC responses include either a `result` object with `Status: "Success"` or an `error` object wrapping the XAPI failure payload. Inspect `error.data.ErrorDescription` (array) for detailed failure codes and context.
