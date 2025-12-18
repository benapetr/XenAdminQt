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

#include "poolpropertiescommand.h"
#include <QDebug>
#include "../../dialogs/poolpropertiesdialog.h"
#include <QDebug>
#include "../../mainwindow.h"
#include <QDebug>
#include "xenlib.h"
#include <QDebug>
#include <QMessageBox>

PoolPropertiesCommand::PoolPropertiesCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool PoolPropertiesCommand::canRun() const
{
    QString poolRef = this->getSelectedPoolRef();
    return !poolRef.isEmpty() && this->mainWindow()->xenLib()->isConnected();
}

void PoolPropertiesCommand::run()
{
    QString poolRef = this->getSelectedPoolRef();

    if (poolRef.isEmpty())
        return;

    // Get connection from xenLib
    XenConnection* connection = this->xenLib()->getConnection();
    if (!connection)
    {
        qWarning() << "PoolPropertiesCommand: No connection available";
        QMessageBox::warning(this->mainWindow(), tr("No Connection"),
                             tr("Not connected to XenServer."));
        return;
    }

    PoolPropertiesDialog* dialog = new PoolPropertiesDialog(connection, poolRef, this->mainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);
    dialog->exec();
}

QString PoolPropertiesCommand::menuText() const
{
    return "Properties";
}

QString PoolPropertiesCommand::getSelectedPoolRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "pool")
        return QString();

    return this->getSelectedObjectRef();
}
