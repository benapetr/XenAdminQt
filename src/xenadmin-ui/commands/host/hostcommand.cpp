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
#include "xenlib/xen/xenobject.h"
#include "../../mainwindow.h"
#include <QTreeWidget>

HostCommand::HostCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

HostCommand::HostCommand(QList<QSharedPointer<Host> > hosts, MainWindow *mainWindow, QObject *parent) : Command(mainWindow, parent)
{
    this->m_hosts = hosts;
}

QSharedPointer<Host> HostCommand::getSelectedHost() const
{
    const QList<QSharedPointer<Host>> hosts = this->getHosts();
    if (!hosts.isEmpty())
        return hosts.first();

    return qSharedPointerCast<Host>(this->GetObject());
}

QString HostCommand::getSelectedHostRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "host")
        return QString();

    return this->getSelectedObjectRef();
}

QString HostCommand::getSelectedHostName() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "host")
        return QString();

    return item->text(0);
}

QList<QSharedPointer<Host>> HostCommand::getHosts() const
{
    if (!this->m_hosts.isEmpty())
        return this->m_hosts;

    QList<QSharedPointer<Host>> hosts;
    if (!this->mainWindow())
        return hosts;

    QTreeWidget* treeWidget = this->mainWindow()->GetServerTreeWidget();
    if (!treeWidget)
        return hosts;

    const QList<QTreeWidgetItem*> selectedItems = treeWidget->selectedItems();
    for (QTreeWidgetItem* item : selectedItems)
    {
        if (!item)
            continue;

        QVariant data = item->data(0, Qt::UserRole);
        if (!data.canConvert<QSharedPointer<XenObject>>())
            continue;

        QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
        if (!obj || obj->GetObjectType() != "host")
            continue;

        QSharedPointer<Host> host = qSharedPointerCast<Host>(obj);
        if (host)
            hosts.append(host);
    }

    return hosts;
}
