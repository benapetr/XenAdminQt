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

#include "poolremoveservermenu.h"
#include "removehostfrompoolcommand.h"
#include "../../mainwindow.h"
#include "xenadmin-ui/iconmanager.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/xenobject.h"
#include <QTreeWidget>
#include <algorithm>

PoolRemoveServerMenu::PoolRemoveServerMenu(MainWindow* mainWindow, QWidget* parent) : QMenu(parent), mainWindow_(mainWindow)
{
    this->setTitle(tr("Remove Server"));
    connect(this, &QMenu::aboutToShow, this, &PoolRemoveServerMenu::onAboutToShow);
}

bool PoolRemoveServerMenu::CanRun() const
{
    QSharedPointer<Pool> pool = this->getSelectedPool();
    if (!pool)
        return false;

    QList<QSharedPointer<Host>> hosts = pool->GetHosts();
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (RemoveHostFromPoolCommand::CanRunForHost(host))
            return true;
    }

    return false;
}

void PoolRemoveServerMenu::onAboutToShow()
{
    this->clear();

    QSharedPointer<Pool> selectedPool = this->getSelectedPool();
    if (!selectedPool)
        return;

    QList<QSharedPointer<Host>> hosts = selectedPool->GetHosts();
    std::sort(hosts.begin(), hosts.end(), [](const QSharedPointer<Host>& a, const QSharedPointer<Host>& b) {
        return a && b ? a->GetName() < b->GetName() : static_cast<bool>(a);
    });

    for (const QSharedPointer<Host>& host : hosts)
    {
        if (!host || !host->IsValid())
            continue;

        QString hostName = host->GetName();
        if (hostName.length() > 50)
            hostName = hostName.left(47) + "...";
        hostName.replace("&", "&&");

        QAction* action = this->addAction(IconManager::instance().GetIconForHost(host.data()), hostName);
        action->setEnabled(RemoveHostFromPoolCommand::CanRunForHost(host));

        QSharedPointer<Host> hostCopy = host;
        connect(action, &QAction::triggered, this, [hostCopy]() {
            RemoveHostFromPoolCommand cmd(MainWindow::instance(), hostCopy);
            cmd.Run();
        });
    }
}

QSharedPointer<Pool> PoolRemoveServerMenu::getSelectedPool() const
{
    if (!this->mainWindow_)
        return QSharedPointer<Pool>();

    QTreeWidget* tree = this->mainWindow_->GetServerTreeWidget();
    if (!tree)
        return QSharedPointer<Pool>();

    QTreeWidgetItem* item = tree->currentItem();
    if (!item)
        return QSharedPointer<Pool>();

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.canConvert<QSharedPointer<XenObject>>())
        return QSharedPointer<Pool>();

    QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
    if (!obj || !obj->IsValid())
        return QSharedPointer<Pool>();

    if (obj->GetObjectType() == XenObjectType::Pool)
        return qSharedPointerDynamicCast<Pool>(obj);

    if (obj->GetObjectType() == XenObjectType::Host)
    {
        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(obj);
        return host ? host->GetPool() : QSharedPointer<Pool>();
    }

    return QSharedPointer<Pool>();
}
