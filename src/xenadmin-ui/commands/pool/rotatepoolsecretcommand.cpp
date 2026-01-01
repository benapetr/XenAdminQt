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

#include "rotatepoolsecretcommand.h"
#include "mainwindow.h"
#include "xencache.h"
#include "xen/network/connection.h"
#include "xen/pool.h"
#include "xen/host.h"
#include "operations/operationmanager.h"
#include "xen/actions/pool/rotatepoolsecretaction.h"
#include <QMessageBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSettings>

RotatePoolSecretCommand::RotatePoolSecretCommand(MainWindow* mainWindow, QObject* parent) : PoolCommand(mainWindow, parent)
{
}

bool RotatePoolSecretCommand::CanRun() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "pool" && objectType != "host")
        return false;

    // Get pool reference
    QString poolRef;
    XenCache* cache = nullptr;
    
    if (objectType == "pool")
    {
        QSharedPointer<Pool> pool = this->getPool();
        if (!pool || !pool->GetConnection() || !pool->IsValid())
            return false;
        poolRef = pool->OpaqueRef();
        cache = pool->GetConnection()->GetCache();
    } else if (objectType == "host")
    {
        QSharedPointer<Host> host = qSharedPointerCast<Host>(this->GetObject());
        if (!host || !host->GetConnection() || !host->IsValid())
            return false;
        QString hostRef = host->OpaqueRef();
        QVariantMap hostData = host->GetData();
        QList<QVariant> poolList = hostData.value("pool", QVariantList()).toList();
        cache = host->GetConnection()->GetCache();
        if (!poolList.isEmpty())
            poolRef = poolList.first().toString();
    }

    if (poolRef.isEmpty())
        return false;

    QVariantMap poolData = cache->ResolveObjectData("pool", poolRef);
    if (poolData.isEmpty())
        return false;

    return this->canRotateSecret(poolData, cache);
}

bool RotatePoolSecretCommand::canRotateSecret(const QVariantMap& poolData, XenCache *cache) const
{
    // Check HA enabled
    if (poolData.value("ha_enabled", false).toBool())
        return false;

    // Check rolling upgrade
    QVariantMap otherConfig = poolData.value("other_config", QVariantMap()).toMap();
    bool rollingUpgrade = otherConfig.contains("rolling_upgrade_in_progress");
    if (rollingUpgrade)
        return false;

    // Check XenServer version
    if (!this->isStockholmOrGreater(cache))
        return false;

    // Check host restrictions
    QString poolRef = poolData.value("uuid", "").toString();
    if (this->hasRotationRestriction(poolRef, cache))
        return false;

    return true;
}

void RotatePoolSecretCommand::Run()
{
    QString objectType = this->getSelectedObjectType();

    // Get pool reference and cache
    QString poolRef;
    XenCache* cache = nullptr;
    XenConnection *connection = nullptr;
    
    if (objectType == "pool")
    {
        QSharedPointer<Pool> pool = this->getPool();
        if (!pool || !pool->GetConnection() || !pool->IsValid())
            return;

        poolRef = pool->OpaqueRef();
        connection = pool->GetConnection();
        cache = pool->GetConnection()->GetCache();
    } else if (objectType == "host")
    {
        QSharedPointer<Host> host = qSharedPointerCast<Host>(this->GetObject());
        if (!host || !host->GetConnection() || !host->IsValid())
            return;

        QString hostRef = host->OpaqueRef();
        QVariantMap hostData = host->GetData();
        connection = host->GetConnection();
        cache = host->GetConnection()->GetCache();
        QList<QVariant> poolList = hostData.value("pool", QVariantList()).toList();
        if (!poolList.isEmpty())
            poolRef = poolList.first().toString();
    }

    if (poolRef.isEmpty() || !cache)
        return;

    QVariantMap poolData = cache->ResolveObjectData("pool", poolRef);
    if (poolData.isEmpty())
    {
        QMessageBox::warning(this->mainWindow(), tr("Error"),
                             tr("Unable to retrieve pool information."));
        return;
    }

    if (!this->canRotateSecret(poolData, cache))
    {
        QString reason = this->getCantRunReason();
        QMessageBox::information(this->mainWindow(), tr("Cannot Rotate Pool Secret"), reason);
        return;
    }

    // Show reminder dialog (first time only)
    QSettings settings;
    bool showReminder = settings.value("Pool/RemindChangePassword", true).toBool();

    if (showReminder)
    {
        QDialog* dialog = new QDialog(this->mainWindow());
        dialog->setWindowTitle(tr("Rotate Pool Secret"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);

        QVBoxLayout* layout = new QVBoxLayout(dialog);

        QLabel* label = new QLabel(
            tr("After rotating the pool secret, you should change the passwords "
               "of all pool members to ensure security.\n\n"
               "The pool secret is used for authentication between hosts in the pool."),
            dialog);
        label->setWordWrap(true);
        layout->addWidget(label);

        QCheckBox* checkBox = new QCheckBox(tr("Do not show this message again"), dialog);
        layout->addWidget(checkBox);

        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
        layout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

        if (dialog->exec() != QDialog::Accepted)
            return;

        if (checkBox->isChecked())
        {
            settings.setValue("Pool/RemindChangePassword", false);
        }
    }

    // Create and run action
    QString poolName = poolData.value("name_label", "Pool").toString();

    RotatePoolSecretAction* action = new RotatePoolSecretAction(connection, poolRef);

    action->setTitle(tr("Rotating pool secret"));
    action->setDescription(tr("Rotating secret for pool '%1'...").arg(poolName));

    OperationManager* opManager = OperationManager::instance();
    opManager->registerOperation(action);
    action->runAsync();

    this->mainWindow()->showStatusMessage(tr("Pool secret rotation started"), 3000);
}

QString RotatePoolSecretCommand::MenuText() const
{
    return tr("&Rotate Pool Secret...");
}

QString RotatePoolSecretCommand::getCantRunReason() const
{
    QString objectType = this->getSelectedObjectType();
    XenCache* cache = nullptr;

    // Get pool reference
    QString poolRef;
    if (objectType == "pool")
    {
        QSharedPointer<Pool> pool = this->getPool();
        if (!pool || !pool->GetConnection() || !pool->IsValid())
            return tr("No pool selected.");

        cache = pool->GetConnection()->GetCache();
        poolRef = this->getSelectedObjectRef();
    } else if (objectType == "host")
    {
        QSharedPointer<Host> host = qSharedPointerCast<Host>(this->GetObject());
        if (!host || !host->GetConnection() || !host->IsValid())
            return tr("No pool selected.");

        cache = host->GetConnection()->GetCache();
        QString hostRef = host->OpaqueRef();
        QVariantMap hostData = host->GetData();
        QList<QVariant> poolList = hostData.value("pool", QVariantList()).toList();
        if (!poolList.isEmpty())
            poolRef = poolList.first().toString();
    }

    if (poolRef.isEmpty())
        return tr("No pool selected.");

    QVariantMap poolData = cache->ResolveObjectData("pool", poolRef);
    if (poolData.isEmpty())
        return tr("Unable to retrieve pool information.");

    // Check HA enabled
    if (poolData.value("ha_enabled", false).toBool())
        return tr("Cannot rotate pool secret while HA is enabled.");

    // Check rolling upgrade
    QVariantMap otherConfig = poolData.value("other_config", QVariantMap()).toMap();
    bool rollingUpgrade = otherConfig.contains("rolling_upgrade_in_progress");
    if (rollingUpgrade)
        return tr("Cannot rotate pool secret during rolling upgrade.");

    // Check XenServer version
    if (!this->isStockholmOrGreater(cache))
        return tr("Pool secret rotation requires XenServer 8.0 or later.");

    // Check host restrictions
    if (this->hasRotationRestriction(poolRef, cache))
        return tr("One or more hosts in the pool have restrictions preventing secret rotation.");

    return tr("Unknown reason.");
}

bool RotatePoolSecretCommand::hasRotationRestriction(const QString& poolRef, XenCache* cache) const
{
    // Get all hosts in pool
    QVariantMap poolData = cache->ResolveObjectData("pool", poolRef);
    QList<QVariant> hostRefs = poolData.value("hosts", QVariantList()).toList();

    for (const QVariant& hostRefVariant : hostRefs)
    {
        QString hostRef = hostRefVariant.toString();
        QVariantMap hostData = cache->ResolveObjectData("host", hostRef);

        // Check for restrict_pool_secret_rotation in host restrictions
        QVariantMap restrictions = hostData.value("restrictions", QVariantMap()).toMap();
        if (restrictions.value("restrict_pool_secret_rotation", false).toBool())
            return true;
    }

    return false;
}

bool RotatePoolSecretCommand::isStockholmOrGreater(XenCache *cache) const
{
    // Check API version from connection
    // Stockholm = XenServer 8.0 = API version 2.11+

    // Get pool to check version
    QList<QVariantMap> pools = cache->GetAllData("pool");
    if (pools.isEmpty())
        return false;

    QVariantMap poolData = pools.first();
    QVariantMap otherConfig = poolData.value("other_config", QVariantMap()).toMap();

    // Check for platform version or API version
    // Stockholm is version 8.0.0 or API 2.11+
    QString platformVersion = otherConfig.value("platform_version", "0.0.0").toString();
    QStringList versionParts = platformVersion.split(".");

    if (!versionParts.isEmpty())
    {
        int majorVersion = versionParts.first().toInt();
        if (majorVersion >= 8)
            return true;
    }

    // Fallback: assume true if we can't determine (better to allow than block)
    return true;
}
