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

#ifndef XENAPI_VM_APPLIANCE_H
#define XENAPI_VM_APPLIANCE_H

#include <QString>
#include <QVariantMap>
#include <QStringList>

/**
 * @brief XenAPI VM_appliance bindings - static methods only
 *
 * VM appliances (vApps) are groups of VMs that are started/stopped together.
 * First published in XenServer 6.0.
 *
 * Matches C# XenAPI.VM_appliance class from xenadmin/XenModel/XenAPI/VM_appliance.cs
 */
namespace XenAPI
{
    class Session;

    class VM_appliance
    {
        public:
            // Prevent instantiation - this is a static-only class
            VM_appliance() = delete;
            VM_appliance(const VM_appliance&) = delete;
            VM_appliance& operator=(const VM_appliance&) = delete;

            /**
             * @brief Get allowed operations for this VM appliance
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             * @return List of allowed vm_appliance_operation values (as strings: "start", "clean_shutdown", etc.)
             */
            static QStringList get_allowed_operations(Session* session, const QString& applianceRef);

            /**
             * @brief Get current operations in progress
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             * @return Map of task_ref → operation_name
             */
            static QVariantMap get_current_operations(Session* session, const QString& applianceRef);

            /**
             * @brief Get list of VMs in this appliance
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             * @return List of VM OpaqueRefs
             */
            static QStringList get_VMs(Session* session, const QString& applianceRef);

            /**
             * @brief Get full record for VM appliance
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             * @return QVariantMap with all fields: uuid, name_label, name_description, allowed_operations, current_operations, VMs
             */
            static QVariantMap get_record(Session* session, const QString& applianceRef);

            /**
             * @brief Get all VM appliance records
             * @param session XenSession with valid connection
             * @return Map of appliance_ref → record_map
             */
            static QVariantMap get_all_records(Session* session);

            /**
             * @brief Set the name/label field
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             * @param label New name
             */
            static void set_name_label(Session* session, const QString& applianceRef, const QString& label);

            /**
             * @brief Set the name/description field
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             * @param description New description
             */
            static void set_name_description(Session* session, const QString& applianceRef, const QString& description);

            /**
             * @brief Start all VMs in the appliance (async)
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             * @param paused If true, start VMs in paused state
             * @return Task reference (OpaqueRef) for async operation
             */
            static QString async_start(Session* session, const QString& applianceRef, bool paused);

            /**
             * @brief Start all VMs in the appliance (sync - blocks until complete)
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             * @param paused If true, start VMs in paused state
             */
            static void start(Session* session, const QString& applianceRef, bool paused);

            /**
             * @brief Perform clean shutdown of all VMs in the appliance (async)
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             * @return Task reference (OpaqueRef) for async operation
             */
            static QString async_clean_shutdown(Session* session, const QString& applianceRef);

            /**
             * @brief Perform clean shutdown of all VMs in the appliance (sync)
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             */
            static void clean_shutdown(Session* session, const QString& applianceRef);

            /**
             * @brief Perform hard shutdown of all VMs in the appliance (async)
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             * @return Task reference (OpaqueRef) for async operation
             */
            static QString async_hard_shutdown(Session* session, const QString& applianceRef);

            /**
             * @brief Perform hard shutdown of all VMs in the appliance (sync)
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             */
            static void hard_shutdown(Session* session, const QString& applianceRef);

            /**
             * @brief Try clean shutdown, fall back to hard shutdown (async)
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             * @return Task reference (OpaqueRef) for async operation
             */
            static QString async_shutdown(Session* session, const QString& applianceRef);

            /**
             * @brief Try clean shutdown, fall back to hard shutdown (sync)
             * @param session XenSession with valid connection
             * @param applianceRef OpaqueRef of VM_appliance
             */
            static void shutdown(Session* session, const QString& applianceRef);
    };

} // namespace XenAPI

#endif // XENAPI_VM_APPLIANCE_H
