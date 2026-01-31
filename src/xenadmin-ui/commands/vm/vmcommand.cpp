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

#include "vmcommand.h"
#include "xenlib/xen/vm.h"

VMCommand::VMCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

QSharedPointer<VM> VMCommand::getVM() const
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    if (!vms.isEmpty())
        return vms.first();

    QSharedPointer<XenObject> xo = this->GetObject();
    if (!xo || xo->GetObjectType() != XenObjectType::VM)
        return QSharedPointer<VM>();

    return qSharedPointerDynamicCast<VM>(this->GetObject());
}

QList<QSharedPointer<VM>> VMCommand::getVMs() const
{
    if (!this->m_overrideVM.isNull())
        return { this->m_overrideVM };

    QList<QSharedPointer<VM>> vms;
    const QList<QSharedPointer<XenObject>> objects = this->getSelectedObjects();
    for (const QSharedPointer<XenObject>& obj : objects)
    {
        if (!obj || obj->GetObjectType() != XenObjectType::VM)
            continue;

        QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(obj);
        if (vm)
            vms.append(vm);
    }

    if (!vms.isEmpty())
        return vms;

    QSharedPointer<XenObject> xo = this->GetObject();
    if (!xo || xo->GetObjectType() != XenObjectType::VM)
        return {};

    QSharedPointer<VM> vm = qSharedPointerDynamicCast<VM>(xo);
    if (vm)
        return { vm };

    return {};
}

QString VMCommand::getSelectedVMRef() const
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    return vms.isEmpty() ? QString() : vms.first()->OpaqueRef();
}

QString VMCommand::getSelectedVMName() const
{
    const QList<QSharedPointer<VM>> vms = this->getVMs();
    return vms.isEmpty() ? QString() : vms.first()->GetName();
}
