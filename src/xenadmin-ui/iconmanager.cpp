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

#include "iconmanager.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/network.h"
#include "xenlib/xencache.h"
#include <QPainter>
#include <QDebug>

IconManager::IconManager()
{
    // Load static icons from resources
    this->m_connectedIcon = QIcon(":/tree-icons/host.png");
    this->m_disconnectedIcon = QIcon(":/tree-icons/host_disconnected.png");
    this->m_connectingIcon = this->createStatusIcon(QColor(255, 165, 0)); // Orange for connecting
    this->m_successIcon = QIcon(":/icons/tick_16.png");
    this->m_errorIcon = QIcon(":/icons/error_16.png");
    this->m_cancelledIcon = QIcon(":/icons/cancelled_action_16.png");
    this->m_inProgressIcon = QIcon(":/tree-icons/spinning_frame_0.png");
    this->m_notStartedIcon = QIcon(":/icons/empty_icon.png");
}

IconManager& IconManager::instance()
{
    static IconManager instance;
    return instance;
}

QIcon IconManager::GetIconForObject(XenConnection* connection, XenObjectType objectType, const QString& objectRef) const
{
    if (connection && connection->GetCache() && !objectRef.isEmpty())
    {
        QSharedPointer<XenObject> object = connection->GetCache()->ResolveObject(objectType, objectRef);
        if (object)
            return this->GetIconForObject(object);
    }

    switch (objectType)
    {
        case XenObjectType::VM:
            return QIcon(":/tree-icons/vm_generic.png");
        case XenObjectType::Host:
            return QIcon();
        case XenObjectType::Pool:
            return this->GetIconForPool(static_cast<const Pool*>(nullptr));
        case XenObjectType::SR:
            return QIcon(":/tree-icons/storage.png");
        case XenObjectType::Network:
            return this->GetIconForNetwork(static_cast<const Network*>(nullptr));
        case XenObjectType::VDI:
            return QIcon(":/tree-icons/storage.png");
        case XenObjectType::VMAppliance:
            return QIcon(":/tree-icons/vm_generic.png");
        default:
            break;
    }

    return QIcon(); // Empty icon for unknown types
}

QIcon IconManager::GetIconForObject(const XenObject* object) const
{
    if (!object)
        return QIcon();

    if (const VM* vm = dynamic_cast<const VM*>(object))
        return this->GetIconForVM(vm);
    if (const Host* host = dynamic_cast<const Host*>(object))
        return this->GetIconForHost(host);
    if (const Pool* pool = dynamic_cast<const Pool*>(object))
        return this->GetIconForPool(pool);
    if (const SR* sr = dynamic_cast<const SR*>(object))
        return this->GetIconForSR(sr);
    if (const Network* network = dynamic_cast<const Network*>(object))
        return this->GetIconForNetwork(network);
    if (const VDI* vdi = dynamic_cast<const VDI*>(object))
        return this->GetIconForVDI(vdi);
    if (object->GetObjectType() == XenObjectType::VMAppliance)
        return QIcon(":/tree-icons/vm_generic.png");

    return this->GetIconForObject(object->GetConnection(), object->GetObjectType(), object->OpaqueRef());
}

QIcon IconManager::GetIconForObject(QSharedPointer<XenObject> object) const
{
    return this->GetIconForObject(object.data());
}

QIcon IconManager::GetIconForVM(const VM* vm) const
{
    if (!vm)
        return QIcon();

    const QString powerState = vm->GetPowerState();
    const bool isTemplate = vm->IsTemplate();
    const bool isSnapshot = vm->IsSnapshot();
    const bool operationInProgress = !vm->CurrentOperations().isEmpty();

    QString cacheKey = QString("vm_%1_%2_%3_%4")
                           .arg(powerState)
                           .arg(isTemplate ? "template" : "vm")
                           .arg(isSnapshot ? "snapshot" : "normal")
                           .arg(operationInProgress ? "busy" : "idle");

    if (this->m_iconCache.contains(cacheKey))
        return this->m_iconCache.value(cacheKey);

    QIcon icon;

    if (isTemplate)
    {
        icon = vm->IsDefaultTemplate()
            ? QIcon(":/tree-icons/template.png")
            : QIcon(":/tree-icons/template_user.png");
    } else if (isSnapshot)
    {
        icon = QIcon(":/tree-icons/snapshot.png");
    } else if (operationInProgress)
    {
        icon = QIcon(":/tree-icons/vm_starting.png");
    } else if (vm->IsRunning())
    {
        icon = QIcon(":/tree-icons/vm_running.png");
    } else if (vm->IsHalted())
    {
        icon = QIcon(":/tree-icons/vm_stopped.png");
    } else if (vm->IsSuspended())
    {
        icon = QIcon(":/tree-icons/vm_suspended.png");
    } else if (vm->IsPaused())
    {
        icon = QIcon(":/tree-icons/vm_paused.png");
    } else
    {
        icon = QIcon(":/tree-icons/vm_generic.png");
    }

    this->m_iconCache.insert(cacheKey, icon);
    return icon;
}

QIcon IconManager::GetIconForHost(const Host *host) const
{
    if (!host)
        return QIcon();

    // C# pattern from Images.cs GetIconFor(Host host):
    // 1. Resolve host_metrics: Host_metrics metrics = host.Connection.Resolve(host.metrics);
    // 2. Check if live: if (metrics != null && metrics.live) { ... }
    // 3. Check enabled: if (host.enabled) for maintenance mode

    bool enabled = host->IsEnabled();
    bool isDisconnected = !host->IsConnected();
    bool isLive = host->IsLive();

    QString cacheKey = QString("host_%1_%2_%3")
                           .arg(enabled ? "enabled" : "disabled")
                           .arg(isLive ? "live" : "notlive")
                           .arg(isDisconnected ? "disconnected" : "connected");

    if (this->m_iconCache.contains(cacheKey))
    {
        return this->m_iconCache.value(cacheKey);
    }

    QIcon icon;

    // C# Icons.cs GetIconFor(Host host) logic:
    // 1. If metrics is live:
    //    - Various checks (expired license, crash dumps, evacuate, older than master, etc.)
    //    - Default: Icons.HostConnected
    // 2. If connection.InProgress && !connection.IsConnected: Icons.HostConnecting
    // 3. Else: Icons.HostDisconnected

    if (isDisconnected)
    {
        icon = QIcon(":/tree-icons/host_disconnected.png");
    } else if (isLive)
    {
        // Host is live
        if (!enabled)
        {
            // host.enabled = false means maintenance/evacuate mode
            // C#: if (host.current_operations.ContainsValue(host_allowed_operations.evacuate) || !host.enabled)
            //     return Icons.HostEvacuate;
            icon = QIcon(":/tree-icons/host_maintenance.png"); // Maintenance mode (if exists)
            if (icon.isNull())
            {
                icon = QIcon(":/tree-icons/host_disconnected.png"); // Fallback
            }
        } else
        {
            // Host is live and enabled
            icon = QIcon(":/tree-icons/host.png"); // Connected/live host
        }
    } else
    {
        // Host is not live (disconnected)
        icon = QIcon(":/tree-icons/host_disconnected.png");
    }

    this->m_iconCache.insert(cacheKey, icon);
    return icon;
}

QIcon IconManager::GetIconForPool(const Pool* pool) const
{
    Q_UNUSED(pool);

    QString cacheKey = "pool_connected";

    if (this->m_iconCache.contains(cacheKey))
    {
        return this->m_iconCache.value(cacheKey);
    }

    // Matching C# Icons.PoolConnected
    QIcon icon = QIcon(":/tree-icons/pool_connected.png");
    this->m_iconCache.insert(cacheKey, icon);
    return icon;
}

QIcon IconManager::GetIconForPool(const QVariantMap& poolData) const
{
    Q_UNUSED(poolData);
    return this->GetIconForPool(static_cast<const Pool*>(nullptr));
}

QIcon IconManager::GetIconForSR(const SR* sr) const
{
    if (!sr)
        return QIcon();

    const QString type = sr->GetType();
    const bool shared = sr->IsShared();
    const bool hasPbds = sr->HasPBDs();
    const bool isHidden = sr->IsHidden();
    const bool isBroken = sr->IsDetached() || sr->IsBroken() || !sr->MultipathAOK();

    bool isDefault = false;
    XenConnection* connection = sr->GetConnection();
    if (connection && connection->GetCache())
    {
        QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
        if (pool && !pool->GetDefaultSRRef().isEmpty() && pool->GetDefaultSRRef() == sr->OpaqueRef())
            isDefault = true;
    }

    QString cacheKey = QString("sr_%1_%2_%3_%4_%5")
                           .arg(type)
                           .arg(shared ? "shared" : "local")
                           .arg(isDefault ? "default" : "regular")
                           .arg(hasPbds ? "attached" : "detached")
                           .arg(isBroken ? "broken" : "ok");

    if (this->m_iconCache.contains(cacheKey))
        return this->m_iconCache.value(cacheKey);

    QIcon icon;
    if (!hasPbds || isHidden)
        icon = QIcon(":/tree-icons/storage_disabled.png");
    else if (isBroken)
        icon = QIcon(":/tree-icons/storage_broken.png");
    else if (isDefault)
        icon = QIcon(":/tree-icons/storage_default.png");
    else
        icon = QIcon(":/tree-icons/storage.png");

    this->m_iconCache.insert(cacheKey, icon);
    return icon;
}

QIcon IconManager::GetIconForSR(const QVariantMap& srData) const
{
    return this->GetIconForSR(srData, nullptr);
}

QIcon IconManager::GetIconForSR(const QVariantMap& srData, XenConnection* connection) const
{
    QString ref = srData.value("ref").toString();
    if (ref.isEmpty())
        ref = srData.value("opaqueRef").toString();
    if (ref.isEmpty())
        ref = srData.value("_ref").toString();

    QSharedPointer<SR> srObj;
    if (connection && connection->GetCache() && !ref.isEmpty())
        srObj = connection->GetCache()->ResolveObject<SR>(ref);

    const QString type = srObj ? srObj->GetType() : srData.value("type", "unknown").toString();
    const bool shared = srObj ? srObj->IsShared() : srData.value("shared", false).toBool();

    bool isDefault = false;
    bool hasPbds = srObj ? srObj->HasPBDs() : !srData.value("PBDs").toList().isEmpty();
    bool isHidden = false;
    bool isBroken = srObj ? (srObj->IsDetached() || srObj->IsBroken() || !srObj->MultipathAOK()) : false;

    if (srObj)
    {
        const QVariantMap otherConfig = srObj->GetOtherConfig();
        const QString hiddenFlag = otherConfig.value("hide_from_xencenter").toString().toLower();
        isHidden = hiddenFlag == "true";
    } else
    {
        const QVariantMap otherConfig = srData.value("other_config").toMap();
        const QString hiddenFlag = otherConfig.value("hide_from_xencenter").toString().toLower();
        isHidden = hiddenFlag == "true";
    }

    if (connection && connection->GetCache())
    {
        QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
        if (pool && !pool->GetDefaultSRRef().isEmpty() && pool->GetDefaultSRRef() == ref)
            isDefault = true;
    }

    QString cacheKey = QString("sr_%1_%2_%3_%4_%5")
                           .arg(type)
                           .arg(shared ? "shared" : "local")
                           .arg(isDefault ? "default" : "regular")
                           .arg(hasPbds ? "attached" : "detached")
                           .arg(isBroken ? "broken" : "ok");

    if (this->m_iconCache.contains(cacheKey))
    {
        return this->m_iconCache.value(cacheKey);
    }

    QIcon icon;
    if (!hasPbds || isHidden)
    {
        icon = QIcon(":/tree-icons/storage_disabled.png");
    } else if (isBroken)
    {
        icon = QIcon(":/tree-icons/storage_broken.png");
    } else if (isDefault)
    {
        icon = QIcon(":/tree-icons/storage_default.png");
    } else
    {
        icon = QIcon(":/tree-icons/storage.png");
    }

    this->m_iconCache.insert(cacheKey, icon);
    return icon;
}

QIcon IconManager::GetIconForNetwork(const QVariantMap& networkData) const
{
    Q_UNUSED(networkData);
    return this->GetIconForNetwork(static_cast<const Network*>(nullptr));
}

QIcon IconManager::GetIconForNetwork(const Network* network) const
{
    Q_UNUSED(network);

    QString cacheKey = "network_default";

    if (this->m_iconCache.contains(cacheKey))
    {
        return this->m_iconCache.value(cacheKey);
    }

    QIcon icon = QIcon(":/icons/network-16.png");
    if (icon.isNull())
    {
        icon = this->createTextIcon("N", QColor(50, 100, 200)); // Blue for network
    }

    this->m_iconCache.insert(cacheKey, icon);
    return icon;
}

QIcon IconManager::GetIconForVDI(const VDI* vdi) const
{
    if (!vdi)
        return QIcon();

    return vdi->IsSnapshot()
        ? QIcon(":/tree-icons/snapshot.png")
        : QIcon(":/tree-icons/storage.png");
}

QIcon IconManager::GetIconForPIF(const PIF* pif)
{
    if (!pif)
        return QIcon();

    bool isPrimary = pif->IsPrimaryManagementInterface();
    QString cacheKey = isPrimary ? "pif_primary" : "pif_secondary";

    if (this->m_iconCache.contains(cacheKey))
        return this->m_iconCache.value(cacheKey);

    QIcon icon = QIcon(":/icons/management-interface-16.png");
    if (icon.isNull())
        icon = this->createTextIcon("M", QColor(70, 110, 160));

    this->m_iconCache.insert(cacheKey, icon);
    return icon;
}

QIcon IconManager::GetConnectedIcon() const
{
    return this->m_connectedIcon;
}

QIcon IconManager::GetDisconnectedIcon() const
{
    return this->m_disconnectedIcon;
}

QIcon IconManager::GetConnectingIcon() const
{
    return this->m_connectingIcon;
}

QIcon IconManager::GetSuccessIcon() const
{
    return this->m_successIcon;
}

QIcon IconManager::GetErrorIcon() const
{
    return this->m_errorIcon;
}

QIcon IconManager::GetCancelledIcon() const
{
    return this->m_cancelledIcon;
}

QIcon IconManager::GetInProgressIcon() const
{
    return this->m_inProgressIcon;
}

QIcon IconManager::GetNotStartedIcon() const
{
    return this->m_notStartedIcon;
}

QIcon IconManager::GetEventIcon(EventIconType type) const
{
    switch (type)
    {
        case EventIconType::VmStarted:
            return QIcon(QStringLiteral(":/icons/start_vm.png"));
        case EventIconType::VmShutdown:
            return QIcon(QStringLiteral(":/icons/shutdown.png"));
        case EventIconType::VmRebooted:
            return QIcon(QStringLiteral(":/icons/reboot.png"));
        case EventIconType::VmResumed:
            return QIcon(QStringLiteral(":/icons/resume.png"));
        case EventIconType::VmSuspended:
            return QIcon(QStringLiteral(":/icons/suspend.png"));
        case EventIconType::VmCrashed:
            return this->GetErrorIcon();
        case EventIconType::VmCloned:
            return this->GetSuccessIcon();
        case EventIconType::Unknown:
        default:
            return QIcon();
    }
}

// Helper methods
QIcon IconManager::createStatusIcon(const QColor& color, const QString& symbol) const
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw filled circle
    painter.setBrush(color);
    painter.setPen(color.darker(120));
    painter.drawEllipse(1, 1, 14, 14);

    // Draw symbol if provided
    if (!symbol.isEmpty())
    {
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPixelSize(10);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, symbol);
    }

    return QIcon(pixmap);
}

QIcon IconManager::createTextIcon(const QString& text, const QColor& bgColor) const
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // Draw rounded rectangle background
    painter.setBrush(bgColor);
    painter.setPen(bgColor.darker(120));
    painter.drawRoundedRect(0, 0, 16, 16, 3, 3);

    // Draw text
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(text.length() > 1 ? 7 : 10);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, text);

    return QIcon(pixmap);
}
