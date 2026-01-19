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

#include "vbd.h"
#include "vm.h"
#include "vdi.h"
#include "xencache.h"

VBD::VBD(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

QString VBD::GetVMRef() const
{
    return this->stringProperty("VM");
}

QSharedPointer<VM> VBD::GetVM()
{
    if (!this->GetCache())
        return QSharedPointer<VM>();
    return this->GetCache()->ResolveObject<VM>("vm", this->GetVMRef());
}

QSharedPointer<VDI> VBD::GetVDI()
{
    if (!this->GetCache())
        return QSharedPointer<VDI>();
    return this->GetCache()->ResolveObject<VDI>("vdi", this->GetVDIRef());
}

bool VBD::IsOwner() const
{
    return this->GetOtherConfig().contains("owner");
}

QString VBD::GetVDIRef() const
{
    QString vdi = this->stringProperty("VDI");
    // Return empty string if VDI is NULL reference (CD drive with no disc)
    if (vdi == "OpaqueRef:NULL")
    {
        return QString();
    }
    return vdi;
}

QString VBD::GetDevice() const
{
    return this->stringProperty("device");
}

QString VBD::GetUserdevice() const
{
    return this->stringProperty("userdevice");
}

bool VBD::IsBootable() const
{
    return this->boolProperty("bootable");
}

QString VBD::GetMode() const
{
    return this->stringProperty("mode");
}

bool VBD::IsReadOnly() const
{
    return this->GetMode() == "RO";
}

QString VBD::GetType() const
{
    return this->stringProperty("type");
}

bool VBD::IsCD() const
{
    return this->GetType() == "CD";
}

bool VBD::IsFloppyDrive() const
{
    // TODO: If we get floppy disk support, extend the enum and fix this check
    // For now, return false as XenAPI doesn't have floppy disk support yet
    // This matches the C# implementation
    return false;
}

bool VBD::Unpluggable() const
{
    return this->boolProperty("unpluggable");
}

bool VBD::CurrentlyAttached() const
{
    return this->boolProperty("currently_attached");
}

bool VBD::Empty() const
{
    return this->boolProperty("empty");
}

QStringList VBD::AllowedOperations() const
{
    return this->stringListProperty("allowed_operations");
}

bool VBD::CanPlug() const
{
    return this->AllowedOperations().contains("plug");
}

bool VBD::CanUnplug() const
{
    return this->AllowedOperations().contains("unplug");
}

QString VBD::Description() const
{
    QString typeStr = this->IsCD() ? "CD Drive" : "Disk";
    QString deviceNum = this->GetUserdevice();
    QString deviceName = this->GetDevice();

    if (deviceName.isEmpty())
    {
        return QString("%1 %2").arg(typeStr, deviceNum);
    } else
    {
        return QString("%1 %2 (%3)").arg(typeStr, deviceNum, deviceName);
    }
}

QVariantMap VBD::CurrentOperations() const
{
    return this->property("current_operations").toMap();
}

bool VBD::StorageLock() const
{
    return this->boolProperty("storage_lock", false);
}

qint64 VBD::StatusCode() const
{
    return this->intProperty("status_code", 0);
}

QString VBD::StatusDetail() const
{
    return this->stringProperty("status_detail");
}

QVariantMap VBD::RuntimeProperties() const
{
    return this->property("runtime_properties").toMap();
}

QString VBD::QosAlgorithmType() const
{
    return this->stringProperty("qos_algorithm_type");
}

QVariantMap VBD::QosAlgorithmParams() const
{
    return this->property("qos_algorithm_params").toMap();
}

QStringList VBD::QosSupportedAlgorithms() const
{
    return this->stringListProperty("qos_supported_algorithms");
}

QString VBD::MetricsRef() const
{
    return this->stringProperty("metrics");
}

int VBD::GetIoNice() const
{
    QVariantMap qosParams = this->QosAlgorithmParams();
    if (qosParams.contains("class"))
    {
        return qosParams["class"].toInt();
    }
    return 0;
}

QString VBD::GetObjectType() const
{
    return "vbd";
}
