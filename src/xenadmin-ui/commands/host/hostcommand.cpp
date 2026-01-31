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

#include "hostcommand.h"
#include "xenlib/xen/host.h"
#include "../../mainwindow.h"
#include <QTreeWidget>

HostCommand::HostCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

QSharedPointer<Host> HostCommand::getSelectedHost() const
{
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    if (!hosts.isEmpty())
        return hosts.first();

    QSharedPointer<XenObject> xo = this->GetObject();
    if (!xo || xo->GetObjectType() != XenObjectType::Host)
        return QSharedPointer<Host>();

    return qSharedPointerDynamicCast<Host>(xo);
}

QString HostCommand::getSelectedHostRef() const
{
    QSharedPointer<Host> host = this->getSelectedHost();
    return host ? host->OpaqueRef() : QString();
}

QString HostCommand::getSelectedHostName() const
{
    QSharedPointer<Host> host = this->getSelectedHost();
    return host ? host->GetName() : QString();
}

QList<QSharedPointer<Host>> HostCommand::getHosts() const
{
    QList<QSharedPointer<Host>> hosts;
    const QList<QSharedPointer<XenObject>> objects = this->getSelectedObjects();
    for (const QSharedPointer<XenObject>& obj : objects)
    {
        if (!obj || obj->GetObjectType() != XenObjectType::Host)
            continue;

        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(obj);
        if (host)
            hosts.append(host);
    }

    if (!hosts.isEmpty())
        return hosts;

    QSharedPointer<XenObject> xo = this->GetObject();
    if (!xo || xo->GetObjectType() != XenObjectType::Host)
        return {};

    QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(xo);
    if (host)
        return { host };

    return {};
}
