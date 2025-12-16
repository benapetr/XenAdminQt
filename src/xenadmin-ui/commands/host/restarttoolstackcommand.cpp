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

#include "restarttoolstackcommand.h"
#include "../../mainwindow.h"
#include "xenlib.h"
#include "xen/api.h"
#include "xencache.h"
#include <QMessageBox>

RestartToolstackCommand::RestartToolstackCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool RestartToolstackCommand::canRun() const
{
    QString hostRef = this->getSelectedHostRef();
    if (hostRef.isEmpty())
        return false;

    // Can restart toolstack if host is live
    return this->isHostLive(hostRef);
}

void RestartToolstackCommand::run()
{
    QString hostRef = this->getSelectedHostRef();
    QString hostName = this->getSelectedHostName();

    if (hostRef.isEmpty() || hostName.isEmpty())
        return;

    // Show confirmation dialog
    int ret = QMessageBox::warning(this->mainWindow(), "Restart Toolstack",
                                   QString("Are you sure you want to restart the toolstack on '%1'?\n\n"
                                           "The management interface will restart. This may take a minute.")
                                       .arg(hostName),
                                   QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes)
    {
        this->mainWindow()->showStatusMessage(QString("Restarting toolstack on '%1'...").arg(hostName));

        // TODO: Need to add Host.async_restart_agent to XenAPI bindings
        // For now, show a not-implemented message
        QMessageBox::information(this->mainWindow(), "Not Implemented",
                                 "Restart Toolstack functionality will be implemented with XenAPI Host bindings.");

        /*
        bool success = this->mainWindow()->xenLib()->getAPI()->restartHostAgent(hostRef);
        if (success)
        {
            this->mainWindow()->showStatusMessage(QString("Toolstack restarted on '%1'").arg(hostName), 5000);
        } else
        {
            QMessageBox::warning(this->mainWindow(), "Restart Toolstack Failed",
                                 QString("Failed to restart toolstack on '%1'. Check the error log for details.").arg(hostName));
            this->mainWindow()->showStatusMessage("Toolstack restart failed", 5000);
        }
        */
    }
}

QString RestartToolstackCommand::menuText() const
{
    return "Restart Toolstack";
}

QString RestartToolstackCommand::getSelectedHostRef() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "host")
        return QString();

    return this->getSelectedObjectRef();
}

QString RestartToolstackCommand::getSelectedHostName() const
{
    QTreeWidgetItem* item = this->getSelectedItem();
    if (!item)
        return QString();

    QString objectType = this->getSelectedObjectType();
    if (objectType != "host")
        return QString();

    if (!this->mainWindow()->xenLib())
        return QString();

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return QString();

    QString hostRef = this->getSelectedObjectRef();
    QVariantMap hostData = cache->resolve("host", hostRef);
    return hostData.value("name_label", "").toString();
}

bool RestartToolstackCommand::isHostLive(const QString& hostRef) const
{
    if (!this->mainWindow()->xenLib())
        return false;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    QVariantMap hostData = cache->resolve("host", hostRef);
    if (hostData.isEmpty())
        return false;

    // Check if host is live (connected and responsive)
    bool live = hostData.value("live", false).toBool();
    return live;
}
