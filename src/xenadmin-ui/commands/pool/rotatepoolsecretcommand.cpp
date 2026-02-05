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
    XenObjectType objectType = this->getSelectedObjectType();
    if (objectType != XenObjectType::Pool && objectType != XenObjectType::Host)
        return false;

    QSharedPointer<Pool> pool;
    if (objectType == XenObjectType::Pool)
        pool = this->getPool();
    else if (objectType == XenObjectType::Host)
    {
        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->GetObject());
        pool = host ? host->GetPoolOfOne() : QSharedPointer<Pool>();
    }

    if (!pool || !pool->GetConnection() || !pool->IsValid())
        return false;

    return this->canRotateSecret(pool);
}

bool RotatePoolSecretCommand::canRotateSecret(const QSharedPointer<Pool>& pool) const
{
    if (!pool)
        return false;

    // Check HA enabled
    if (pool->HAEnabled())
        return false;

    // Check rolling upgrade
    QVariantMap otherConfig = pool->GetOtherConfig();
    bool rollingUpgrade = otherConfig.contains("rolling_upgrade_in_progress");
    if (rollingUpgrade)
        return false;

    // Check XenServer version
    if (!this->isStockholmOrGreater(pool))
        return false;

    // Check host restrictions
    if (this->hasRotationRestriction(pool))
        return false;

    return true;
}

void RotatePoolSecretCommand::Run()
{
    XenObjectType objectType = this->getSelectedObjectType();

    QSharedPointer<Pool> pool;

    if (objectType == XenObjectType::Pool)
    {
        pool = this->getPool();
        if (!pool || !pool->GetConnection() || !pool->IsValid())
            return;
    } else if (objectType == XenObjectType::Host)
    {
        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->GetObject());
        if (!host || !host->GetConnection() || !host->IsValid())
            return;
        pool = host->GetPoolOfOne();
    }

    if (!pool || !pool->IsValid())
        return;

    if (!this->canRotateSecret(pool))
    {
        QString reason = this->GetCantRunReason();
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

    // Resolve Pool object from cache
    // Create and run action
    QString poolName = pool->GetName().isEmpty() ? tr("Pool") : pool->GetName();

    RotatePoolSecretAction* action = new RotatePoolSecretAction(pool);

    action->SetTitle(tr("Rotating pool secret"));
    action->SetDescription(tr("Rotating secret for pool '%1'...").arg(poolName));

    OperationManager* opManager = OperationManager::instance();
    opManager->RegisterOperation(action);
    action->RunAsync(true);

    this->mainWindow()->ShowStatusMessage(tr("Pool secret rotation started"), 3000);
}

QString RotatePoolSecretCommand::MenuText() const
{
    return tr("&Rotate Pool Secret...");
}

QString RotatePoolSecretCommand::GetCantRunReason() const
{
    XenObjectType objectType = this->getSelectedObjectType();

    QSharedPointer<Pool> pool;
    if (objectType == XenObjectType::Pool)
    {
        pool = this->getPool();
        if (!pool || !pool->GetConnection() || !pool->IsValid())
            return tr("No pool selected.");
    } else if (objectType == XenObjectType::Host)
    {
        QSharedPointer<Host> host = qSharedPointerDynamicCast<Host>(this->GetObject());
        if (!host || !host->GetConnection() || !host->IsValid())
            return tr("No pool selected.");

        pool = host->GetPoolOfOne();
    }

    if (!pool)
        return tr("No pool selected.");

    // Check HA enabled
    if (pool->HAEnabled())
        return tr("Cannot rotate pool secret while HA is enabled.");

    // Check rolling upgrade
    QVariantMap otherConfig = pool->GetOtherConfig();
    bool rollingUpgrade = otherConfig.contains("rolling_upgrade_in_progress");
    if (rollingUpgrade)
        return tr("Cannot rotate pool secret during rolling upgrade.");

    // Check XenServer version
    if (!this->isStockholmOrGreater(pool))
        return tr("Pool secret rotation requires XenServer 8.0 or later.");

    // Check host restrictions
    if (this->hasRotationRestriction(pool))
        return tr("One or more hosts in the pool have restrictions preventing secret rotation.");

    return tr("Unknown reason.");
}

bool RotatePoolSecretCommand::hasRotationRestriction(const QSharedPointer<Pool>& pool) const
{
    if (!pool || !pool->GetConnection() || !pool->GetConnection()->GetCache())
        return false;

    XenCache* cache = pool->GetConnection()->GetCache();

    // Get all hosts in pool
    QStringList hostRefs = pool->GetHostRefs();

    for (const QString& hostRef : hostRefs)
    {
        QSharedPointer<Host> host = cache->ResolveObject<Host>(hostRef);
        if (!host || !host->IsValid())
            continue;

        // Check for restrict_pool_secret_rotation in host restrictions
        QVariantMap restrictions = host->GetData().value("restrictions", QVariantMap()).toMap();
        if (restrictions.value("restrict_pool_secret_rotation", false).toBool())
            return true;
    }

    return false;
}

bool RotatePoolSecretCommand::isStockholmOrGreater(const QSharedPointer<Pool>& pool) const
{
    // Check API version from connection
    // Stockholm = XenServer 8.0 = API version 2.11+

    if (!pool)
        return false;

    QVariantMap otherConfig = pool->GetOtherConfig();

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
