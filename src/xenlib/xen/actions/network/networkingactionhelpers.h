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

#ifndef NETWORKINGACTIONHELPERS_H
#define NETWORKINGACTIONHELPERS_H

#include <QString>
#include <QVariantMap>
#include <functional>

// Forward declarations
class AsyncOperation;
class XenConnection;

/**
 * @brief Helper functions for networking actions
 *
 * Provides static utility methods for coordinated network reconfiguration
 * across pool hosts. Handles management interface migration, PIF configuration,
 * and pool-wide network changes.
 *
 * Matches C# XenAdmin.Actions.NetworkingActionHelpers
 */
class NetworkingActionHelpers
{
    private:
        NetworkingActionHelpers() = delete; // Static-only class

        // Private helper methods
        static void depurpose(AsyncOperation* action, const QString& pifRef, int hi);
        static void reconfigureManagement_(AsyncOperation* action, const QString& pifRef, int hi);
        static void poolManagementReconfigure_(AsyncOperation* action, const QString& pifRef, int hi);
        static void clearIP(AsyncOperation* action, const QString& pifRef, int hi);
        static void waitForMembersToRecover(XenConnection* connection, const QString& poolRef);

    public:
        /**
         * @brief Delegate type for PIF operations
         *
         * Signature: void callback(AsyncOperation* action, QString pifRef, int progressHi)
         */
        typedef std::function<void(AsyncOperation*, const QString&, int)> PIFMethod;

        /**
         * @brief Move management interface name from one PIF to another
         *
         * If the "from" PIF is a secondary management interface, transfers the
         * management_purpose label to the "to" PIF.
         *
         * @param action Parent async operation for session/progress
         * @param fromPifRef Source PIF opaque reference
         * @param toPifRef Destination PIF opaque reference
         */
        static void moveManagementInterfaceName(AsyncOperation* action,
                                                const QString& fromPifRef,
                                                const QString& toPifRef);

        /**
         * @brief Reconfigure management interface on hosts
         *
         * Switches management from down_pif to up_pif with coordinated sequence:
         * 1. Depurpose down_pif (clear disallow_unplug and management_purpose)
         * 2. Call Host.management_reconfigure to switch to up_pif
         * 3. Clear IP from down_pif (if bring_down_down_pif is true)
         *
         * @param action Parent async operation
         * @param downPifRef Current management PIF reference
         * @param upPifRef New management PIF reference
         * @param thisHost If true, operate on coordinator only; if false, on supporters only
         * @param lockPif If true, set PIF.Locked during operation (not yet implemented in Qt)
         * @param hi Progress percentage target (0-100)
         * @param bringDownDownPif If true, depurpose and clear IP on down_pif
         */
        static void reconfigureManagement(AsyncOperation* action,
                                          const QString& downPifRef,
                                          const QString& upPifRef,
                                          bool thisHost,
                                          bool lockPif,
                                          int hi,
                                          bool bringDownDownPif);

        /**
         * @brief Pool-wide management interface reconfiguration
         *
         * Uses Pool.async_management_reconfigure for pool-wide changes.
         * This triggers pool_recover_slaves internally to coordinate across hosts.
         * Waits for supporters to recover before clearing IPs.
         *
         * @param action Parent async operation
         * @param poolRef Pool opaque reference
         * @param upPifRef New management PIF reference (on coordinator)
         * @param downPifRef Current management PIF reference (on coordinator)
         * @param hi Progress percentage target (0-100)
         */
        static void poolReconfigureManagement(AsyncOperation* action,
                                              const QString& poolRef,
                                              const QString& upPifRef,
                                              const QString& downPifRef,
                                              int hi);

        /**
         * @brief Bring up a PIF with IP configuration
         *
         * Configures IP address, management purpose, disallow_unplug flag, and plugs the PIF.
         * For primary management (empty management_purpose), sets disallow_unplug=false.
         * For secondary management, sets disallow_unplug=true and plugs explicitly.
         *
         * @param action Parent async operation
         * @param newPifRef Source of IP configuration
         * @param newIp IP address to configure (can differ from newPifRef.IP for pool operations)
         * @param existingPifRef PIF to configure and bring up
         * @param hi Progress percentage target (0-100)
         */
        static void bringUp(AsyncOperation* action,
                            const QString& newPifRef,
                            const QString& newIp,
                            const QString& existingPifRef,
                            int hi);

        /**
         * @brief Bring up a PIF (uses newPif's IP address)
         *
         * Overload that uses the IP from newPifRef.
         *
         * @param action Parent async operation
         * @param newPifRef Source PIF with IP configuration
         * @param existingPifRef PIF to configure and bring up
         * @param hi Progress percentage target (0-100)
         */
        static void bringUp(AsyncOperation* action,
                            const QString& newPifRef,
                            const QString& existingPifRef,
                            int hi);

        /**
         * @brief Bring down a PIF
         *
         * Clears disallow_unplug, management_purpose, and removes IP address.
         * Skips IP clearing if PIF is used by clustering (IsUsedByClustering).
         *
         * @param action Parent async operation
         * @param pifRef PIF to bring down
         * @param hi Progress percentage target (0-100)
         */
        static void bringDown(AsyncOperation* action,
                              const QString& pifRef,
                              int hi);

        /**
         * @brief Execute a PIF method on hosts in a network
         *
         * Finds all PIFs in the same network as the given PIF, filters by host
         * (coordinator vs supporters), and executes the callback on each.
         *
         * @param action Parent async operation
         * @param pifRef Reference PIF (determines network and host)
         * @param thisHost If true, operate on coordinator only; if false, on supporters only
         * @param lockPif If true, set PIF.Locked during operation (not yet implemented in Qt)
         * @param hi Progress percentage target (0-100)
         * @param pifMethod Callback to execute for each matching PIF
         */
        static void forSomeHosts(AsyncOperation* action,
                                 const QString& pifRef,
                                 bool thisHost,
                                 bool lockPif,
                                 int hi,
                                 PIFMethod pifMethod);

        /**
         * @brief Reconfigure single primary management interface
         *
         * Used for simple management interface changes (single host or non-bonded).
         * Copies IP config from src to dest, brings up dest, then reconfigures management.
         *
         * @param action Parent async operation
         * @param srcPifRef Source management PIF
         * @param destPifRef Destination PIF
         * @param hi Progress percentage target (0-100)
         */
        static void reconfigureSinglePrimaryManagement(AsyncOperation* action,
                                                       const QString& srcPifRef,
                                                       const QString& destPifRef,
                                                       int hi);

        /**
         * @brief Reconfigure primary management across pool
         *
         * Used for complex management interface changes (bonded networks).
         * Coordinates bringing up bond members on supporters, then coordinator,
         * then reconfiguring management on supporters, then coordinator.
         *
         * @param action Parent async operation
         * @param srcPifRef Source management PIF
         * @param destPifRef Destination PIF
         * @param hi Progress percentage target (0-100)
         */
        static void reconfigurePrimaryManagement(AsyncOperation* action,
                                                 const QString& srcPifRef,
                                                 const QString& destPifRef,
                                                 int hi);

        /**
         * @brief Reconfigure IP address on a PIF
         *
         * Calls PIF.async_reconfigure_ip with parameters from newPifRef.
         *
         * @param action Parent async operation
         * @param newPifRef Source of IP configuration
         * @param existingPifRef PIF to reconfigure
         * @param ip IP address to assign
         * @param hi Progress percentage target (0-100)
         */
        static void reconfigureIP(AsyncOperation* action,
                                  const QString& newPifRef,
                                  const QString& existingPifRef,
                                  const QString& ip,
                                  int hi);

        /**
         * @brief Plug a PIF if not currently attached
         *
         * Checks PIF.currently_attached and calls PIF.async_plug if false.
         *
         * @param action Parent async operation
         * @param pifRef PIF to plug
         * @param hi Progress percentage target (0-100)
         */
        static void plug(AsyncOperation* action,
                         const QString& pifRef,
                         int hi);

        // Private overloads for forSomeHosts with different signatures
    private:
        static void bringUp(AsyncOperation* action,
                            const QString& newPifRef,
                            bool thisHost,
                            int hi);
};

#endif // NETWORKINGACTIONHELPERS_H
