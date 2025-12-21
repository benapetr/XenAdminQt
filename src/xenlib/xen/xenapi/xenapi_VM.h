/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XENAPI_VM_H
#define XENAPI_VM_H

#include "../../xenlib_global.h"
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

//! TODO rewrite all docs to Doxygen format from that XML format

// Forward declarations
class XenSession;

namespace XenAPI
{

    /**
     * @brief XenAPI VM class - Static methods for VM operations
     *
     * Qt equivalent of C# XenAPI.VM class. All methods are static and mirror
     * the C# XenServer API bindings exactly.
     *
     * Matches: xenadmin/XenModel/XenAPI/VM.cs
     */
    class XENLIB_EXPORT VM
    {
        public:
            // VM lifecycle operations

            /**
             * @brief Start the specified VM. Only valid when VM is Halted.
             *
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param start_paused Instantiate VM in paused state if true
             * @param force Attempt to force the VM to start; if false the VM may fail pre-boot checks
             */
            static void start(XenSession* session, const QString& vm, bool start_paused, bool force);

            /**
             * @brief Start the specified VM asynchronously; returns a task ref.
             *
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param start_paused Instantiate VM in paused state if true
             * @param force Attempt to force the VM to start
             * @return Task ref for async operation
             */
            static QString async_start(XenSession* session, const QString& vm, bool start_paused, bool force);

            /**
             * @brief Start the specified VM on a particular host. Only valid when VM is Halted.
             *
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param host The Host on which to start the VM
             * @param start_paused Instantiate VM in paused state if true
             * @param force Attempt to force the VM to start
             */
            static void start_on(XenSession* session, const QString& vm, const QString& host, bool start_paused, bool force);

            /**
             * @brief Start the specified VM on a particular host asynchronously; returns a task ref.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param host The Host on which to start the VM
             * @param start_paused Instantiate VM in paused state if true
             * @param force Attempt to force the VM to start
             * @return Task ref for async operation
             */
            static QString async_start_on(XenSession* session, const QString& vm, const QString& host, bool start_paused, bool force);

            /**
             * @brief Resume the specified VM. Only valid when VM is Suspended.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param start_paused Resume VM in paused state if true
             * @param force Attempt to force the VM to resume
             */
            static void resume(XenSession* session, const QString& vm, bool start_paused, bool force);

            /**
             * @brief Resume the specified VM asynchronously; returns a task ref.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param start_paused Resume VM in paused state if true
             * @param force Attempt to force the VM to resume
             * @return Task ref for async operation
             */
            static QString async_resume(XenSession* session, const QString& vm, bool start_paused, bool force);

            /**
             * @brief Resume the specified VM on a particular host. Only valid when VM is Suspended.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param host The Host on which to resume the VM
             * @param start_paused Resume VM in paused state if true
             * @param force Attempt to force the VM to resume
             */
            static void resume_on(XenSession* session, const QString& vm, const QString& host, bool start_paused, bool force);

            /**
             * @brief Resume the specified VM on a particular host asynchronously; returns a task ref.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param host The Host on which to resume the VM
             * @param start_paused Resume VM in paused state if true
             * @param force Attempt to force the VM to resume
             * @return Task ref for async operation
             */
            static QString async_resume_on(XenSession* session, const QString& vm, const QString& host, bool start_paused, bool force);

            /**
             * @brief Attempt a clean shutdown of a VM; fall back to hard shutdown on failure.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             */
            static void clean_shutdown(XenSession* session, const QString& vm);

            /**
             * @brief Asynchronous clean shutdown; returns a task ref.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @return Task ref for async operation
             */
            static QString async_clean_shutdown(XenSession* session, const QString& vm);

            /**
             * @brief Perform a hard shutdown (stop without clean shutdown).
             * @param session The session
             * @param vm The opaque_ref of the given VM
             */
            static void hard_shutdown(XenSession* session, const QString& vm);

            /**
             * @brief Asynchronous hard shutdown; returns a task ref.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @return Task ref for async operation
             */
            static QString async_hard_shutdown(XenSession* session, const QString& vm);

            /**
             * @brief Suspend the specified VM to disk. Only valid when VM is Running.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             */
            static void suspend(XenSession* session, const QString& vm);

            /**
             * @brief Asynchronous suspend; returns a task ref.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @return Task ref for async operation
             */
            static QString async_suspend(XenSession* session, const QString& vm);

            /**
             * @brief Attempt a clean reboot of a VM; fall back to hard reboot on failure.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             */
            static void clean_reboot(XenSession* session, const QString& vm);

            /**
             * @brief Asynchronous clean reboot; returns a task ref.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @return Task ref for async operation
             */
            static QString async_clean_reboot(XenSession* session, const QString& vm);

            /**
             * @brief Hard reboot the VM (immediate stop and restart).
             * @param session The session
             * @param vm The opaque_ref of the given VM
             */
            static void hard_reboot(XenSession* session, const QString& vm);

            /**
             * @brief Asynchronous hard reboot; returns a task ref.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @return Task ref for async operation
             */
            static QString async_hard_reboot(XenSession* session, const QString& vm);

            /**
             * @brief Pause the specified VM. This can only be called when the specified VM is in the Running state.
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             */
            static void pause(XenSession* session, const QString& vm);

            /**
             * @brief Asynchronous pause; returns a task ref.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @return Task ref for async operation
             */
            static QString async_pause(XenSession* session, const QString& vm);

            /**
             * @brief Resume the specified VM. This can only be called when the specified VM is in the Paused state.
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             */
            static void unpause(XenSession* session, const QString& vm);

            /**
             * @brief Asynchronous unpause; returns a task ref.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @return Task ref for async operation
             */
            static QString async_unpause(XenSession* session, const QString& vm);

            /**
             * @brief Assert whether a VM can boot on this host.
             * First published in XenServer 6.1.
             * @param session The session
             * @param self The opaque_ref of the given vm
             * @param host The host on which we want to assert the VM can boot
             */
            static void assert_can_boot_here(XenSession* session, const QString& self, const QString& host);

            /**
             * @brief Assert whether all SRs required to recover this VM are available.
             * First published in XenServer 5.0.
             * @param session The session
             * @param self The opaque_ref of the given vm
             * @param session_to The session to which we want to recover the VM.
             */
            static void assert_can_migrate(XenSession* session, const QString& self, const QString& session_to);

            /**
             * @brief Assert whether the VM is agile (i.e. can be migrated without downtime).
             * Used for HA protection checks.
             * First published in XenServer 5.0.
             * @param session The session
             * @param self The opaque_ref of the given vm
             */
            static void assert_agile(XenSession* session, const QString& self);

            /**
             * @brief Get the list of allowed VBD device numbers
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @return List of allowed device numbers as QVariant (QStringList)
             */
            static QVariant get_allowed_VBD_devices(XenSession* session, const QString& vm);
            static QVariant get_allowed_VIF_devices(XenSession* session, const QString& vm);

            /**
             * @brief Get the full record for a VM
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @return VM record as QVariantMap
             */
            static QVariantMap get_record(XenSession* session, const QString& vm);

            /**
             * @brief Get all VMs and their records
             * First published in XenServer 4.0.
             * @param session The session
             * @return Map of VM refs to VM records
             */
            static QVariantMap get_all_records(XenSession* session);

            /**
             * @brief Set the suspend VDI for a suspended VM
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param value The new value for suspend_VDI
             */
            static void set_suspend_VDI(XenSession* session, const QString& vm, const QString& value);

            /**
             * @brief Migrate a VM to another Host (async)
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param host The target host
             * @param options Extra configuration operations (live migration, etc.)
             * @return Task ref for async operation
             */
            static QString async_pool_migrate(XenSession* session, const QString& vm, const QString& host, const QVariantMap& options);

            /**
             * @brief Clone a VM (async)
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param new_name The name of the cloned VM
             * @return Task ref for async operation
             */
            static QString async_clone(XenSession* session, const QString& vm, const QString& new_name);

            /**
             * @brief Clone a VM (sync)
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param new_name The name of the cloned VM
             * @return VM ref of the cloned VM
             */
            static QString clone(XenSession* session, const QString& vm, const QString& new_name);

            /**
             * @brief Copy a VM to an SR (async)
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param new_name The name of the copied VM
             * @param sr The SR to copy the VM to
             * @return Task ref for async operation
             */
            static QString async_copy(XenSession* session, const QString& vm, const QString& new_name, const QString& sr);

            /**
             * @brief Provision a VM (async)
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @return Task ref for async operation
             */
            static QString async_provision(XenSession* session, const QString& vm);

            /**
             * @brief Destroy a VM
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             */
            static void destroy(XenSession* session, const QString& vm);

            /**
             * @brief Set the is_a_template field
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param value New value for is_a_template
             */
            static void set_is_a_template(XenSession* session, const QString& vm, bool value);

            /**
             * @brief Set the name_label field
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param value New value for name_label
             */
            static void set_name_label(XenSession* session, const QString& vm, const QString& value);

            /**
             * @brief Set the name_description field
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param value New value for name_description
             */
            static void set_name_description(XenSession* session, const QString& vm, const QString& value);

            /**
             * @brief Set the tags field
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param value New value for tags
             */
            static void set_tags(XenSession* session, const QString& vm, const QStringList& value);

            // Snapshot operations

            /**
             * @brief Snapshot the specified VM asynchronously, creating a new VM record.
             *
             * Snapshot exploits SR capabilities (e.g., Copy-on-Write).
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param new_name The name of the snapshotted VM
             * @return Task ref for async operation (ref of the newly created VM)
             */
            static QString async_snapshot(XenSession* session, const QString& vm, const QString& new_name);

            /**
             * @brief Snapshot the VM with quiesce asynchronously; returns new VM ref.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param new_name The name of the snapshotted VM
             * @return Task ref for async operation (ref of the newly created VM)
             */
            static QString async_snapshot_with_quiesce(XenSession* session, const QString& vm, const QString& new_name);

            /**
             * @brief Checkpoint the specified VM asynchronously (includes memory image).
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param new_name The name of the checkpointed VM
             * @return Task ref for async operation (ref of the newly created VM)
             */
            static QString async_checkpoint(XenSession* session, const QString& vm, const QString& new_name);

            /**
             * @brief Revert the specified VM to a previous snapshot asynchronously.
             * @param session The session
             * @param snapshot The opaque_ref of the snapshot
             * @return Task ref for async operation
             */
            static QString async_revert(XenSession* session, const QString& snapshot);

            // Memory configuration

            /**
             * @brief Set the memory limits of the VM.
             *
             * First published in XenServer 4.0.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param static_min New value for memory_static_min (bytes)
             * @param static_max New value for memory_static_max (bytes)
             * @param dynamic_min New value for memory_dynamic_min (bytes)
             * @param dynamic_max New value for memory_dynamic_max (bytes)
             */
            static void set_memory_limits(XenSession* session, const QString& vm,
                                          qint64 static_min, qint64 static_max,
                                          qint64 dynamic_min, qint64 dynamic_max);

            /**
             * @brief Set the dynamic memory range (for running VMs).
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param dynamic_min New value for memory_dynamic_min (bytes)
             * @param dynamic_max New value for memory_dynamic_max (bytes)
             */
            static void set_memory_dynamic_range(XenSession* session, const QString& vm,
                                                 qint64 dynamic_min, qint64 dynamic_max);

            // VCPU configuration

            /**
             * @brief Set the maximum number of VCPUs for a halted VM.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param value New value for VCPUs_max
             */
            static void set_VCPUs_max(XenSession* session, const QString& vm, qint64 value);

            /**
             * @brief Set the number of VCPUs at startup for a halted VM.
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param value New value for VCPUs_at_startup
             */
            static void set_VCPUs_at_startup(XenSession* session, const QString& vm, qint64 value);

            /**
             * @brief Set the number of VCPUs for a running VM (hotplug).
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param nvcpu The number of VCPUs
             */
            static void set_VCPUs_number_live(XenSession* session, const QString& vm, qint64 nvcpu);

            /**
             * @brief Migrate VM asynchronously (cross-pool)
             * @param session Active XenSession
             * @param vm VM opaque reference
             * @param dest Destination host/session data from Host.migrate_receive
             * @param live Live migration flag
             * @param vdi_map VDI to SR mapping
             * @param vif_map VIF to Network mapping
             * @param options Migration options (e.g., {"copy": "true"} for copy operation)
             * @return Task reference
             */
            static QString async_migrate_send(XenSession* session, const QString& vm,
                                              const QVariantMap& dest, bool live,
                                              const QVariantMap& vdi_map, const QVariantMap& vif_map,
                                              const QVariantMap& options);

            /**
             * @brief Set HA restart priority for VM
             * @param session Active XenSession
             * @param vm VM opaque reference
             * @param value Priority: "" (empty = do not restart), "restart", "best-effort"
             *
             * Sets the VM's restart priority when using HA. Common values:
             * - "" (empty) = Do not restart automatically
             * - "restart" = Always restart
             * - "best-effort" = Restart if possible
             */
            static void set_ha_restart_priority(XenSession* session, const QString& vm, const QString& value);

            /**
             * @brief Set VM start order for HA
             * @param session Active XenSession
             * @param vm VM opaque reference
             * @param value Start order (0-n, lower starts first)
             *
             * Sets the order in which VMs are started during HA recovery.
             * VMs with lower order values start first.
             */
            static void set_order(XenSession* session, const QString& vm, qint64 value);

            /**
             * @brief Set VM start delay for HA
             * @param session Active XenSession
             * @param vm VM opaque reference
             * @param value Delay in seconds before starting next VM
             *
             * Sets the delay between starting this VM and the next VM in the HA sequence.
             */
            static void set_start_delay(XenSession* session, const QString& vm, qint64 value);

            /**
             * @brief Set HVM boot policy (e.g., "BIOS order")
             * @param session Active XenSession
             * @param vm VM opaque reference
             * @param value Boot policy string (e.g., "BIOS order", "")
             *
             * First published in XenServer 4.0.
             * Deprecated in XenServer 7.5.
             */
            static void set_HVM_boot_policy(XenSession* session, const QString& vm, const QString& value);

            /**
             * @brief Set HVM boot parameters (e.g., boot order)
             * @param session Active XenSession
             * @param vm VM opaque reference
             * @param bootParams Map of boot parameters (e.g., {"order": "DN"} for DVD then Network)
             *
             * Boot order examples:
             * - "C" = hard disk
             * - "D" = DVD/CD-ROM
             * - "N" = network PXE boot
             * - "DN" = DVD first, then network
             *
             * First published in XenServer 4.0.
             */
            static void set_HVM_boot_params(XenSession* session, const QString& vm, const QVariantMap& bootParams);

            /**
             * @brief Get HVM boot policy
             * @param session Active XenSession
             * @param vm VM opaque reference
             * @return Boot policy string
             */
            static QString get_HVM_boot_policy(XenSession* session, const QString& vm);

            /**
             * @brief Get HVM boot parameters
             * @param session Active XenSession
             * @param vm VM opaque reference
             * @return Map of boot parameters
             */
            static QVariantMap get_HVM_boot_params(XenSession* session, const QString& vm);

            /**
             * @brief Set PV args
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param value The PV args string
             *
             * Matches C# VM.set_PV_args()
             */
            static void set_PV_args(XenSession* session, const QString& vm, const QString& value);

            /**
             * @brief Set other_config
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param otherConfig Map of other_config values
             *
             * Matches C# VM.set_other_config()
             */
            static void set_other_config(XenSession* session, const QString& vm, const QVariantMap& otherConfig);

            /**
             * @brief Set platform
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param platform Platform map
             *
             * Matches C# VM.set_platform()
             */
            static void set_platform(XenSession* session, const QString& vm, const QVariantMap& platform);

            /**
             * @brief Set affinity
             * @param session The session
             * @param vm The opaque_ref of the given VM
             * @param host The host ref or "OpaqueRef:NULL"
             *
             * Matches C# VM.set_affinity()
             */
            static void set_affinity(XenSession* session, const QString& vm, const QString& host);

            // TODO: Add more VM methods as needed (pause, unpause, reboot, etc.)
            // See xenadmin/XenModel/XenAPI/VM.cs for complete list

        private:
            VM() = delete; // Static class, no instances
            ~VM() = delete;
    };

} // namespace XenAPI

#endif // XENAPI_VM_H
