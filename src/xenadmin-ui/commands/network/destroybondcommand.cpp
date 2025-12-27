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

#include "destroybondcommand.h"
#include "../../mainwindow.h"
#include "../../operations/operationmanager.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/network/destroybondaction.h"
#include "xenlib/xen/xenobject.h"
#include <QMessageBox>
#include <QDebug>

DestroyBondCommand::DestroyBondCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

bool DestroyBondCommand::CanRun() const
{
    QString networkRef = this->getSelectedNetworkRef();
    if (networkRef.isEmpty())
        return false;

    QVariantMap networkData = this->getSelectedNetworkData();
    if (networkData.isEmpty())
        return false;

    // Can only destroy if this is a bonded network
    return this->isNetworkABond(networkData);
}

void DestroyBondCommand::Run()
{
    QString networkRef = this->getSelectedNetworkRef();
    QVariantMap networkData = this->getSelectedNetworkData();

    if (networkRef.isEmpty() || networkData.isEmpty())
        return;

    QString bondRef = this->getBondRefFromNetwork(networkData);
    if (bondRef.isEmpty())
        return;

    QString bondName = this->getBondName(networkData);

    bool affectsPrimary = false;
    bool affectsSecondary = false;
    this->checkManagementImpact(networkData, affectsPrimary, affectsSecondary);

    // Check if HA is enabled and primary management will be affected
    if (affectsPrimary && this->isHAEnabled())
    {
        QMessageBox::critical(
            this->mainWindow(),
            "Cannot Delete Bond",
            QString("Cannot delete bond '%1' because High Availability (HA) is enabled "
                    "and this bond carries the primary management interface.\n\n"
                    "Please disable HA before deleting this bond.")
                .arg(bondName));
        return;
    }

    // Build confirmation message
    QString message;
    if (affectsPrimary && affectsSecondary)
    {
        message = QString("Are you sure you want to delete bond '%1'?\n\n"
                          "WARNING: This bond carries BOTH the primary and secondary management interfaces. "
                          "Deleting it will interrupt your connection to XenServer and may make "
                          "the servers inaccessible until you reconfigure networking.\n\n"
                          "Do you want to continue?")
                      .arg(bondName);
    } else if (affectsPrimary)
    {
        message = QString("Are you sure you want to delete bond '%1'?\n\n"
                          "WARNING: This bond carries the primary management interface. "
                          "Deleting it will interrupt your connection to XenServer.\n\n"
                          "Do you want to continue?")
                      .arg(bondName);
    } else if (affectsSecondary)
    {
        message = QString("Are you sure you want to delete bond '%1'?\n\n"
                          "Warning: This bond carries the secondary management interface. "
                          "Deleting it may affect failover capabilities.\n\n"
                          "Do you want to continue?")
                      .arg(bondName);
    } else
    {
        message = QString("Are you sure you want to delete bond '%1'?")
                      .arg(bondName);
    }

    // Show confirmation dialog
    QMessageBox msgBox(this->mainWindow());
    msgBox.setWindowTitle("Delete Bond");
    msgBox.setText(message);
    msgBox.setIcon(affectsPrimary ? QMessageBox::Critical : QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Ok)
        return;

    qDebug() << "DestroyBondCommand: Destroying bond" << bondName << "(" << bondRef << ")";

    XenConnection* connection = this->GetObject() ? this->GetObject()->GetConnection() : nullptr;
    if (!connection)
        return;

    // Create and run destroy bond action
    DestroyBondAction* action = new DestroyBondAction(
        connection,
        bondRef,
        this);

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, [this, bondName, action]() {
        if (action->state() == AsyncOperation::Completed && !action->isFailed())
        {
            this->mainWindow()->showStatusMessage(QString("Successfully deleted bond '%1'").arg(bondName), 5000);
        } else
        {
            QMessageBox::warning(
                this->mainWindow(),
                "Delete Bond Failed",
                QString("Failed to delete bond '%1'.\n\n%2")
                    .arg(bondName, action->errorMessage()));
        }
        // Auto-delete when complete
        action->deleteLater();
    });

    // Run action asynchronously
    action->runAsync();
}

QString DestroyBondCommand::MenuText() const
{
    return "Delete Bond";
}

QString DestroyBondCommand::getSelectedNetworkRef() const
{
    QString objectType = this->getSelectedObjectType();
    if (objectType != "network")
        return QString();

    return this->getSelectedObjectRef();
}

QVariantMap DestroyBondCommand::getSelectedNetworkData() const
{
    if (!this->GetObject())
        return QVariantMap();

    return this->GetObject()->GetData();
}

bool DestroyBondCommand::isNetworkABond(const QVariantMap& networkData) const
{
    if (networkData.isEmpty())
        return false;

    // Check if network has any PIFs
    QVariantList pifs = networkData.value("PIFs", QVariantList()).toList();
    if (pifs.isEmpty())
        return false;

    if (!this->GetObject() || !this->GetObject()->GetConnection())
        return false;

    XenCache* cache = this->GetObject()->GetConnection()->GetCache();

    // Check if any PIF is a bond interface
    for (const QVariant& pifRefVar : pifs)
    {
        QString pifRef = pifRefVar.toString();
        QVariantMap pifData = cache->ResolveObjectData("pif", pifRef);

        // Check if PIF has a bond_master_of field (is a bond interface)
        QVariantList bondMasterOf = pifData.value("bond_master_of", QVariantList()).toList();
        if (!bondMasterOf.isEmpty())
        {
            // This PIF is a bond interface
            return true;
        }
    }

    return false;
}

QString DestroyBondCommand::getBondRefFromNetwork(const QVariantMap& networkData) const
{
    if (networkData.isEmpty())
        return QString();

    QVariantList pifs = networkData.value("PIFs", QVariantList()).toList();
    if (pifs.isEmpty())
        return QString();

    if (!this->GetObject() || !this->GetObject()->GetConnection())
        return QString();

    XenCache* cache = this->GetObject()->GetConnection()->GetCache();
    if (!cache)
        return QString();

    // Find the first PIF that is a bond interface
    for (const QVariant& pifRefVar : pifs)
    {
        QString pifRef = pifRefVar.toString();
        QVariantMap pifData = cache->ResolveObjectData("pif", pifRef);

        QVariantList bondMasterOf = pifData.value("bond_master_of", QVariantList()).toList();
        if (!bondMasterOf.isEmpty())
        {
            // Return first bond reference
            return bondMasterOf.first().toString();
        }
    }

    return QString();
}

void DestroyBondCommand::checkManagementImpact(const QVariantMap& networkData,
                                               bool& affectsPrimary,
                                               bool& affectsSecondary) const
{
    affectsPrimary = false;
    affectsSecondary = false;

    if (networkData.isEmpty())
        return;

    QVariantList pifs = networkData.value("PIFs", QVariantList()).toList();
    if (pifs.isEmpty())
        return;

    if (!this->GetObject() || !this->GetObject()->GetConnection())
        return;

    XenCache* cache = this->GetObject()->GetConnection()->GetCache();
    if (!cache)
        return;

    // Check each PIF to see if it's a management interface
    for (const QVariant& pifRefVar : pifs)
    {
        QString pifRef = pifRefVar.toString();
        QVariantMap pifData = cache->ResolveObjectData("pif", pifRef);

        bool isManagement = pifData.value("management", false).toBool();
        if (isManagement)
        {
            // Check if this is the primary management interface (host.address matches PIF.IP)
            QString hostRef = pifData.value("host").toString();
            QVariantMap hostData = cache->ResolveObjectData("host", hostRef);
            QString hostAddress = hostData.value("address").toString();
            QString pifIP = pifData.value("IP").toString();

            if (hostAddress == pifIP)
            {
                affectsPrimary = true;
            } else
            {
                affectsSecondary = true;
            }
        }
    }
}

bool DestroyBondCommand::isHAEnabled() const
{
    if (!this->GetObject() || !this->GetObject()->GetConnection())
        return false;

    XenCache* cache = this->GetObject()->GetConnection()->GetCache();
    if (!cache)
        return false;

    // Get all objects and check pools
    QList<QPair<QString, QString>> allObjects = cache->GetAllObjectsData();
    for (const auto& obj : allObjects)
    {
        if (obj.first == "pool")
        {
            QVariantMap poolData = cache->ResolveObjectData("pool", obj.second);
            bool haEnabled = poolData.value("ha_enabled", false).toBool();
            if (haEnabled)
                return true;
        }
    }

    return false;
}

QString DestroyBondCommand::getBondName(const QVariantMap& networkData) const
{
    if (networkData.isEmpty())
        return "Bond";

    // Try to get the network name first
    QString networkName = networkData.value("name_label", "").toString();
    if (!networkName.isEmpty())
        return networkName;

    // Otherwise try to get the PIF device name
    QVariantList pifs = networkData.value("PIFs", QVariantList()).toList();
    if (!pifs.isEmpty() && this->GetObject() && this->GetObject()->GetConnection())
    {
        XenCache* cache = this->GetObject()->GetConnection()->GetCache();
        if (cache)
        {
            QString pifRef = pifs.first().toString();
            QVariantMap pifData = cache->ResolveObjectData("pif", pifRef);
            QString device = pifData.value("device", "").toString();
            if (!device.isEmpty())
                return device;
        }
    }

    return "Bond";
}
