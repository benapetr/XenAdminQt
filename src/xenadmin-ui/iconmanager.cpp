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
#include "xenlib/xen/sr.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xencache.h"
#include <QPainter>
#include <QDebug>

IconManager::IconManager()
{
    // Load static icons from resources
    this->m_connectedIcon = QIcon(":/tree-icons/host.png");
    this->m_disconnectedIcon = QIcon(":/tree-icons/host_disconnected.png");
    this->m_connectingIcon = this->createStatusIcon(QColor(255, 165, 0)); // Orange for connecting
}

IconManager& IconManager::instance()
{
    static IconManager instance;
    return instance;
}

namespace
{
    XenObjectType iconTypeFromString(const QString& objectType)
    {
        const QString normalized = objectType.toLower();
        if (normalized == "vm" || normalized == "vms")
            return XenObjectType::VM;
        if (normalized == "host" || normalized == "hosts")
            return XenObjectType::Host;
        if (normalized == "pool" || normalized == "pools")
            return XenObjectType::Pool;
        if (normalized == "sr" || normalized == "srs")
            return XenObjectType::SR;
        if (normalized == "network" || normalized == "networks")
            return XenObjectType::Network;
        return XenObjectType::Null;
    }
}

QIcon IconManager::GetIconForObject(const QString& objectType, const QVariantMap& objectData) const
{
    return this->GetIconForObject(iconTypeFromString(objectType), objectData);
}

QIcon IconManager::GetIconForObject(XenObjectType objectType, const QVariantMap& objectData) const
{
    switch (objectType)
    {
        case XenObjectType::VM:
            return this->GetIconForVM(objectData);
        case XenObjectType::Host:
            return QIcon();
            //return this->GetIconForHost(objectData);
        case XenObjectType::Pool:
            return this->GetIconForPool(objectData);
        case XenObjectType::SR:
            return this->GetIconForSR(objectData);
        case XenObjectType::Network:
            return this->GetIconForNetwork(objectData);
        default:
            break;
    }

    return QIcon(); // Empty icon for unknown types
}

QIcon IconManager::GetIconForObject(const XenObject* object) const
{
    if (!object)
        return QIcon();

    XenObjectType objectType = object->GetObjectType();
    QVariantMap objectData = object->GetData();

    if (objectType == XenObjectType::Host)
    {
        const Host *host = dynamic_cast<const Host*>(object);
        return this->GetIconForHost(host);
    }

    if (objectType == XenObjectType::SR)
        return this->GetIconForSR(objectData, object->GetConnection());

    return this->GetIconForObject(objectType, objectData);
}

QIcon IconManager::GetIconForVM(const QVariantMap& vmData) const
{
    QString powerState = getVMPowerState(vmData);
    bool isTemplate = vmData.value("is_a_template", false).toBool();
    bool isSnapshot = vmData.value("is_a_snapshot", false).toBool();
    bool operationInProgress = isVMOperationInProgress(vmData);

    // Create cache key
    QString cacheKey = QString("vm_%1_%2_%3_%4")
                           .arg(powerState)
                           .arg(isTemplate ? "template" : "vm")
                           .arg(isSnapshot ? "snapshot" : "normal")
                           .arg(operationInProgress ? "busy" : "idle");

    // Check cache
    if (this->m_iconCache.contains(cacheKey))
    {
        return this->m_iconCache.value(cacheKey);
    }

    QIcon icon;

    // Templates (matching C# Icons.Template, Icons.TemplateUser)
    if (isTemplate)
    {
        bool isUserTemplate = !vmData.value("other_config").toMap().value("default_template", false).toBool();
        if (isUserTemplate)
        {
            icon = QIcon(":/tree-icons/template_user.png");
        } else
        {
            icon = QIcon(":/tree-icons/template.png");
        }
    }
    // Snapshots (matching C# Icons.SnapshotDisksOnly, Icons.SnapshotWithMem)
    else if (isSnapshot)
    {
        icon = QIcon(":/tree-icons/snapshot.png");
    }
    // VMs with operations in progress (matching C# Icons.VmStarting)
    else if (operationInProgress)
    {
        icon = QIcon(":/tree-icons/vm_starting.png");
    }
    // VMs by power state (matching C# Icons.VmRunning, VmStopped, VmSuspended)
    else if (powerState == "Running")
    {
        icon = QIcon(":/tree-icons/vm_running.png"); // Running VM (green)
    } else if (powerState == "Halted")
    {
        icon = QIcon(":/tree-icons/vm_stopped.png"); // Stopped VM (red)
    } else if (powerState == "Suspended")
    {
        icon = QIcon(":/tree-icons/vm_suspended.png"); // Suspended VM (orange)
    } else if (powerState == "Paused")
    {
        icon = QIcon(":/tree-icons/vm_paused.png"); // Paused VM (yellow)
    } else
    {
        icon = QIcon(":/tree-icons/vm_generic.png"); // Generic VM
    }

    // Cache the icon
    m_iconCache.insert(cacheKey, icon);

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

    m_iconCache.insert(cacheKey, icon);
    return icon;
}

QIcon IconManager::GetIconForPool(const QVariantMap& poolData) const
{
    Q_UNUSED(poolData);

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

QIcon IconManager::GetIconForSR(const QVariantMap& srData) const
{
    return this->GetIconForSR(srData, nullptr);
}

QIcon IconManager::GetIconForSR(const QVariantMap& srData, XenConnection* connection) const
{
    QString type = srData.value("type", "unknown").toString();
    bool shared = srData.value("shared", false).toBool();

    QString ref = srData.value("ref").toString();
    if (ref.isEmpty())
        ref = srData.value("opaqueRef").toString();
    if (ref.isEmpty())
        ref = srData.value("_ref").toString();

    bool isDefault = false;
    bool hasPbds = !srData.value("PBDs").toList().isEmpty();
    bool isHidden = false;
    bool isBroken = false;
    if (connection && connection->GetCache())
    {
        QSharedPointer<SR> srObj = connection->GetCache()->ResolveObject<SR>(ref);
        if (srObj)
        {
            hasPbds = srObj->HasPBDs();
            isBroken = srObj->IsDetached() || srObj->IsBroken() || !srObj->MultipathAOK();
        }

        const QVariantMap otherConfig = srData.value("other_config").toMap();
        const QString hiddenFlag = otherConfig.value("hide_from_xencenter").toString().toLower();
        isHidden = hiddenFlag == "true";

        QStringList poolRefs = connection->GetCache()->GetAllRefs(XenObjectType::Pool);
        if (!poolRefs.isEmpty())
        {
            QVariantMap poolData = connection->GetCache()->ResolveObjectData("pool", poolRefs.first());
            QString defaultRef = poolData.value("default_SR").toString();
            if (!defaultRef.isEmpty() && defaultRef == ref)
                isDefault = true;
        }
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

    QString cacheKey = "network_default";

    if (m_iconCache.contains(cacheKey))
    {
        return m_iconCache.value(cacheKey);
    }

    QIcon icon = QIcon(":/icons/network-16.png");
    if (icon.isNull())
    {
        icon = createTextIcon("N", QColor(50, 100, 200)); // Blue for network
    }

    m_iconCache.insert(cacheKey, icon);
    return icon;
}

QIcon IconManager::GetIconForPIF(const PIF* pif)
{
    if (!pif)
        return QIcon();

    bool isPrimary = pif->IsPrimaryManagementInterface();
    QString cacheKey = isPrimary ? "pif_primary" : "pif_secondary";

    if (m_iconCache.contains(cacheKey))
        return m_iconCache.value(cacheKey);

    QIcon icon = QIcon(":/icons/management-interface-16.png");
    if (icon.isNull())
        icon = createTextIcon("M", QColor(70, 110, 160));

    m_iconCache.insert(cacheKey, icon);
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

QString IconManager::getVMPowerState(const QVariantMap& vmData) const
{
    return vmData.value("power_state", "unknown").toString();
}

bool IconManager::isVMOperationInProgress(const QVariantMap& vmData) const
{
    // Check for current_operations
    QVariantMap operations = vmData.value("current_operations", QVariantMap()).toMap();
    return !operations.isEmpty();
}
