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

#include "networkpropertiescommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/networkpropertiesdialog.h"
#include "xenlib.h"
#include "xen/xenobject.h"
#include <QTreeWidgetItem>

NetworkPropertiesCommand::NetworkPropertiesCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

NetworkPropertiesCommand::~NetworkPropertiesCommand()
{
}

bool NetworkPropertiesCommand::canRun() const
{
    QString networkUuid = this->getSelectedNetworkUuid();
    return !networkUuid.isEmpty() && this->mainWindow()->xenLib()->isConnected();
}

void NetworkPropertiesCommand::run()
{
    QString networkUuid = this->getSelectedNetworkUuid();
    if (networkUuid.isEmpty())
        return;

    NetworkPropertiesDialog dialog(this->mainWindow()->xenLib(), networkUuid, this->mainWindow());
    dialog.exec();
}

QString NetworkPropertiesCommand::menuText() const
{
    return "Properties";
}

QString NetworkPropertiesCommand::getSelectedNetworkUuid() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    // Check if this is a Network item
    QVariant data = item->data(0, Qt::UserRole);
    QSharedPointer<XenObject> obj = data.value<QSharedPointer<XenObject>>();
    if (!obj || obj->objectType() != "network")
        return QString();

    return obj->uuid();
    return QString();
}
