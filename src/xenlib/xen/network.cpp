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

#include "network.h"
#include "pif.h"
#include "vif.h"
#include "xencache.h"

Network::Network(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

QString Network::GetBridge() const
{
    return this->GetData().value("bridge").toString();
}

bool Network::IsManaged() const
{
    // Default to true (managed by xapi) if not specified
    return this->GetData().value("managed", true).toBool();
}

bool Network::IsAutomatic() const
{
    return this->GetData().value("other_config", QVariantMap()).toMap().value("automatic", "false").toString() == "true" ? true : false;
}

bool Network::IsBond() const
{
    XenCache* cache = this->GetCache();
    if (!cache)
        return false;

    const QStringList pifRefs = this->GetPIFRefs();
    for (const QString& pifRef : pifRefs)
    {
        QSharedPointer<PIF> pif = cache->ResolveObject<PIF>("pif", pifRef);
        if (pif && pif->IsValid() && pif->IsBondMaster())
            return true;
    }

    return false;
}

bool Network::IsMember() const
{
    XenCache* cache = this->GetCache();
    if (!cache)
        return false;

    const QStringList pifRefs = this->GetPIFRefs();
    for (const QString& pifRef : pifRefs)
    {
        QSharedPointer<PIF> pif = cache->ResolveObject<PIF>("pif", pifRef);
        if (pif && pif->IsValid() && pif->IsBondMember())
            return true;
    }

    return false;
}

bool Network::IsGuestInstallerNetwork() const
{
    QVariantMap otherConfig = this->GetOtherConfig();
    const QString guestInstaller = otherConfig.value("is_guest_installer_network").toString().trimmed().toLower();
    return guestInstaller == "true" || guestInstaller == "1";
}

bool Network::Show(bool showHiddenObjects) const
{
    if (this->IsGuestInstallerNetwork() && !showHiddenObjects)
        return false;

    XenCache* cache = this->GetCache();
    if (cache)
    {
        const QStringList pifRefs = this->GetPIFRefs();
        for (const QString& pifRef : pifRefs)
        {
            QSharedPointer<PIF> pif = cache->ResolveObject<PIF>("pif", pifRef);
            if (pif && pif->IsValid() && !pif->Show(showHiddenObjects))
                return false;
        }
    }

    if (showHiddenObjects)
        return true;

    if (this->IsMember())
        return false;

    return !this->IsHidden();
}

qint64 Network::GetMTU() const
{
    return this->GetData().value("MTU", 1500).toLongLong();
}

QStringList Network::GetVIFRefs() const
{
    QVariantList vifs = this->GetData().value("VIFs").toList();
    QStringList result;
    for (const QVariant& vif : vifs)
    {
        result.append(vif.toString());
    }
    return result;
}

QStringList Network::GetPIFRefs() const
{
    QVariantList pifs = this->GetData().value("PIFs").toList();
    QStringList result;
    for (const QVariant& pif : pifs)
    {
        result.append(pif.toString());
    }
    return result;
}

QStringList Network::AllowedOperations() const
{
    QVariantList ops = this->GetData().value("allowed_operations").toList();
    QStringList result;
    for (const QVariant& op : ops)
    {
        result.append(op.toString());
    }
    return result;
}

QVariantMap Network::CurrentOperations() const
{
    return this->GetData().value("current_operations").toMap();
}

QVariantMap Network::GetBlobs() const
{
    return this->GetData().value("blobs").toMap();
}

QString Network::GetDefaultLockingMode() const
{
    return this->GetData().value("default_locking_mode", "unlocked").toString();
}

QVariantMap Network::GetAssignedIPs() const
{
    return this->GetData().value("assigned_ips").toMap();
}

QStringList Network::GetPurpose() const
{
    QVariantList purposeList = this->GetData().value("purpose").toList();
    QStringList result;
    for (const QVariant& p : purposeList)
    {
        result.append(p.toString());
    }
    return result;
}

QList<QSharedPointer<PIF>> Network::GetPIFs() const
{
    QList<QSharedPointer<PIF>> pifs;
    
    XenCache* cache = this->GetCache();
    if (!cache)
        return pifs;

    const QStringList pifRefs = this->GetPIFRefs();
    for (const QString& pifRef : pifRefs)
    {
        QSharedPointer<PIF> pif = cache->ResolveObject<PIF>("pif", pifRef);
        if (pif && pif->IsValid())
            pifs.append(pif);
    }

    return pifs;
}

QList<QSharedPointer<VIF>> Network::GetVIFs() const
{
    QList<QSharedPointer<VIF>> vifs;
    
    XenCache* cache = this->GetCache();
    if (!cache)
        return vifs;

    const QStringList vifRefs = this->GetVIFRefs();
    for (const QString& vifRef : vifRefs)
    {
        QSharedPointer<VIF> vif = cache->ResolveObject<VIF>("vif", vifRef);
        if (vif && vif->IsValid())
            vifs.append(vif);
    }

    return vifs;
}
