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

#include "vmappliance.h"
#include "vm.h"
#include "sr.h"
#include "network/connection.h"
#include "../xencache.h"
#include <QSet>

VMAppliance::VMAppliance(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}


QStringList VMAppliance::VMRefs() const
{
    return this->stringListProperty("VMs");
}

QStringList VMAppliance::AllowedOperations() const
{
    return this->stringListProperty("allowed_operations");
}

QStringList VMAppliance::GetFateSharingVMs() const
{
    if (!this->GetConnection())
        return QStringList();
    
    XenCache* cache = this->GetConnection()->GetCache();
    if (!cache)
        return QStringList();
    
    // Get all VMs not in this appliance
    QStringList allVmRefs = cache->GetAllRefs(XenObjectType::VM);
    QStringList thisApplianceVmRefs = this->VMRefs();
    QSet<QString> vmsNotInCurApp;
    
    for (const QString& vmRef : allVmRefs)
    {
        QVariantMap vmData = cache->ResolveObjectData(XenObjectType::VM, vmRef);
        QString vmApplianceRef = vmData.value("appliance").toString();
        
        if (vmApplianceRef != this->OpaqueRef())
        {
            vmsNotInCurApp.insert(vmRef);
        }
    }
    
    QStringList fateSharingVms;
    
    // For each VM in this appliance, find VMs outside the appliance that share storage
    for (const QString& thisVmRef : thisApplianceVmRefs)
    {
        QVariantMap thisVmData = cache->ResolveObjectData(XenObjectType::VM, thisVmRef);
        if (thisVmData.isEmpty())
            continue;
        
        // Get SRs used by this VM (via VBDs)
        QSet<QString> thisVmSRs;
        QVariantList vbdRefs = thisVmData.value("VBDs").toList();
        for (const QVariant& vbdRefVar : vbdRefs)
        {
            QString vbdRef = vbdRefVar.toString();
            QVariantMap vbdData = cache->ResolveObjectData(XenObjectType::VBD, vbdRef);
            QString vdiRef = vbdData.value("VDI").toString();
            
            if (!vdiRef.isEmpty() && vdiRef != "OpaqueRef:NULL")
            {
                QVariantMap vdiData = cache->ResolveObjectData(XenObjectType::VDI, vdiRef);
                QString srRef = vdiData.value("SR").toString();
                if (!srRef.isEmpty())
                {
                    thisVmSRs.insert(srRef);
                }
            }
        }
        
        // Check each VM not in current appliance for SR intersection
        for (const QString& otherVmRef : vmsNotInCurApp)
        {
            if (fateSharingVms.contains(otherVmRef))
                continue;
            
            QVariantMap otherVmData = cache->ResolveObjectData(XenObjectType::VM, otherVmRef);
            if (otherVmData.isEmpty())
                continue;
            
            // Check if it's a real VM (not template/snapshot) and not halted
            bool isTemplate = otherVmData.value("is_a_template").toBool();
            bool isSnapshot = otherVmData.value("is_a_snapshot").toBool();
            QString powerState = otherVmData.value("power_state").toString();
            
            if (isTemplate || isSnapshot || powerState == "Halted")
                continue;
            
            // Get SRs used by other VM
            QSet<QString> otherVmSRs;
            QVariantList otherVbdRefs = otherVmData.value("VBDs").toList();
            for (const QVariant& vbdRefVar : otherVbdRefs)
            {
                QString vbdRef = vbdRefVar.toString();
                QVariantMap vbdData = cache->ResolveObjectData(XenObjectType::VBD, vbdRef);
                QString vdiRef = vbdData.value("VDI").toString();
                
                if (!vdiRef.isEmpty() && vdiRef != "OpaqueRef:NULL")
                {
                    QVariantMap vdiData = cache->ResolveObjectData(XenObjectType::VDI, vdiRef);
                    QString srRef = vdiData.value("SR").toString();
                    if (!srRef.isEmpty())
                    {
                        otherVmSRs.insert(srRef);
                    }
                }
            }
            
            // Check for SR intersection
            if (thisVmSRs.intersects(otherVmSRs))
            {
                fateSharingVms.append(otherVmRef);
            }
        }
    }
    
    return fateSharingVms;
}

bool VMAppliance::IsRunning() const
{
    if (!this->GetConnection())
        return false;
    
    XenCache* cache = this->GetConnection()->GetCache();
    if (!cache)
        return false;
    
    QStringList vmRefs = this->VMRefs();
    
    for (const QString& vmRef : vmRefs)
    {
        QVariantMap vmData = cache->ResolveObjectData(XenObjectType::VM, vmRef);
        if (vmData.isEmpty())
            continue;
        
        QString powerState = vmData.value("power_state").toString();
        
        if (powerState == "Running" || powerState == "Paused" || powerState == "Suspended")
        {
            return true;
        }
    }
    
    return false;
}
