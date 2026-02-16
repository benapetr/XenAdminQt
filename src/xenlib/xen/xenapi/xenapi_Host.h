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

#ifndef XENAPI_HOST_H
#define XENAPI_HOST_H

#include "../../xenlib_global.h"
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QHash>
#include <QList>

namespace XenAPI
{
    class Session;

    /**
     * @brief XenAPI Host bindings
     *
     * Static-only class providing XenServer Host API bindings.
     * Matches C# XenModel/XenAPI/Host.cs structure.
     *
     * This is a minimal implementation with only get_all for now.
     * More methods will be added as needed.
     */
    class XENLIB_EXPORT Host
    {
        private:
            Host() = delete; // Static-only class

        public:
            /**
             * @brief Get list of all host references
             * @param session XenServer session
             * @return List of host opaque references
             *
             * Matches C# Host.get_all()
             */
            static QVariantList get_all(Session* session);

            /**
             * @brief Get single host record
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Host record as QVariantMap
             *
             * Matches C# Host.get_record()
             */
            static QVariantMap get_record(Session* session, const QString& host);

            /**
             * @brief Get host server time
             * Matches C# Host.get_servertime()
             */
            static QVariant get_servertime(Session* session, const QString& host);

            /**
             * @brief Query a performance data source for a host
             * @param session The session
             * @param host The opaque_ref of the given Host
             * @param data_source The data source name
             * @return Data source value
             *
             * Matches C# Host.query_data_source()
             */
            static double query_data_source(Session* session, const QString& host, const QString& data_source);

            /**
             * @brief Get available data sources for a host
             * @param session Active Xen session
             * @param host Host opaque reference
             * @return List of data source records
             *
             * Matches C# Host.get_data_sources()
             */
            static QList<QVariantMap> get_data_sources(Session* session, const QString& host);

            /**
             * @brief Enable recording of a host data source
             * @param session Active Xen session
             * @param host Host opaque reference
             * @param data_source Data source name
             *
             * Matches C# Host.record_data_source()
             */
            static void record_data_source(Session* session, const QString& host, const QString& data_source);

            /**
             * @brief Forget archived records for a host data source
             * @param session Active Xen session
             * @param host Host opaque reference
             * @param data_source Data source name
             *
             * Matches C# Host.forget_data_source_archives()
             */
            static void forget_data_source_archives(Session* session, const QString& host, const QString& data_source);

            /**
             * @brief Set the name_label field
             * Matches C# Host.set_name_label()
             */
            static void set_name_label(Session* session, const QString& host, const QString& value);

            /**
             * @brief Set the name_description field
             * Matches C# Host.set_name_description()
             */
            static void set_name_description(Session* session, const QString& host, const QString& value);

            /**
             * @brief Set the tags field
             * Matches C# Host.set_tags()
             */
            static void set_tags(Session* session, const QString& host, const QStringList& value);

            /**
             * @brief Set the other_config field
             * Matches C# Host.set_other_config()
             */
            static void set_other_config(Session* session, const QString& host, const QVariantMap& otherConfig);

            /**
             * @brief Set the logging field
             * Matches C# Host.set_logging()
             */
            static void set_logging(Session* session, const QString& host, const QVariantMap& logging);

            /**
             * @brief Set the iSCSI IQN
             * Matches C# Host.set_iscsi_iqn()
             */
            static void set_iscsi_iqn(Session* session, const QString& host, const QString& value);

            /**
             * @brief Call a plugin on the host
             * @param session XenServer session
             * @param host Host opaque reference
             * @param plugin Plugin name (e.g., "trim", "perfmon")
             * @param function Function name within plugin (e.g., "do_trim")
             * @param args Arguments map for the plugin function
             * @return Plugin result as string
             *
             * Matches C# Host.call_plugin()
             */
            static QString call_plugin(Session* session,
                                       const QString& host,
                                       const QString& plugin,
                                       const QString& function,
                                       const QVariantMap& args);

            /**
             * @brief Disable host (async)
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Task reference
             *
             * Matches C# Host.async_disable()
             * Disables the host for maintenance
             */
            static QString async_disable(Session* session, const QString& host);

            /**
             * @brief Enable host (sync)
             * @param session XenServer session
             * @param host Host opaque reference
             *
             * Matches C# Host.enable()
             * Re-enables the host after maintenance
             */
            static void enable(Session* session, const QString& host);

            /**
             * @brief Enable host (async)
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Task reference
             *
             * Matches C# Host.async_enable()
             * Re-enables the host after maintenance (async version)
             */
            static QString async_enable(Session* session, const QString& host);

            /**
             * @brief Reboot host (async)
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Task reference
             *
             * Matches C# Host.async_reboot()
             * Reboots the physical host
             */
            static QString async_reboot(Session* session, const QString& host);

            /**
             * @brief Shutdown host (async)
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Task reference
             *
             * Matches C# Host.async_shutdown()
             * Powers off the physical host
             */
            static QString async_shutdown(Session* session, const QString& host);

            /**
             * @brief Evacuate host (async)
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Task reference
             *
             * Matches C# Host.async_evacuate()
             * Migrates all VMs off the host
             */
            static QString async_evacuate(Session* session, const QString& host);

            /**
             * @brief Power on a host (sync)
             * @param session XenServer session
             * @param host Host opaque reference
             *
             * Matches C# Host.power_on()
             */
            static void power_on(Session* session, const QString& host);

            /**
             * @brief Retrieve WLB evacuate recommendations
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Map of VM ref -> recommendation string list
             *
             * Matches C# Host.retrieve_wlb_evacuate_recommendations()
             */
            static QHash<QString, QStringList> retrieve_wlb_evacuate_recommendations(Session* session, const QString& host);

            /**
             * @brief Get VMs which prevent evacuation
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Map of VM ref -> reason string list
             *
             * Matches C# Host.get_vms_which_prevent_evacuation()
             */
            static QHash<QString, QStringList> get_vms_which_prevent_evacuation(Session* session, const QString& host);

            /**
             * @brief Destroy host (async)
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Task reference
             *
             * Matches C# Host.async_destroy()
             * Removes host from pool
             */
            static QString async_destroy(Session* session, const QString& host);

            /**
             * @brief Remove entry from other_config
             * @param session XenServer session
             * @param host Host opaque reference
             * @param key Key to remove
             *
             * Matches C# Host.remove_from_other_config()
             */
            static void remove_from_other_config(Session* session, const QString& host, const QString& key);

            /**
             * @brief Add entry to other_config
             * @param session XenServer session
             * @param host Host opaque reference
             * @param key Key to add
             * @param value Value for key
             *
             * Matches C# Host.add_to_other_config()
             */
            static void add_to_other_config(Session* session, const QString& host, const QString& key, const QString& value);

            /**
             * @brief Reconfigure syslog logging
             * @param session XenServer session
             * @param host Host opaque reference
             *
             * Matches C# Host.syslog_reconfigure()
             */
            static void syslog_reconfigure(Session* session, const QString& host);

            /**
             * @brief Reconfigure management interface
             * @param session XenServer session
             * @param pif PIF reference to use as new management interface
             *
             * Matches C# Host.management_reconfigure()
             * Changes the management interface to the specified PIF
             */
            static void management_reconfigure(Session* session, const QString& pif);

            /**
             * @brief Async reconfigure management interface on host
             * @param session Active XenSession
             * @param pif PIF opaque reference to use as new management interface
             * @return Task reference
             *
             * Matches C# Host.async_management_reconfigure()
             */
            static QString async_management_reconfigure(Session* session, const QString& pif);

            /**
             * @brief Prepare host to receive migrating VM
             * @param session Active XenSession (destination session)
             * @param host Destination host opaque reference
             * @param network Network opaque reference for transfer
             * @param options Migration options
             * @return Migration receive data (session info, etc.)
             *
             * Matches C# Host.migrate_receive()
             * Used in cross-pool migration to set up destination
             */
            static QVariantMap migrate_receive(Session* session, const QString& host, const QString& network, const QVariantMap& options);

            /**
             * @brief Restart the XAPI toolstack (agent) on the host (async)
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Task reference
             *
             * Matches C# Host.async_restart_agent()
             * Restarts the XAPI service without rebooting the host.
             * VMs continue running during the restart.
             */
            static QString async_restart_agent(Session* session, const QString& host);

            /**
             * @brief Enable integrated display on host
             * @param session XenServer session
             * @param host Host opaque reference
             * @return New host display state
             *
             * Matches C# Host.enable_display()
             */
            static QString enable_display(Session* session, const QString& host);

            /**
             * @brief Enable integrated display on host (async)
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Task reference
             *
             * Matches C# Host.async_enable_display()
             */
            static QString async_enable_display(Session* session, const QString& host);

            /**
             * @brief Disable integrated display on host
             * @param session XenServer session
             * @param host Host opaque reference
             * @return New host display state
             *
             * Matches C# Host.disable_display()
             */
            static QString disable_display(Session* session, const QString& host);

            /**
             * @brief Disable integrated display on host (async)
             * @param session XenServer session
             * @param host Host opaque reference
             * @return Task reference
             *
             * Matches C# Host.async_disable_display()
             */
            static QString async_disable_display(Session* session, const QString& host);
    };

} // namespace XenAPI

#endif // XENAPI_HOST_H
