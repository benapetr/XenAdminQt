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

#include "haconfigurecommand.h"
#include "../../mainwindow.h"
#include "../../dialogs/hawizard.h"
#include "../../dialogs/editvmhaprioritiesdialog.h"
#include "xenlib.h"
#include "xen/connection.h"
#include "xencache.h"
#include <QMessageBox>

HAConfigureCommand::HAConfigureCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

bool HAConfigureCommand::canRun() const
{
    QString poolRef = this->getSelectedPoolRef();
    if (poolRef.isEmpty())
        return false;

    // Can configure HA if:
    // - Pool is connected
    // - Pool has coordinator
    // - Pool is not locked
    return this->isPoolConnected() && this->hasCoordinator() && !this->isPoolLocked();
}

void HAConfigureCommand::run()
{
    QString poolRef = this->getSelectedPoolRef();
    if (poolRef.isEmpty())
        return;

    // Check if HA is already enabled
    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (cache)
    {
        QVariantMap poolData = cache->resolve("pool", poolRef);
        bool haEnabled = poolData.value("ha_enabled", false).toBool();

        if (haEnabled)
        {
            // HA is already enabled - show edit dialog to modify priorities
            EditVmHaPrioritiesDialog dialog(this->mainWindow()->xenLib(), poolRef, this->mainWindow());
            dialog.exec();
            return;
        }
    }

    // Launch HA wizard to enable HA
    HAWizard wizard(this->mainWindow()->xenLib(), poolRef, this->mainWindow());
    wizard.exec();
}

QString HAConfigureCommand::menuText() const
{
    return "Configure...";
}

QString HAConfigureCommand::getSelectedPoolRef() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "pool")
        return QString();

    return this->getSelectedObjectRef();
}

bool HAConfigureCommand::isPoolConnected() const
{
    if (!this->mainWindow()->xenLib())
        return false;

    XenConnection* conn = this->mainWindow()->xenLib()->getConnection();
    return conn && conn->isConnected();
}

bool HAConfigureCommand::hasCoordinator() const
{
    if (!this->mainWindow()->xenLib())
        return false;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    // Get all hosts and check if any is coordinator
    QStringList hosts = cache->getAllRefs("host");
    for (const QString& hostRef : hosts)
    {
        QVariantMap hostData = cache->resolve("host", hostRef);
        if (hostData.value("master", false).toBool())
            return true;
    }

    return false;
}

bool HAConfigureCommand::isPoolLocked() const
{
    QString poolRef = this->getSelectedPoolRef();
    if (poolRef.isEmpty())
        return false;

    if (!this->mainWindow()->xenLib())
        return false;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    QVariantMap poolData = cache->resolve("pool", poolRef);

    // Check if pool has any operations in progress that would lock it
    QVariantMap currentOps = poolData.value("current_operations", QVariantMap()).toMap();
    return !currentOps.isEmpty();
}
