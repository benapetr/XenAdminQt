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

        /// <summary>
        /// Start the specified VM.  This function can only be called with the VM is in the Halted State.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_start_paused">Instantiate VM in paused state if set to true.</param>
        /// <param name="_force">Attempt to force the VM to start. If this flag is false then the VM may fail pre-boot safety checks (e.g. if the CPU the VM last booted on looks substantially different to the current one)</param>
        static void start(XenSession* session, const QString& vm, bool start_paused, bool force);

        /// <summary>
        /// Start the specified VM.  This function can only be called with the VM is in the Halted State.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_start_paused">Instantiate VM in paused state if set to true.</param>
        /// <param name="_force">Attempt to force the VM to start. If this flag is false then the VM may fail pre-boot safety checks (e.g. if the CPU the VM last booted on looks substantially different to the current one)</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_start(XenSession* session, const QString& vm, bool start_paused, bool force);

        /// <summary>
        /// Start the specified VM on a particular host.  This function can only be called with the VM is in the Halted State.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_host">The Host on which to start the VM</param>
        /// <param name="_start_paused">Instantiate VM in paused state if set to true.</param>
        /// <param name="_force">Attempt to force the VM to start. If this flag is false then the VM may fail pre-boot safety checks (e.g. if the CPU the VM last booted on looks substantially different to the current one)</param>
        static void start_on(XenSession* session, const QString& vm, const QString& host, bool start_paused, bool force);

        /// <summary>
        /// Start the specified VM on a particular host.  This function can only be called with the VM is in the Halted State.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_host">The Host on which to start the VM</param>
        /// <param name="_start_paused">Instantiate VM in paused state if set to true.</param>
        /// <param name="_force">Attempt to force the VM to start. If this flag is false then the VM may fail pre-boot safety checks (e.g. if the CPU the VM last booted on looks substantially different to the current one)</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_start_on(XenSession* session, const QString& vm, const QString& host, bool start_paused, bool force);

        /// <summary>
        /// Resume the specified VM. This can only be called when the specified VM is in the Suspended state.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_start_paused">Resume VM in paused state if set to true.</param>
        /// <param name="_force">Attempt to force the VM to resume. If this flag is false then the VM may fail pre-resume safety checks (e.g. if the CPU the VM was running on looks substantially different to the current one)</param>
        static void resume(XenSession* session, const QString& vm, bool start_paused, bool force);

        /// <summary>
        /// Resume the specified VM. This can only be called when the specified VM is in the Suspended state.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_start_paused">Resume VM in paused state if set to true.</param>
        /// <param name="_force">Attempt to force the VM to resume. If this flag is false then the VM may fail pre-resume safety checks (e.g. if the CPU the VM was running on looks substantially different to the current one)</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_resume(XenSession* session, const QString& vm, bool start_paused, bool force);

        /// <summary>
        /// Resume the specified VM on a particular host.  This can only be called when the specified VM is in the Suspended state.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_host">The Host on which to resume the VM</param>
        /// <param name="_start_paused">Resume VM in paused state if set to true.</param>
        /// <param name="_force">Attempt to force the VM to resume. If this flag is false then the VM may fail pre-resume safety checks (e.g. if the CPU the VM was running on looks substantially different to the current one)</param>
        static void resume_on(XenSession* session, const QString& vm, const QString& host, bool start_paused, bool force);

        /// <summary>
        /// Resume the specified VM on a particular host.  This can only be called when the specified VM is in the Suspended state.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_host">The Host on which to resume the VM</param>
        /// <param name="_start_paused">Resume VM in paused state if set to true.</param>
        /// <param name="_force">Attempt to force the VM to resume. If this flag is false then the VM may fail pre-resume safety checks (e.g. if the CPU the VM was running on looks substantially different to the current one)</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_resume_on(XenSession* session, const QString& vm, const QString& host, bool start_paused, bool force);

        /// <summary>
        /// Attempts to first clean shutdown a VM and if it should fail then perform a hard shutdown on it.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        static void clean_shutdown(XenSession* session, const QString& vm);

        /// <summary>
        /// Attempts to first clean shutdown a VM and if it should fail then perform a hard shutdown on it.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_clean_shutdown(XenSession* session, const QString& vm);

        /// <summary>
        /// Stop executing the specified VM without attempting a clean shutdown.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        static void hard_shutdown(XenSession* session, const QString& vm);

        /// <summary>
        /// Stop executing the specified VM without attempting a clean shutdown.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_hard_shutdown(XenSession* session, const QString& vm);

        /// <summary>
        /// Suspend the specified VM to disk.  This can only be called when the specified VM is in the Running state.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        static void suspend(XenSession* session, const QString& vm);

        /// <summary>
        /// Suspend the specified VM to disk.  This can only be called when the specified VM is in the Running state.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_suspend(XenSession* session, const QString& vm);

        /// <summary>
        /// Attempts to first clean reboot a VM and if it should fail then perform a hard reboot on it.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        static void clean_reboot(XenSession* session, const QString& vm);

        /// <summary>
        /// Attempts to first clean reboot a VM and if it should fail then perform a hard reboot on it.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_clean_reboot(XenSession* session, const QString& vm);

        /// <summary>
        /// Stop executing the specified VM without attempting a clean reboot and immediately restart the VM.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        static void hard_reboot(XenSession* session, const QString& vm);

        /// <summary>
        /// Stop executing the specified VM without attempting a clean reboot and immediately restart the VM.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_hard_reboot(XenSession* session, const QString& vm);

        /// <summary>
        /// Pause the specified VM. This can only be called when the specified VM is in the Running state.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        static void pause(XenSession* session, const QString& vm);

        /// <summary>
        /// Pause the specified VM. This can only be called when the specified VM is in the Running state.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_pause(XenSession* session, const QString& vm);

        /// <summary>
        /// Resume the specified VM. This can only be called when the specified VM is in the Paused state.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        static void unpause(XenSession* session, const QString& vm);

        /// <summary>
        /// Resume the specified VM. This can only be called when the specified VM is in the Paused state.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_unpause(XenSession* session, const QString& vm);

        /// <summary>
        /// Assert whether a VM can boot on this host.
        /// First published in XenServer 6.1.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_self">The opaque_ref of the given vm</param>
        /// <param name="_host">The host on which we want to assert the VM can boot</param>
        static void assert_can_boot_here(XenSession* session, const QString& self, const QString& host);

        /// <summary>
        /// Assert whether all SRs required to recover this VM are available.
        /// First published in XenServer 5.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_self">The opaque_ref of the given vm</param>
        /// <param name="_session_to">The session to which we want to recover the VM.</param>
        static void assert_can_migrate(XenSession* session, const QString& self, const QString& session_to);

        /// <summary>
        /// Assert whether the VM is agile (i.e. can be migrated without downtime).
        /// Used for HA protection checks.
        /// First published in XenServer 5.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_self">The opaque_ref of the given vm</param>
        static void assert_agile(XenSession* session, const QString& self);

        /// <summary>
        /// Get the list of allowed VBD device numbers
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <returns>List of allowed device numbers as QVariant (QStringList)</returns>
        static QVariant get_allowed_VBD_devices(XenSession* session, const QString& vm);

        /// <summary>
        /// Get the full record for a VM
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <returns>VM record as QVariantMap</returns>
        static QVariantMap get_record(XenSession* session, const QString& vm);

        /// <summary>
        /// Get all VMs and their records
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <returns>Map of VM refs to VM records</returns>
        static QVariantMap get_all_records(XenSession* session);

        /// <summary>
        /// Set the suspend VDI for a suspended VM
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_value">The new value for suspend_VDI</param>
        static void set_suspend_VDI(XenSession* session, const QString& vm, const QString& value);

        /// <summary>
        /// Migrate a VM to another Host (async)
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_host">The target host</param>
        /// <param name="_options">Extra configuration operations (live migration, etc.)</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_pool_migrate(XenSession* session, const QString& vm, const QString& host, const QVariantMap& options);

        /// <summary>
        /// Clone a VM (async)
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_new_name">The name of the cloned VM</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_clone(XenSession* session, const QString& vm, const QString& new_name);

        /// <summary>
        /// Copy a VM to an SR (async)
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_new_name">The name of the copied VM</param>
        /// <param name="_sr">The SR to copy the VM to</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_copy(XenSession* session, const QString& vm, const QString& new_name, const QString& sr);

        /// <summary>
        /// Provision a VM (async)
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_provision(XenSession* session, const QString& vm);

        /// <summary>
        /// Destroy a VM
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        static void destroy(XenSession* session, const QString& vm);

        /// <summary>
        /// Set the is_a_template field
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_value">New value for is_a_template</param>
        static void set_is_a_template(XenSession* session, const QString& vm, bool value);

        /// <summary>
        /// Set the name_label field
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_value">New value for name_label</param>
        static void set_name_label(XenSession* session, const QString& vm, const QString& value);

        /// <summary>
        /// Set the name_description field
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_value">New value for name_description</param>
        static void set_name_description(XenSession* session, const QString& vm, const QString& value);

        // Snapshot operations

        /// <summary>
        /// Snapshots the specified VM, making a new VM. Snapshot automatically exploits the capabilities of the underlying storage repository in which the VM's disk images are stored (e.g. Copy on Write).
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_new_name">The name of the snapshotted VM</param>
        /// <returns>Task ref for async operation - result is ref of the newly created VM</returns>
        static QString async_snapshot(XenSession* session, const QString& vm, const QString& new_name);

        /// <summary>
        /// Snapshots the specified VM with quiesce, making a new VM. Snapshot automatically exploits the capabilities of the underlying storage repository in which the VM's disk images are stored (e.g. Copy on Write).
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_new_name">The name of the snapshotted VM</param>
        /// <returns>Task ref for async operation - result is ref of the newly created VM</returns>
        static QString async_snapshot_with_quiesce(XenSession* session, const QString& vm, const QString& new_name);

        /// <summary>
        /// Checkpoints the specified VM, making a new VM. Checkpoint automatically exploits the capabilities of the underlying storage repository in which the VM's disk images are stored (e.g. Copy on Write) and saves the memory image as well.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_new_name">The name of the checkpointed VM</param>
        /// <returns>Task ref for async operation - result is ref of the newly created VM</returns>
        static QString async_checkpoint(XenSession* session, const QString& vm, const QString& new_name);

        /// <summary>
        /// Reverts the specified VM to a previous state.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_snapshot">The opaque_ref of the snapshot</param>
        /// <returns>Task ref for async operation</returns>
        static QString async_revert(XenSession* session, const QString& snapshot);

        // Memory configuration

        /// <summary>
        /// Set the memory limits of the VM.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_static_min">New value for memory_static_min (bytes)</param>
        /// <param name="_static_max">New value for memory_static_max (bytes)</param>
        /// <param name="_dynamic_min">New value for memory_dynamic_min (bytes)</param>
        /// <param name="_dynamic_max">New value for memory_dynamic_max (bytes)</param>
        static void set_memory_limits(XenSession* session, const QString& vm,
                                      qint64 static_min, qint64 static_max,
                                      qint64 dynamic_min, qint64 dynamic_max);

        /// <summary>
        /// Set the dynamic memory range (for running VMs).
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_dynamic_min">New value for memory_dynamic_min (bytes)</param>
        /// <param name="_dynamic_max">New value for memory_dynamic_max (bytes)</param>
        static void set_memory_dynamic_range(XenSession* session, const QString& vm,
                                             qint64 dynamic_min, qint64 dynamic_max);

        // VCPU configuration

        /// <summary>
        /// Set the maximum number of VCPUs for a halted VM.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_value">New value for VCPUs_max</param>
        static void set_VCPUs_max(XenSession* session, const QString& vm, qint64 value);

        /// <summary>
        /// Set the number of VCPUs at startup for a halted VM.
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_value">New value for VCPUs_at_startup</param>
        static void set_VCPUs_at_startup(XenSession* session, const QString& vm, qint64 value);

        /// <summary>
        /// Set the number of VCPUs for a running VM (hotplug).
        /// First published in XenServer 4.0.
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_vm">The opaque_ref of the given vm</param>
        /// <param name="_nvcpu">The number of VCPUs</param>
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

        // TODO: Add more VM methods as needed (pause, unpause, reboot, etc.)
        // See xenadmin/XenModel/XenAPI/VM.cs for complete list

    private:
        VM() = delete; // Static class, no instances
        ~VM() = delete;
    };

} // namespace XenAPI

#endif // XENAPI_VM_H
