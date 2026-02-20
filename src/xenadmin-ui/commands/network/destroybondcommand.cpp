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
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/network/destroybondaction.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/pif.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include <QPointer>
#include <QMessageBox>
#include <QDebug>

DestroyBondCommand::DestroyBondCommand(MainWindow* mainWindow, QObject* parent) : Command(mainWindow, parent)
{
}

DestroyBondCommand::DestroyBondCommand(MainWindow* mainWindow, const QSharedPointer<Network>& network, QObject* parent)
    : Command(mainWindow, parent)
{
    if (network)
        this->SetSelectionOverride(QList<QSharedPointer<XenObject>>{network});
}

bool DestroyBondCommand::CanRun() const
{
    QSharedPointer<Network> network = qSharedPointerDynamicCast<Network>(this->GetObject());
    if (!network)
        return false;

    // Can only destroy if this is a bonded network
    return this->isNetworkABond(network);
}

void DestroyBondCommand::Run()
{
    QSharedPointer<Network> network = qSharedPointerDynamicCast<Network>(this->GetObject());

    if (!network)
        return;

    QString bondRef = this->getBondRefFromNetwork(network);
    if (bondRef.isEmpty())
        return;

    QString bondName = this->getBondName(network);

    bool affectsPrimary = false;
    bool affectsSecondary = false;
    this->checkManagementImpact(network, affectsPrimary, affectsSecondary);

    // Check if HA is enabled and primary management will be affected
    if (affectsPrimary && this->isHAEnabled())
    {
        QMessageBox::critical(
            MainWindow::instance(),
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
    QMessageBox msgBox(MainWindow::instance());
    msgBox.setWindowTitle("Delete Bond");
    msgBox.setText(message);
    msgBox.setIcon(affectsPrimary ? QMessageBox::Critical : QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);

    int ret = msgBox.exec();

    if (ret != QMessageBox::Ok)
        return;

    qDebug() << "DestroyBondCommand: Destroying bond" << bondName << "(" << bondRef << ")";

    XenConnection* connection = network->GetConnection();
    if (!connection)
        return;

    // Create and run destroy bond action
    DestroyBondAction* action = new DestroyBondAction(connection, bondRef, nullptr);

    QPointer<MainWindow> mainWindow = MainWindow::instance();
    if (!mainWindow)
    {
        action->deleteLater();
        return;
    }

    // Connect completion signal for cleanup and status update
    connect(action, &AsyncOperation::completed, mainWindow, [bondName, action, mainWindow]()
    {
        if (action->GetState() == AsyncOperation::Completed && !action->IsFailed())
        {
            mainWindow->ShowStatusMessage(QString("Successfully deleted bond '%1'").arg(bondName), 5000);
        } else
        {
            QMessageBox::warning(mainWindow, "Delete Bond Failed", QString("Failed to delete bond '%1'.\n\n%2").arg(bondName, action->GetErrorMessage()));
        }
        // Auto-delete when complete
        action->deleteLater();
    }, Qt::QueuedConnection);

    // Run action asynchronously
    action->RunAsync();
}

QString DestroyBondCommand::MenuText() const
{
    return "Delete Bond";
}

bool DestroyBondCommand::isNetworkABond(QSharedPointer<Network> network) const
{
    QList<QSharedPointer<PIF>> pifs = network->GetPIFs();
    if (pifs.isEmpty())
        return false;

    // Check if any PIF is a bond interface
    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;

        if (!pif->BondMasterOfRefs().isEmpty())
        {
            // This PIF is a bond interface
            return true;
        }
    }

    return false;
}

QString DestroyBondCommand::getBondRefFromNetwork(QSharedPointer<Network> network) const
{
    QList<QSharedPointer<PIF>> pifs = network->GetPIFs();
    if (pifs.isEmpty())
        return QString();

    // Find the first PIF that is a bond interface
    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;

        const QStringList bondMasterOf = pif->BondMasterOfRefs();
        if (!bondMasterOf.isEmpty())
        {
            // Return first bond reference
            return bondMasterOf.first();
        }
    }

    return QString();
}

void DestroyBondCommand::checkManagementImpact(const QSharedPointer<Network>& network,
                                               bool& affectsPrimary,
                                               bool& affectsSecondary) const
{
    affectsPrimary = false;
    affectsSecondary = false;

    if (!network)
        return;

    QList<QSharedPointer<PIF>> pifs = network->GetPIFs();
    if (pifs.isEmpty())
        return;

    if (!this->GetObject() || !this->GetObject()->GetConnection())
        return;

    // Check each PIF to see if it's a management interface
    for (const QSharedPointer<PIF>& pif : pifs)
    {
        if (!pif || !pif->IsValid())
            continue;

        if (pif->Management())
        {
            // Check if this is the primary management interface (host.address matches PIF.IP)
            QSharedPointer<Host> host = pif->GetHost();
            const QString hostAddress = host ? host->GetAddress() : QString();
            const QString pifIP = pif->IP();

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

    QSharedPointer<Pool> pool = cache->GetPoolOfOne();
    return pool && pool->HAEnabled();
}

QString DestroyBondCommand::getBondName(const QSharedPointer<Network>& network) const
{
    if (!network)
        return "Bond";

    // Try to get the network name first
    QString networkName = network->GetName();
    if (!networkName.isEmpty())
        return networkName;

    // Otherwise try to get the PIF device name
    QList<QSharedPointer<PIF>> pifs = network->GetPIFs();
    if (!pifs.isEmpty())
    {
        QSharedPointer<PIF> pif = pifs.first();
        QString device = pif ? pif->GetDevice() : QString();
        if (!device.isEmpty())
            return device;
    }

    return "Bond";
}
