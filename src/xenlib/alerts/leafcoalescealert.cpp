/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "leafcoalescealert.h"
#include "../xen/vdi.h"
#include "../xen/vbd.h"
#include "../xen/vm.h"

using namespace XenLib;

LeafCoalesceAlert::LeafCoalesceAlert(XenConnection* connection, const QVariantMap& messageData)
    : MessageAlert(connection, messageData)
{
}

QString LeafCoalesceAlert::vdiName() const
{
    // C#: _vdi?.Name() ?? Message.obj_uuid
    QSharedPointer<VDI> vdi = qSharedPointerDynamicCast<VDI>(this->GetXenObject());
    return vdi ? vdi->GetName() : this->GetObjUUID();
}

QString LeafCoalesceAlert::vmName() const
{
    // C#: the VM of the first resolvable VBD of the VDI, or Messages.UNKNOWN
    QSharedPointer<VDI> vdi = qSharedPointerDynamicCast<VDI>(this->GetXenObject());
    if (vdi)
    {
        for (const QSharedPointer<VBD>& vbd : vdi->GetVBDs())
        {
            QSharedPointer<VM> vm = vbd ? vbd->GetVM() : QSharedPointer<VM>();
            if (vm)
                return vm->GetName();
        }
    }
    return tr("Unknown");
}

QString LeafCoalesceAlert::GetTitle() const
{
    const QString type = this->GetMessageType();
    if (type == QLatin1String("LEAF_COALESCE_START_MESSAGE"))
        return tr("Data merge for VM %1 has started.").arg(this->vmName());
    if (type == QLatin1String("LEAF_COALESCE_COMPLETED"))
        return tr("Data merge for VM %1 has completed.").arg(this->vmName());
    if (type == QLatin1String("LEAF_COALESCE_FAILED"))
        return tr("Data merge for VM %1 has failed.").arg(this->vmName());
    return MessageAlert::GetTitle();
}

QString LeafCoalesceAlert::GetDescription() const
{
    const QString type = this->GetMessageType();
    if (type == QLatin1String("LEAF_COALESCE_START_MESSAGE"))
        return tr("Data merge for VDI '%1' of VM '%2' has started, performance may be reduced.").arg(this->vdiName(), this->vmName());
    if (type == QLatin1String("LEAF_COALESCE_COMPLETED"))
        return tr("Data merge for VDI '%1' of VM '%2' has completed.").arg(this->vdiName(), this->vmName());
    if (type == QLatin1String("LEAF_COALESCE_FAILED"))
        return tr("Data merge for VDI '%1' of VM '%2' has failed.").arg(this->vdiName(), this->vmName());
    return MessageAlert::GetDescription();
}
