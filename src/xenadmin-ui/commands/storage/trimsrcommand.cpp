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

#include "trimsrcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/sr.h"
#include "xen/actions/sr/srtrimaction.h"
#include <QMessageBox>
#include <QDebug>

TrimSRCommand::TrimSRCommand(MainWindow* mainWindow, QObject* parent)
    : Command(mainWindow, parent)
{
}

void TrimSRCommand::setTargetSR(const QString& srRef)
{
    this->m_overrideSRRef = srRef;
}

bool TrimSRCommand::CanRun() const
{
    QString srRef = this->getSelectedSRRef();
    if (srRef.isEmpty())
        return false;

    QVariantMap srData = this->getSelectedSRData();
    if (srData.isEmpty())
        return false;

    // Can trim if SR supports it and is attached to a host
    return this->supportsTrim(srData) && this->isAttachedToHost(srData);
}

void TrimSRCommand::Run()
{
    QString srRef = this->getSelectedSRRef();
    QVariantMap srData = this->getSelectedSRData();

    if (srRef.isEmpty() || srData.isEmpty())
        return;

    QString srName = this->getSRName(srData);

    // Show confirmation dialog
    QMessageBox msgBox(this->mainWindow());
    msgBox.setWindowTitle("Trim Storage Repository");
    msgBox.setText(QString("Are you sure you want to trim storage repository '%1'?").arg(srName));
    msgBox.setInformativeText("Trimming will reclaim freed space from the storage repository.\n\n"
                              "This operation may take some time depending on the amount of space to reclaim.\n\n"
                              "Do you want to continue?");
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Yes)
        return;

    qDebug() << "TrimSRCommand: Trimming SR" << srName << "(" << srRef << ")";

    // Create SR object for action
    SR* sr = new SR(this->mainWindow()->xenLib()->getConnection(), srRef, this);

    // Create and run trim action
    SrTrimAction* action = new SrTrimAction(
        this->mainWindow()->xenLib()->getConnection(),
        sr,
        this);

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, [this, srName, action, sr]() {
        if (action->state() == AsyncOperation::Completed && !action->isFailed())
        {
            this->mainWindow()->showStatusMessage(QString("Successfully trimmed SR '%1'").arg(srName), 5000);

            QMessageBox::information(
                this->mainWindow(),
                "Trim Completed",
                QString("Successfully reclaimed freed space from storage repository '%1'.\n\n"
                        "The storage has been trimmed and space returned to the underlying storage.")
                    .arg(srName));
        } else
        {
            QMessageBox::warning(
                this->mainWindow(),
                "Trim Failed",
                QString("Failed to trim SR '%1'.\n\n%2")
                    .arg(srName, action->errorMessage()));
        }
        // Auto-delete when complete
        action->deleteLater();
        sr->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
}

QString TrimSRCommand::MenuText() const
{
    return "Trim SR...";
}

QString TrimSRCommand::getSelectedSRRef() const
{
    if (!this->m_overrideSRRef.isEmpty())
        return this->m_overrideSRRef;

    QString objectType = this->getSelectedObjectType();
    if (objectType != "sr")
        return QString();

    return this->getSelectedObjectRef();
}

QVariantMap TrimSRCommand::getSelectedSRData() const
{
    QString srRef = this->getSelectedSRRef();
    if (srRef.isEmpty())
        return QVariantMap();

    if (!this->mainWindow()->xenLib())
        return QVariantMap();

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return QVariantMap();

    return cache->ResolveObjectData("sr", srRef);
}

bool TrimSRCommand::supportsTrim(const QVariantMap& srData) const
{
    if (srData.isEmpty())
        return false;

    // Check if SR supports trim operation
    // From C# SR.SupportsTrim():
    // Trim is supported for thin-provisioned SRs
    // Check sm_config for "use_vhd" = "true" or SR type supports it

    QVariantMap smConfig = srData.value("sm_config", QVariantMap()).toMap();

    // Check if this is a VHD-based SR (thin-provisioned)
    bool useVhd = smConfig.value("use_vhd", false).toBool();
    if (useVhd)
        return true;

    // Check SR type - certain types support trim
    QString srType = srData.value("type", "").toString();

    // Types that typically support trim:
    // - lvm over iSCSI with thin provisioning
    // - local ext/xfs
    // Note: This is a simplified check - the actual support depends on backend
    QStringList trimSupportedTypes = {"ext", "nfs", "lvmoiscsi", "smb", "cifs"};

    if (trimSupportedTypes.contains(srType))
    {
        // For NFS and similar, check if it's actually thin-provisioned
        // by checking sm_config
        if (srType == "nfs" || srType == "smb" || srType == "cifs")
        {
            // NFS SRs with VHD support trim
            return useVhd;
        }
        return true;
    }

    return false;
}

bool TrimSRCommand::isAttachedToHost(const QVariantMap& srData) const
{
    if (srData.isEmpty())
        return false;

    if (!this->mainWindow()->xenLib())
        return false;

    XenCache* cache = this->mainWindow()->xenLib()->getCache();
    if (!cache)
        return false;

    // Check if any PBD is currently attached
    QVariantList pbds = srData.value("PBDs", QVariantList()).toList();

    for (const QVariant& pbdRefVar : pbds)
    {
        QString pbdRef = pbdRefVar.toString();
        QVariantMap pbdData = cache->ResolveObjectData("pbd", pbdRef);

        bool currentlyAttached = pbdData.value("currently_attached", false).toBool();
        if (currentlyAttached)
        {
            // At least one PBD is attached
            return true;
        }
    }

    return false;
}

QString TrimSRCommand::getSRName(const QVariantMap& srData) const
{
    if (srData.isEmpty())
        return "Storage Repository";

    QString name = srData.value("name_label", "").toString();
    return name.isEmpty() ? "Storage Repository" : name;
}
