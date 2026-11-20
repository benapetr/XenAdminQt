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

#include "messagealert.h"
#include "../xen/role.h"
#include "alarmmessagealert.h"
#include "policyalert.h"
#include "certificatealert.h"
#include "failedloginattemptalert.h"
#include "leafcoalescealert.h"
#include "alertmanager.h"
#include "../xen/network/connection.h"
#include "../xen/xenobject.h"
#include "../xen/xenobjecttype.h"
#include "../xen/message.h"
#include "../xen/host.h"
#include "../xen/pif.h"
#include "../xen/pool.h"
#include "../xencache.h"
#include "../xen/friendlynames.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>

using namespace XenLib;

MessageAlert::MessageAlert(XenConnection* connection, const QVariantMap& messageData) : Alert(connection), m_messageData(messageData), m_priority(AlertPriority::Unknown)
{
    this->parseMessageData();
}

namespace
{
    // C# Reference: Helpers.GetPoolOfOne(connection)
    QSharedPointer<Pool> poolOfOne(XenConnection* connection)
    {
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        return cache ? cache->GetPoolOfOne() : QSharedPointer<Pool>();
    }

    // C# Reference: Helpers.IsPool(connection) - a visible (named or multi-host) pool exists
    bool isPool(XenConnection* connection)
    {
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        return cache && cache->GetPool();
    }

    QString hostNameByUuid(XenConnection* connection, const QString& uuid)
    {
        XenCache* cache = connection ? connection->GetCache() : nullptr;
        if (!cache)
            return QString();

        for (const QSharedPointer<Host>& host : cache->GetAll<Host>())
        {
            if (host && host->GetUUID() == uuid)
                return host->GetName();
        }
        return QString();
    }

    QString fmt(const QString& format, const QStringList& args)
    {
        return FriendlyNames::Format(format, args);
    }
} // namespace

QString MessageAlert::GetTitle() const
{
    // C# Reference: MessageAlert.Title
    const QString type = this->GetMessageType();

    if (type == QLatin1String("GFS2_CAPACITY"))
        return Message::FriendlyName(type);

    QString title = Message::FriendlyName(type);
    if (title.isEmpty())
        title = type;

    if (XenObjectTypeFromString(this->GetObjClass()) != XenObjectType::Pool)
    {
        QSharedPointer<XenObject> xenObject = this->GetXenObject();
        const bool isHost = xenObject && xenObject->GetObjectType() == XenObjectType::Host;
        if (!isHost || isPool(xenObject->GetConnection()))
        {
            const QString name = xenObject ? xenObject->GetName() : QString();
            if (!name.isEmpty())
                title = QStringLiteral("%1: %2").arg(name, title);
        }
    }

    return title;
}

QString MessageAlert::GetDescription() const
{
    // C# Reference: MessageAlert.Description
    // If you add something here, be sure the corresponding FriendlyNames entry exists.
    const QString type = this->GetMessageType();
    const QString body = this->GetMessageBody();
    const QSharedPointer<XenObject> xenObject = this->GetXenObject();
    const QString objectName = xenObject ? xenObject->GetName() : QString();
    const QString friendlyFormat = Message::FriendlyBody(type);

    static const QSet<QString> nameOnlyTypes = {
        // applies to hosts, vms and pools where only the name is required
        "HA_HEARTBEAT_APPROACHING_TIMEOUT", "HA_HOST_FAILED", "HA_HOST_WAS_FENCED",
        "HA_PROTECTED_VM_RESTART_FAILED", "HA_STATEFILE_APPROACHING_TIMEOUT", "HA_STATEFILE_LOST",
        "HA_XAPI_HEALTHCHECK_APPROACHING_TIMEOUT", "LICENSE_DOES_NOT_SUPPORT_POOLING",
        "PBD_PLUG_FAILED_ON_SERVER_START", "VM_CLONED", "VM_CRASHED", "VM_REBOOTED", "VM_RESUMED",
        "VM_SHUTDOWN", "VM_STARTED", "VM_SUSPENDED", "METADATA_LUN_BROKEN", "METADATA_LUN_HEALTHY",
        "LICENSE_SERVER_UNREACHABLE", "LICENSE_SERVER_VERSION_OBSOLETE", "GRACE_LICENSE",
        "LICENSE_NOT_AVAILABLE", "LICENSE_EXPIRED", "LICENSE_SERVER_CONNECTED",
        "LICENSE_SERVER_UNAVAILABLE", "HOST_CLOCK_WENT_BACKWARDS", "POOL_CPU_FEATURES_UP",
        "POOL_CPU_FEATURES_DOWN", "HOST_CPU_FEATURES_UP", "HOST_CPU_FEATURES_DOWN",
        "VDI_CBT_RESIZE_FAILED", "VDI_CBT_SNAPSHOT_FAILED", "VDI_CBT_METADATA_INCONSISTENT",
        "CLUSTER_HOST_FENCING", "CLUSTER_HOST_ENABLE_FAILED", "VM_SECURE_BOOT_FAILED",
        "TLS_VERIFICATION_EMERGENCY_DISABLED",
        // C# LICENSE_EXPIRES_SOON also only uses the object name
        "LICENSE_EXPIRES_SOON",
    };

    if (xenObject)
    {
        if (type == QLatin1String("HA_POOL_DROP_IN_PLAN_EXISTS_FOR") || type == QLatin1String("HA_POOL_OVERCOMMITTED"))
        {
            bool ok = false;
            const int pef = body.trimmed().toInt(&ok);
            if (ok)
            {
                const QString suffix = pef == 0 ? QStringLiteral("0") : pef == 1 ? QStringLiteral("1") : QStringLiteral("n");
                const QString f = Message::FriendlyBody(QStringLiteral("ha_pool_drop_in_plan_exists_for-") + suffix);
                return fmt(f, {objectName, QString::number(pef)});
            }
        } else if (nameOnlyTypes.contains(type))
        {
            return fmt(friendlyFormat, {objectName});
        } else if (type == QLatin1String("HOST_CLOCK_SKEW_DETECTED") || type == QLatin1String("POOL_MASTER_TRANSITION"))
        {
            // object then pool
            QSharedPointer<Pool> pool = poolOfOne(xenObject->GetConnection());
            if (pool)
                return fmt(friendlyFormat, {objectName, pool->GetName()});
        } else if (type == QLatin1String("HA_NETWORK_BONDING_ERROR"))
        {
            return fmt(friendlyFormat, {this->getManagementBondName(), objectName});
        } else if (type == QLatin1String("VBD_QOS_FAILED") || type == QLatin1String("VCPU_QOS_FAILED") || type == QLatin1String("VIF_QOS_FAILED"))
        {
            return fmt(friendlyFormat, {QString(), objectName});
        } else if (type == QLatin1String("EXTAUTH_INIT_IN_HOST_FAILED"))
        {
            static const QRegularExpression extAuthRegex(QStringLiteral("error=(.*)"));
            const QRegularExpressionMatch m = extAuthRegex.match(body);
            return m.hasMatch() ? fmt(friendlyFormat, {objectName, m.captured(1)}) : QString();
        } else if (type == QLatin1String("EXTAUTH_IN_POOL_IS_NON_HOMOGENEOUS"))
        {
            QSharedPointer<Pool> pool = poolOfOne(xenObject->GetConnection());
            return fmt(friendlyFormat, {pool ? pool->GetName() : QString()});
        } else if (type == QLatin1String("MULTIPATH_PERIODIC_ALERT"))
        {
            return this->extractMultipathCurrentState(friendlyFormat);
        } else if (type == QLatin1String("WLB_CONSULTATION_FAILED"))
        {
            QSharedPointer<Pool> pool = poolOfOne(xenObject->GetConnection());
            return fmt(friendlyFormat, {pool ? pool->GetName() : QString(), objectName});
        } else if (type == QLatin1String("WLB_OPTIMIZATION_ALERT"))
        {
            static const QRegularExpression wlbOptAlertRegex(QStringLiteral("severity:(.*) mode:(.*)"));
            const QRegularExpressionMatch m = wlbOptAlertRegex.match(body);
            QSharedPointer<Pool> pool = poolOfOne(xenObject->GetConnection());
            return m.hasMatch() ? fmt(friendlyFormat, {pool ? pool->GetName() : QString(), m.captured(2), m.captured(1)}) : QString();
        } else if (type == QLatin1String("GFS2_CAPACITY"))
        {
            return fmt(Message::FriendlyBody(type), {objectName});
        }
        // PVS_PROXY_NO_CACHE_SR_AVAILABLE is not ported (PVS is out of scope), so it uses the body
    }

    return body;
}

AlertPriority MessageAlert::GetPriority() const
{
    return this->m_priority;
}

QString MessageAlert::AppliesTo() const
{
    // C# Reference: MessageAlert.AppliesTo - the pool (or standalone server) name
    QSharedPointer<Pool> pool = poolOfOne(this->m_connection);
    const QString name = pool ? pool->GetName() : QString();
    return !name.isEmpty() ? name : this->GetObjUUID();
}

QString MessageAlert::GetName() const
{
    return this->m_name;
}

void MessageAlert::Dismiss()
{
    // C# MessageAlert.Dismiss() calls Message.destroy synchronously. Here the server-side
    // deletion is done by DismissAlertsAction (off the UI thread), which the UI runs for
    // message alerts; this only marks the alert locally.
    this->m_dismissing = true;
}

bool MessageAlert::IsAllowedToDismiss() const
{
    // C# Reference: MessageAlert.AllowedToDismiss()
    if (this->m_dismissing)
        return false;

    // this shouldn't happen for this type of alert, but check for safety
    if (!this->m_connection)
        return true;

    // if we are disconnected do not dismiss as the alert will disappear soon
    if (!this->m_connection->IsConnected())
        return false;

    // C#: the local superuser, or a role allowed to call Message.destroy
    return Role::SessionCanPerform(this->m_connection, QStringLiteral("Message.destroy"));
}

QString MessageAlert::GetMessageType() const
{
    return this->m_messageData.value("name").toString();
}

QString MessageAlert::GetMessageBody() const
{
    return this->m_messageData.value("body").toString();
}

QString MessageAlert::GetObjUUID() const
{
    return this->m_messageData.value("obj_uuid").toString();
}

QString MessageAlert::GetOpaqueRef() const
{
    return this->m_messageData.value("ref").toString();
}

QString MessageAlert::GetObjClass() const
{
    return this->m_messageData.value("cls").toString();
}

QSharedPointer<XenObject> MessageAlert::GetXenObject() const
{
    // C# Reference: Message.GetXenObject() - resolves cls + obj_uuid via the cache
    if (!this->m_connection)
        return QSharedPointer<XenObject>();

    XenCache* cache = this->m_connection->GetCache();
    const QString objUuid = this->GetObjUUID();
    if (!cache || objUuid.isEmpty())
        return QSharedPointer<XenObject>();

    const XenObjectType type = XenObjectTypeFromString(this->GetObjClass());
    if (type == XenObjectType::Null)
        return QSharedPointer<XenObject>();

    const QList<QSharedPointer<XenObject>> objects = cache->GetAll(type);
    for (const QSharedPointer<XenObject>& object : objects)
    {
        if (object && object->IsValid() && object->GetUUID() == objUuid)
            return object;
    }

    return QSharedPointer<XenObject>();
}

Alert* MessageAlert::ParseMessage(XenConnection* connection, const QVariantMap& messageData)
{
    // C# Reference: MessageAlert.cs line 462 - ParseMessage()
    // Factory method that creates the appropriate alert type based on message type
    
    QString msgType = messageData.value("name").toString();
    
    // Create specialized alert types based on message type (C# Reference: line 467-504)
    
    // Performance alarms
    if (msgType == "ALARM")
        return new AlarmMessageAlert(connection, messageData);
    
    // VMSS (VM Snapshot Schedule) alerts
    if (msgType == "VMSS_SNAPSHOT_MISSED_EVENT" ||
        msgType == "VMSS_XAPI_LOGON_FAILURE" ||
        msgType == "VMSS_LICENSE_ERROR" ||
        msgType == "VMSS_SNAPSHOT_FAILED" ||
        msgType == "VMSS_SNAPSHOT_SUCCEEDED" ||
        msgType == "VMSS_SNAPSHOT_LOCK_FAILED")
    {
        return new PolicyAlert(connection, messageData);
    }
    
    // Certificate expiry warnings
    if (msgType == "POOL_CA_CERTIFICATE_EXPIRED" ||
        msgType == "POOL_CA_CERTIFICATE_EXPIRING_07" ||
        msgType == "POOL_CA_CERTIFICATE_EXPIRING_14" ||
        msgType == "POOL_CA_CERTIFICATE_EXPIRING_30" ||
        msgType == "HOST_SERVER_CERTIFICATE_EXPIRED" ||
        msgType == "HOST_SERVER_CERTIFICATE_EXPIRING_07" ||
        msgType == "HOST_SERVER_CERTIFICATE_EXPIRING_14" ||
        msgType == "HOST_SERVER_CERTIFICATE_EXPIRING_30" ||
        msgType == "HOST_INTERNAL_CERTIFICATE_EXPIRED" ||
        msgType == "HOST_INTERNAL_CERTIFICATE_EXPIRING_07" ||
        msgType == "HOST_INTERNAL_CERTIFICATE_EXPIRING_14" ||
        msgType == "HOST_INTERNAL_CERTIFICATE_EXPIRING_30")
    {
        return new CertificateAlert(connection, messageData);
    }
    
    if (msgType == "FAILED_LOGIN_ATTEMPTS")
        return new FailedLoginAttemptAlert(connection, messageData);

    if (msgType == "LEAF_COALESCE_START_MESSAGE" ||
        msgType == "LEAF_COALESCE_COMPLETED" ||
        msgType == "LEAF_COALESCE_FAILED")
    {
        return new LeafCoalesceAlert(connection, messageData);
    }

    // CssExpiryAlert (UPDATES_FEATURE_EXPIR*) is Citrix-only and intentionally not ported.
    
    // Default: basic MessageAlert for unrecognized types
    return new MessageAlert(connection, messageData);
}

void MessageAlert::RemoveAlert(const QString& messageRef)
{
    // C# Reference: MessageAlert.cs line 447 - RemoveWithMessage()
    // Find and remove alert associated with this message opaque_ref
    
    Alert* alert = AlertManager::instance()->FindAlert([messageRef](Alert* a)
    {
        MessageAlert* msgAlert = dynamic_cast<MessageAlert*>(a);
        if (!msgAlert)
            return false;
        return msgAlert->GetOpaqueRef() == messageRef;
    });
    
    if (alert)
    {
        AlertManager::instance()->RemoveAlert(alert);
    }
}

void MessageAlert::parseMessageData()
{
    // Extract timestamp
    qint64 timestamp = this->m_messageData.value("timestamp").toLongLong();
    this->m_timestamp = QDateTime::fromSecsSinceEpoch(timestamp);
    
    // Extract priority (1-5, 1=highest)
    int priorityValue = this->m_messageData.value("priority").toInt();
    switch (priorityValue) {
        case 1: this->m_priority = AlertPriority::Priority1; break;
        case 2: this->m_priority = AlertPriority::Priority2; break;
        case 3: this->m_priority = AlertPriority::Priority3; break;
        case 4: this->m_priority = AlertPriority::Priority4; break;
        case 5: this->m_priority = AlertPriority::Priority5; break;
        default: this->m_priority = AlertPriority::Unknown; break;
    }
    
    this->m_name = this->GetMessageType();
}

QString MessageAlert::getXenObjectName() const
{
    QSharedPointer<XenObject> xenObject = this->GetXenObject();
    return xenObject ? xenObject->GetName() : QString();
}

QString MessageAlert::getManagementBondName() const
{
    // C# Reference: NetworkingHelper.GetCoordinatorManagementBond(connection).Name()
    QSharedPointer<Pool> pool = poolOfOne(this->m_connection);
    QSharedPointer<Host> coordinator = pool ? pool->GetMasterHost() : QSharedPointer<Host>();
    if (coordinator)
    {
        for (const QSharedPointer<PIF>& pif : coordinator->GetPIFs())
        {
            // A bond's name is the name of its master PIF
            if (pif && pif->IsManagementInterface())
                return pif->IsBondMaster() ? pif->GetName() : tr("Unknown");
        }
    }
    return tr("Unknown");
}

QString MessageAlert::extractMultipathCurrentState(const QString& format) const
{
    /* C# Reference: MessageAlert.extractMultipathCurrentState()
     * message body format - if this changes you need to alter this method
     *
     * Unhealthy paths:
     * [20090511T16:29:22Z] host=foo; host-name=bar; pbd=whiz; scsi_id=pop; current=1; max=2
     * ...
     * Events received during the last 120 seconds:
     * [20090511T16:29:22Z] host=foo; host-name=bar; pbd=whiz; scsi_id=pop; current=1; max=2
     * ...
     */
    static const QRegularExpression multipathRegex(QStringLiteral("^.*host=(.*); host-name=.*; current=(\\d+); max=(\\d+)$"));

    const QStringList lines = this->GetMessageBody().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.isEmpty())
        return QString();

    const QString objectName = this->getXenObjectName();

    if (lines.first() == QLatin1String("Events received during the last 120 seconds:"))
    {
        // current state is healthy, past errors have been resolved
        const QString key = isPool(this->m_connection)
            ? QStringLiteral("Message.body-multipath_periodic_alert_healthy")
            : QStringLiteral("Message.body-multipath_periodic_alert_healthy_standalone");
        return fmt(FriendlyNames::getString(key), {objectName});
    }

    // Skip "unhealthy paths" line and record all lines that describe the current state
    QStringList currentState;
    for (int i = 1; i < lines.size() && lines.at(i).startsWith(QLatin1Char('[')); ++i)
        currentState.append(lines.at(i));

    if (currentState.size() == 1)
    {
        // Only one host currently unhealthy, describe its specific current/max paths
        const QRegularExpressionMatch m = multipathRegex.match(currentState.first());
        if (m.hasMatch())
            return fmt(format, {hostNameByUuid(this->m_connection, m.captured(1)), m.captured(2), m.captured(3)});
        return QString();
    }

    // Several hosts in pool unhealthy, list their names as a summary
    QStringList hostUuids;
    for (const QString& line : currentState)
    {
        const QRegularExpressionMatch m = multipathRegex.match(line);
        if (m.hasMatch() && !hostUuids.contains(m.captured(1)))
            hostUuids.append(m.captured(1));
    }

    QStringList hostNames;
    for (const QString& uuid : hostUuids)
        hostNames.append(QStringLiteral("'%1'").arg(hostNameByUuid(this->m_connection, uuid)));

    return fmt(FriendlyNames::getString(QStringLiteral("Message.body-multipath_periodic_alert_summary")),
               {objectName, hostNames.join(QStringLiteral(", "))});
}
