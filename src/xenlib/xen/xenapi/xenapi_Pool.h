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

#ifndef XENAPI_POOL_H
#define XENAPI_POOL_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace XenAPI
{
    class Session;

    /**
     * @brief Pool - XenAPI Pool bindings
     *
     * Static-only class providing XenAPI Pool method bindings.
     */
    class Pool
    {
        private:
            Pool() = delete; // Static-only class

        public:
            /**
             * @brief Get all pool references (typically returns one element)
             * @param session Active XenSession
             * @return QVariant containing list of pool references
             */
            static QVariant get_all(Session* session);

            /**
             * @brief Get all pool records in a single call
             * @param session Active XenSession
             * @return QVariantMap mapping pool refs to pool records
             *
             * Matches C# Pool.get_all_records()
             */
            static QVariantMap get_all_records(Session* session);

            /**
             * @brief Set default SR for pool
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @param sr SR opaque reference
             */
            static void set_default_SR(Session* session, const QString& pool, const QString& sr);

            /**
             * @brief Set suspend image SR for pool
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @param sr SR opaque reference
             */
            static void set_suspend_image_SR(Session* session, const QString& pool, const QString& sr);

            /**
             * @brief Set crash dump SR for pool
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @param sr SR opaque reference
             */
            static void set_crash_dump_SR(Session* session, const QString& pool, const QString& sr);

            /**
             * @brief Designate new pool master/coordinator (async)
             * @param session Active XenSession
             * @param host Host opaque reference of new coordinator
             * @return Task reference
             *
             * Matches C# Pool.async_designate_new_master()
             */
            static QString async_designate_new_master(Session* session, const QString& host);

            /**
             * @brief Reconfigure pool-wide management interface (async)
             * @param session Active XenSession
             * @param network Network opaque reference to use for management
             * @return Task reference
             *
             * Switches the management interface for all hosts in the pool to the specified network.
             * This triggers pool_recover_slaves internally to coordinate changes across all hosts.
             *
             * Matches C# Pool.async_management_reconfigure()
             */
            static QString async_management_reconfigure(Session* session, const QString& network);

            /**
             * @brief Get pool record
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @return QVariantMap containing pool record fields
             */
            static QVariantMap get_record(Session* session, const QString& pool);

            /**
             * @brief Get pool master/coordinator host reference
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @return Host opaque reference of pool coordinator
             */
            static QString get_master(Session* session, const QString& pool);

            /**
             * @brief Join a host to a pool (async)
             * @param session Active XenSession (from host being joined)
             * @param master_address IP address or hostname of pool coordinator
             * @param master_username Username for pool coordinator
             * @param master_password Password for pool coordinator
             * @return Task reference
             *
             * Instructs a standalone host to join an existing pool.
             * The session should be from the host being joined, not the pool.
             */
            static QString async_join(Session* session, const QString& master_address,
                                      const QString& master_username, const QString& master_password);

            /**
             * @brief Eject a host from a pool
             * @param session Active XenSession (from pool coordinator)
             * @param host Host opaque reference to eject
             *
             * Removes a host from the pool. Host must have no running VMs.
             */
            static void eject(Session* session, const QString& host);

            /**
             * @brief Set pool name label
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @param label New name for pool
             */
            static void set_name_label(Session* session, const QString& pool, const QString& label);

            /**
             * @brief Set pool name description
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @param description New description for pool
             */
            static void set_name_description(Session* session, const QString& pool, const QString& description);

            /**
             * @brief Set pool tags
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @param tags New tags list
             */
            static void set_tags(Session* session, const QString& pool, const QStringList& tags);

            /**
             * @brief Set migration compression flag
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @param enabled New value for migration_compression
             */
            static void set_migration_compression(Session* session, const QString& pool, bool enabled);

            /**
             * @brief Set live patching disabled flag
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @param value New value for live_patching_disabled
             *
             * C# Reference: Pool.cs line 1394
             */
            static void set_live_patching_disabled(Session* session, const QString& pool, bool value);

            /**
             * @brief Set IGMP snooping enabled flag
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @param value New value for igmp_snooping_enabled
             *
             * First published in XenServer 7.3.
             * C# Reference: Pool.cs line 2467
             */
            static void set_igmp_snooping_enabled(Session* session, const QString& pool, bool value);

            /**
             * @brief Enable SSL legacy mode on all hosts in pool
             * @param session Active XenSession
             * @param pool Pool opaque reference
             *
             * Sets ssl_legacy true on each host, pool-master last.
             * C# Reference: Pool.cs line 2416
             */
            static void enable_ssl_legacy(Session* session, const QString& pool);

            /**
             * @brief Disable SSL legacy mode on all hosts in pool
             * @param session Active XenSession
             * @param pool Pool opaque reference
             *
             * Sets ssl_legacy false on each host, pool-master last.
             * C# Reference: Pool.cs line 2442
             */
            static void disable_ssl_legacy(Session* session, const QString& pool);

            /**
             * @brief Set SSL legacy mode (wrapper for enable/disable)
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @param enable True to enable SSL legacy, false to disable
             */
            static void set_ssl_legacy(Session* session, const QString& pool, bool enable);

            /**
             * @brief Enable HA on pool (async)
             * @param session Active XenSession
             * @param heartbeat_srs List of SR opaque references to use for heartbeat
             * @param configuration HA configuration map (can be empty)
             * @return Task reference
             *
             * Enables High Availability for the pool. Requires at least one shared SR for heartbeat.
             * Sets up HA metadata and starts HA monitoring.
             */
            static QString async_enable_ha(Session* session, const QStringList& heartbeat_srs,
                                           const QVariantMap& configuration);

            /**
             * @brief Disable HA on pool (async)
             * @param session Active XenSession
             * @return Task reference
             *
             * Disables High Availability for the pool and removes HA metadata.
             */
            static QString async_disable_ha(Session* session);

            /**
             * @brief Set number of host failures to tolerate
             * @param session Active XenSession
             * @param pool Pool opaque reference
             * @param value Number of failures to tolerate (typically 0-3)
             *
             * Sets the HA restart priority. Must be called before enabling HA.
             */
            static void set_ha_host_failures_to_tolerate(Session* session, const QString& pool, qint64 value);

            /**
             * @brief Compute maximum host failures to tolerate for current pool state
             * @param session Active XenSession
             * @return Max host failures to tolerate
             */
            static qint64 ha_compute_max_host_failures_to_tolerate(Session* session);

            /**
             * @brief Compute maximum host failures to tolerate for a hypothetical HA configuration
             * @param session Active XenSession
             * @param configuration VM restart priorities (VM ref -> priority string)
             * @return Max host failures to tolerate
             */
            static qint64 ha_compute_hypothetical_max_host_failures_to_tolerate(Session* session, const QVariantMap& configuration);

            /**
             * @brief Emergency transition to master (synchronous)
             * @param session Active XenSession
             *
             * Instructs a host that's currently a slave to transition to being master.
             * Used in emergency situations when the current master is unavailable.
             * This is a synchronous operation - does not return a task.
             */
            static void emergency_transition_to_master(Session* session);

            /**
             * @brief Forcibly synchronise the database now (asynchronous)
             * @param session Active XenSession
             * @return Task reference for async operation
             *
             * Ensures all pool members have the latest database state.
             * First published in XenServer 4.0.
             */
            static QString async_sync_database(Session* session);

            /**
             * @brief Rotate the pool secret
             * @param session Active XenSession
             * @param pool Pool opaque reference
             *
             * Rotates the shared secret used for authentication between hosts in the pool.
             * After rotation, all hosts will use the new secret for inter-host communication.
             * Requires XenServer 8.0 (Stockholm) or later.
             *
             * First published in XenServer 8.0.
             */
            static void rotate_secret(Session* session, const QString& pool);
    };

} // namespace XenAPI

#endif // XENAPI_POOL_H
