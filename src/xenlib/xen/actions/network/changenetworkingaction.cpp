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

#include "changenetworkingaction.h"
#include "networkingactionhelpers.h"
#include "../../connection.h"
#include "../../session.h"
#include "../../xenapi/xenapi_PIF.h"
#include "../../xenapi/xenapi_Host.h"
#include "../../xenapi/xenapi_Pool.h"
#include "../../../xencache.h"
#include <QDebug>

ChangeNetworkingAction::ChangeNetworkingAction(XenConnection* connection,
                                               const QStringList& pifRefsToReconfigure,
                                               const QStringList& pifRefsToDisable,
                                               const QString& newManagementPifRef,
                                               const QString& oldManagementPifRef,
                                               QObject* parent)
    : AsyncOperation(connection,
                     "Changing Network Configuration",
                     "Reconfiguring host networking",
                     parent),
      m_pifRefsToReconfigure(pifRefsToReconfigure),
      m_pifRefsToDisable(pifRefsToDisable),
      m_newManagementPifRef(newManagementPifRef),
      m_oldManagementPifRef(oldManagementPifRef)
{
}

void ChangeNetworkingAction::run()
{
    try
    {
        connection()->setExpectDisruption(true);

        int totalOps = m_pifRefsToReconfigure.count() + m_pifRefsToDisable.count();
        if (!m_newManagementPifRef.isEmpty())
        {
            totalOps += 1; // Management reconfiguration
        }

        // Determine if we're operating on a pool or single host
        QList<QVariantMap> pools = connection()->getCache()->getAll("pool");
        bool isPool = !pools.isEmpty();

        int inc = totalOps > 0 ? (isPool ? 50 : 100) / totalOps : 100;
        int progress = 0;

        // Phase 1: Bring up/reconfigure new PIFs on supporters first, then coordinator
        if (isPool)
        {
            for (const QString& pifRef : m_pifRefsToReconfigure)
            {
                progress += inc;
                reconfigure(pifRef, true, false, progress); // Supporters
            }
        }

        for (const QString& pifRef : m_pifRefsToReconfigure)
        {
            progress += inc;
            reconfigure(pifRef, true, true, progress); // Coordinator (or single host)
        }

        // Phase 2: Reconfigure management interface if requested
        if (!m_newManagementPifRef.isEmpty() && !m_oldManagementPifRef.isEmpty())
        {
            QVariantMap newPifData = connection()->getCache()->resolve("pif", m_newManagementPifRef);
            QVariantMap oldPifData = connection()->getCache()->resolve("pif", m_oldManagementPifRef);

            // Check if we should clear the old management IP
            bool clearDownManagementIP = true;
            for (const QString& pifRef : m_pifRefsToReconfigure)
            {
                QVariantMap pifData = connection()->getCache()->resolve("pif", pifRef);
                if (pifData.value("uuid").toString() == oldPifData.value("uuid").toString())
                {
                    clearDownManagementIP = false;
                    break;
                }
            }

            if (isPool)
            {
                progress += inc;
                // TODO: Check for Host.RestrictManagementOnVLAN feature
                // For now, use pool reconfiguration if available
                QString poolRef = pools.first().value("_ref").toString();

                try
                {
                    NetworkingActionHelpers::poolReconfigureManagement(
                        this, poolRef, m_newManagementPifRef, m_oldManagementPifRef, progress);

                    setDescription("Network configuration complete");
                    connection()->setExpectDisruption(false);
                    return; // Pool reconfiguration handles everything

                } catch (const std::exception& e)
                {
                    qWarning() << "Pool management reconfiguration not available, falling back to host-by-host:" << e.what();
                    // Fall back to host-by-host reconfiguration
                    NetworkingActionHelpers::reconfigureManagement(
                        this, m_oldManagementPifRef, m_newManagementPifRef,
                        false, true, progress, clearDownManagementIP); // Supporters
                }
            }

            progress += inc;
            NetworkingActionHelpers::reconfigureManagement(
                this, m_oldManagementPifRef, m_newManagementPifRef,
                true, true, progress, clearDownManagementIP); // Coordinator or single host
        }

        // Phase 3: Bring down old PIFs on supporters first, then coordinator
        if (isPool)
        {
            for (const QString& pifRef : m_pifRefsToDisable)
            {
                progress += inc;
                reconfigure(pifRef, false, false, progress); // Supporters
            }
        }

        for (const QString& pifRef : m_pifRefsToDisable)
        {
            progress += inc;
            reconfigure(pifRef, false, true, progress); // Coordinator (or single host)
        }

        setDescription("Network configuration complete");
        connection()->setExpectDisruption(false);

    } catch (const std::exception& e)
    {
        connection()->setExpectDisruption(false);
        setError(QString("Failed to change networking: %1").arg(e.what()));
    }
}

void ChangeNetworkingAction::reconfigure(const QString& pifRef, bool up, bool thisHost, int hi)
{
    // Use NetworkingActionHelpers::forSomeHosts to execute on appropriate hosts
    // This handles coordinator vs supporter logic automatically

    if (up)
    {
        // Bring up: configure IP and plug the PIF
        NetworkingActionHelpers::forSomeHosts(this, pifRef, thisHost, true, hi,
                                              [this, pifRef](AsyncOperation* action, const QString& existingPifRef, int h) {
                                                  // TODO: Add clustering support (disable clustering before, enable after)
                                                  // For now, just bring up the interface
                                                  bringUp(pifRef, existingPifRef, h);
                                              });
    } else
    {
        // Bring down: depurpose and clear IP
        NetworkingActionHelpers::forSomeHosts(this, pifRef, thisHost, true, hi,
                                              [](AsyncOperation* action, const QString& existingPifRef, int h) {
                                                  // TODO: Add clustering support (disable clustering before, enable after)
                                                  NetworkingActionHelpers::bringDown(action, existingPifRef, h);
                                              });
    }
}

void ChangeNetworkingAction::bringUp(const QString& newPifRef, const QString& existingPifRef, int hi)
{
    QVariantMap newPifData = connection()->getCache()->resolve("pif", newPifRef);
    QString ipMode = newPifData.value("ip_configuration_mode").toString();
    QString ip = newPifData.value("IP").toString();

    // For static IP in pool environments, calculate IP from range
    // Simplified: use the same IP as newPif
    // TODO: Implement GetIPInRange logic for pool-wide static IP assignment

    NetworkingActionHelpers::bringUp(this, newPifRef, ip, existingPifRef, hi);
}
