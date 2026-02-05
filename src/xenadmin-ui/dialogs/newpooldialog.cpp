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

#include "newpooldialog.h"
#include "ui_newpooldialog.h"

#include "warningdialogs/warningdialog.h"
#include "addserverdialog.h"

#include "../mainwindow.h"
#include "../controls/customtreeview.h"
#include "../controls/connectionwrapperwithmorestuff.h"
#include "../network/xenconnectionui.h"

#include <xen/network/connection.h>
#include <xen/network/connectionsmanager.h>
#include <xen/session.h>
#include <xen/host.h>
#include <xen/pool.h>
#include <xen/actions/pool/createpoolaction.h>
#include <xen/pooljoinrules.h>
#include <xencache.h>

#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

namespace
{
    bool getPermissionForCpuFeatureLevelling(const QList<QSharedPointer<Host>>& hosts, const QSharedPointer<Pool>& pool, QWidget* owner)
    {
        if (hosts.isEmpty() || !pool)
            return true;

        QList<QSharedPointer<Host>> hostsWithFewerFeatures;
        QList<QSharedPointer<Host>> hostsWithMoreFeatures;

        for (const QSharedPointer<Host>& host : hosts)
        {
            if (!host || !host->IsValid())
                continue;
            if (PoolJoinRules::HostHasFewerFeatures(host, pool))
                hostsWithFewerFeatures.append(host);
            if (PoolJoinRules::HostHasMoreFeatures(host, pool))
                hostsWithMoreFeatures.append(host);
        }

        auto listNames = [](const QList<QSharedPointer<Host>>& list) {
            QStringList names;
            for (const QSharedPointer<Host>& host : list)
                if (host && host->IsValid())
                    names.append(host->GetName());
            return names.join("\n");
        };

        if (!hostsWithFewerFeatures.isEmpty() && !hostsWithMoreFeatures.isEmpty())
        {
            const QString msg = QObject::tr("CPU feature levelling will down-level both pool and host CPUs for:\n%1")
                .arg(listNames(hostsWithFewerFeatures + hostsWithMoreFeatures));
            const auto result = WarningDialog::ShowYesNo(msg, QObject::tr("CPU Feature Levelling"), owner);
            return result == WarningDialog::Result::Yes;
        }

        if (!hostsWithFewerFeatures.isEmpty())
        {
            const QString msg = QObject::tr("CPU feature levelling will down-level the pool CPUs for:\n%1")
                .arg(listNames(hostsWithFewerFeatures));
            const auto result = WarningDialog::ShowYesNo(msg, QObject::tr("CPU Feature Levelling"), owner);
            return result == WarningDialog::Result::Yes;
        }

        if (!hostsWithMoreFeatures.isEmpty())
        {
            const QString msg = QObject::tr("CPU feature levelling will down-level host CPUs for:\n%1")
                .arg(listNames(hostsWithMoreFeatures));
            const auto result = WarningDialog::ShowYesNo(msg, QObject::tr("CPU Feature Levelling"), owner);
            return result == WarningDialog::Result::Yes;
        }

        return true;
    }
}

NewPoolDialog::NewPoolDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NewPoolDialog)
{
    this->ui->setupUi(this);

    QPushButton* createBtn = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    if (createBtn)
    {
        createBtn->setText(tr("Create Pool"));
        createBtn->setEnabled(false);
    }

    this->ui->customTreeViewServers->SetNodeIndent(3);
    this->ui->customTreeViewServers->SetShowCheckboxes(true);
    this->ui->customTreeViewServers->SetShowDescription(true);
    this->ui->customTreeViewServers->SetShowImages(false);

    connect(this->ui->comboBoxCoordinator, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewPoolDialog::onCoordinatorChanged);
    connect(this->ui->lineEditName, &QLineEdit::textChanged, this, &NewPoolDialog::onPoolNameChanged);
    connect(this->ui->buttonAddServer, &QPushButton::clicked, this, &NewPoolDialog::onAddServerClicked);
    connect(this->ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &NewPoolDialog::onCreateClicked);
    connect(this->ui->customTreeViewServers, &CustomTreeView::ItemCheckChanged, this, &NewPoolDialog::onTreeItemCheckChanged);

    if (auto* manager = Xen::ConnectionsManager::instance())
    {
        connect(manager, &Xen::ConnectionsManager::connectionAdded, this, &NewPoolDialog::onConnectionsChanged);
        connect(manager, &Xen::ConnectionsManager::connectionRemoved, this, &NewPoolDialog::onConnectionsChanged);
        connect(manager, &Xen::ConnectionsManager::connectionStateChanged, this, &NewPoolDialog::onConnectionsChanged);
    }

    this->getAllCurrentConnections();
    this->updateButtons();
}

NewPoolDialog::~NewPoolDialog()
{
    for (ConnectionWrapperWithMoreStuff* wrapper : this->m_connections)
    {
        if (wrapper)
            delete wrapper;
    }
    this->m_connections.clear();
    delete this->ui;
}

void NewPoolDialog::onCoordinatorChanged(int index)
{
    Q_UNUSED(index);
    if (this->ui->comboBoxCoordinator->count() == 0)
        return;

    ConnectionWrapperWithMoreStuff* coordinator =
        this->ui->comboBoxCoordinator->currentData(Qt::UserRole).value<ConnectionWrapperWithMoreStuff*>();
    this->setAsCoordinator(coordinator);
}

void NewPoolDialog::onPoolNameChanged(const QString& text)
{
    Q_UNUSED(text);
    this->updateButtons();
}

void NewPoolDialog::onAddServerClicked()
{
    AddServerDialog dialog(nullptr, false, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString serverInput = dialog.serverInput();
    QString hostname = serverInput;
    int port = 443;
    const int lastColon = serverInput.lastIndexOf(':');
    if (lastColon > 0 && lastColon < serverInput.size() - 1)
    {
        bool ok = false;
        const int parsedPort = serverInput.mid(lastColon + 1).toInt(&ok);
        if (ok)
        {
            hostname = serverInput.left(lastColon).trimmed();
            port = parsedPort;
        }
    }

    XenConnection* connection = new XenConnection(nullptr);
    Xen::ConnectionsManager::instance()->AddConnection(connection);

    connection->SetHostname(hostname);
    connection->SetPort(port);
    connection->SetUsername(dialog.username());
    connection->SetPassword(dialog.password());
    connection->SetExpectPasswordIsCorrect(false);
    connection->SetFromDialog(true);

    this->m_newConnections.append(connection);
    XenConnectionUI::BeginConnect(connection, true, this, false);
}

void NewPoolDialog::onCreateClicked()
{
    this->createPool();
    close();
}

void NewPoolDialog::onConnectionsChanged()
{
    this->getAllCurrentConnections();
}

void NewPoolDialog::onConnectionCachePopulated()
{
    this->addConnectionsToComboBox();
}

void NewPoolDialog::onConnectionStateChanged()
{
    this->addConnectionsToComboBox();
}

void NewPoolDialog::onTreeItemCheckChanged(CustomTreeNode* node)
{
    Q_UNUSED(node);
    this->updateButtons();
}

void NewPoolDialog::getAllCurrentConnections()
{
    Xen::ConnectionsManager* manager = Xen::ConnectionsManager::instance();
    if (!manager)
        return;

    const QList<XenConnection*> allConnections = manager->GetAllConnections();

    QList<XenConnection*> toRemove;

    for (XenConnection* connection : allConnections)
    {
        if (!connection || !connection->IsConnected())
            continue;

        bool contains = false;
        for (ConnectionWrapperWithMoreStuff* wrapper : this->m_connections)
        {
            if (wrapper && wrapper->Connection() == connection)
            {
                contains = true;
                break;
            }
        }

        if (!contains)
        {
            auto* wrapper = new ConnectionWrapperWithMoreStuff(connection);
            this->m_connections.append(wrapper);

            connect(connection, &XenConnection::CachePopulated, this, &NewPoolDialog::onConnectionCachePopulated, Qt::UniqueConnection);
            connect(connection, &XenConnection::ConnectionStateChanged, this, &NewPoolDialog::onConnectionStateChanged, Qt::UniqueConnection);

            if (this->m_newConnections.contains(connection))
            {
                wrapper->SetState(Qt::Checked);
                toRemove.append(connection);
            }
        }
    }

    for (XenConnection* connection : toRemove)
        this->m_newConnections.removeAll(connection);

    for (int i = this->m_connections.size() - 1; i >= 0; --i)
    {
        ConnectionWrapperWithMoreStuff* wrapper = this->m_connections.at(i);
        if (!wrapper)
        {
            this->m_connections.removeAt(i);
            continue;
        }

        if (!allConnections.contains(wrapper->Connection()))
        {
            this->m_connections.removeAt(i);
            delete wrapper;
        }
    }

    this->addConnectionsToComboBox();
}

void NewPoolDialog::addConnectionsToComboBox()
{
    this->ui->comboBoxCoordinator->clear();

    ConnectionWrapperWithMoreStuff* coordinator = nullptr;

    for (ConnectionWrapperWithMoreStuff* wrapper : this->m_connections)
    {
        if (!wrapper)
            continue;
        wrapper->Refresh();
        if (wrapper->CanBeCoordinator())
        {
            this->ui->comboBoxCoordinator->addItem(wrapper->ToString(), QVariant::fromValue(wrapper));
            if (wrapper->WillBeCoordinator())
                coordinator = wrapper;
        }
    }

    if (coordinator)
    {
        this->ui->comboBoxCoordinator->setCurrentIndex(this->ui->comboBoxCoordinator->findData(QVariant::fromValue(coordinator)));
    } else if (this->ui->comboBoxCoordinator->count() > 0)
    {
        coordinator = this->ui->comboBoxCoordinator->itemData(0).value<ConnectionWrapperWithMoreStuff*>();
        this->setAsCoordinator(coordinator);
        this->ui->comboBoxCoordinator->setCurrentIndex(0);
    }

    this->addConnectionsToListBox();
    this->updateButtons();
}

void NewPoolDialog::addConnectionsToListBox()
{
    ConnectionWrapperWithMoreStuff* coordinator =
        this->ui->comboBoxCoordinator->currentData(Qt::UserRole).value<ConnectionWrapperWithMoreStuff*>();

    for (ConnectionWrapperWithMoreStuff* wrapper : this->m_connections)
    {
        if (!wrapper)
            continue;
        wrapper->SetCoordinator(coordinator);
        wrapper->Refresh();
    }

    this->ui->customTreeViewServers->BeginUpdate();
    this->ui->customTreeViewServers->ClearAllNodes();
    for (ConnectionWrapperWithMoreStuff* wrapper : this->m_connections)
    {
        if (!wrapper)
            continue;
        this->ui->customTreeViewServers->AddNode(wrapper);
    }
    this->ui->customTreeViewServers->Resort();
    this->ui->customTreeViewServers->EndUpdate();
}

void NewPoolDialog::setAsCoordinator(ConnectionWrapperWithMoreStuff* coordinator)
{
    for (ConnectionWrapperWithMoreStuff* wrapper : this->m_connections)
    {
        if (!wrapper)
            continue;
        wrapper->SetCoordinator(coordinator);
    }

    this->addConnectionsToListBox();
}

void NewPoolDialog::selectCoordinator(const QSharedPointer<Host>& host)
{
    if (!host || host->GetPool())
        return;

    for (ConnectionWrapperWithMoreStuff* wrapper : this->m_connections)
        if (wrapper && wrapper->WillBeCoordinator())
            wrapper->SetState(Qt::Unchecked);

    for (ConnectionWrapperWithMoreStuff* wrapper : this->m_connections)
    {
        if (!wrapper)
            continue;
        if (wrapper->Connection() == host->GetConnection())
        {
            if (wrapper->CanBeCoordinator())
                this->setAsCoordinator(wrapper);
            return;
        }
    }
}

void NewPoolDialog::selectSupporters(const QList<QSharedPointer<Host>>& hosts)
{
    for (ConnectionWrapperWithMoreStuff* wrapper : this->m_connections)
    {
        if (!wrapper || !wrapper->AllowedAsSupporter())
            continue;

        for (const QSharedPointer<Host>& host : hosts)
        {
            if (host && wrapper->Connection() == host->GetConnection())
                wrapper->SetState(Qt::Checked);
        }
    }
}

QSharedPointer<Host> NewPoolDialog::getCoordinator() const
{
    for (ConnectionWrapperWithMoreStuff* wrapper : this->m_connections)
    {
        if (!wrapper)
            continue;
        if (wrapper->WillBeCoordinator())
        {
            XenConnection* conn = wrapper->Connection();
            if (conn && conn->GetCache())
            {
                QSharedPointer<Pool> pool = conn->GetCache()->GetPoolOfOne();
                if (pool && pool->IsValid())
                    return pool->GetMasterHost();

                QList<QSharedPointer<Host>> hosts = conn->GetCache()->GetAll<Host>(XenObjectType::Host);
                if (!hosts.isEmpty())
                    return hosts.first();
            }
        }
    }

    return QSharedPointer<Host>();
}

QList<QSharedPointer<Host>> NewPoolDialog::getSupporters() const
{
    QList<QSharedPointer<Host>> supporters;

    for (ConnectionWrapperWithMoreStuff* wrapper : this->m_connections)
    {
        if (!wrapper)
            continue;
        if (wrapper->State() == Qt::Checked && !wrapper->WillBeCoordinator() && wrapper->AllowedAsSupporter())
        {
            XenConnection* conn = wrapper->Connection();
            if (conn && conn->GetCache())
            {
                QSharedPointer<Pool> pool = conn->GetCache()->GetPoolOfOne();
                if (pool && pool->GetMasterHost())
                    supporters.append(pool->GetMasterHost());
            }
        }
    }

    return supporters;
}

void NewPoolDialog::updateButtons()
{
    QPushButton* createBtn = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    if (!createBtn)
        return;

    const QString poolName = this->ui->lineEditName->text().trimmed();
    const QSharedPointer<Host> coordinator = this->getCoordinator();

    QString statusText;
    bool canCreate = true;

    if (poolName.isEmpty())
    {
        statusText = tr("Please enter a pool name.");
        canCreate = false;
    } else if (!coordinator)
    {
        statusText = tr("Please select a coordinator server.");
        canCreate = false;
    } else
    {
        const QList<QSharedPointer<Host>> supporters = this->getSupporters();
        if (PoolJoinRules::WillExceedPoolMaxSize(coordinator->GetConnection(), supporters.size()))
        {
            statusText = tr("The pool would exceed the maximum size.");
            canCreate = false;
        }
    }

    this->ui->labelStatus->setText(statusText);
    createBtn->setToolTip(statusText);
    createBtn->setEnabled(canCreate);
}

void NewPoolDialog::createPool()
{
    const QString poolName = this->ui->lineEditName->text().trimmed();
    const QString poolDescription = this->ui->lineEditDescription->text().trimmed();
    const QSharedPointer<Host> coordinator = this->getCoordinator();
    if (!coordinator)
        return;

    const QList<QSharedPointer<Host>> supporters = this->getSupporters();

    const QList<QString> badSuppPacks = PoolJoinRules::HomogeneousSuppPacksDiffering(supporters, coordinator);
    if (!badSuppPacks.isEmpty())
    {
        const QString msg = tr("Some supplemental packs differ across hosts:\n%1").arg(badSuppPacks.join("\n"));
        WarningDialog dialog(msg, tr("Supplemental Packs"),
                             { {tr("Proceed"), WarningDialog::Result::Yes},
                               {tr("Cancel"), WarningDialog::Result::Cancel} },
                             this);
        dialog.exec();
        if (dialog.GetResult() != WarningDialog::Result::Yes)
            return;
    }

    QList<QSharedPointer<Host>> hostsWithLicenseIssues;
    for (const QSharedPointer<Host>& host : supporters)
        if (PoolJoinRules::FreeHostPaidCoordinator(host, coordinator, false))
            hostsWithLicenseIssues.append(host);

    if (!hostsWithLicenseIssues.isEmpty())
    {
        QStringList hostNames;
        for (const QSharedPointer<Host>& host : hostsWithLicenseIssues)
            if (host)
                hostNames.append(host->GetName());

        const QString msg = tr("The following hosts will be relicensed to match the coordinator:\n%1").arg(hostNames.join("\n"));
        const auto result = WarningDialog::ShowYesNo(msg, tr("License Warning"), this);
        if (result != WarningDialog::Result::Yes)
            return;
    }

    QList<QSharedPointer<Host>> hostsWithCpuMismatch;
    for (const QSharedPointer<Host>& host : supporters)
        if (!PoolJoinRules::CompatibleCPUs(host, coordinator))
            hostsWithCpuMismatch.append(host);

    if (!hostsWithCpuMismatch.isEmpty())
    {
        QStringList hostNames;
        for (const QSharedPointer<Host>& host : hostsWithCpuMismatch)
            if (host)
                hostNames.append(host->GetName());

        const QString msg = tr("CPU masking will be required for:\n%1").arg(hostNames.join("\n"));
        const auto result = WarningDialog::ShowYesNo(msg, tr("CPU Masking"), this);
        if (result != WarningDialog::Result::Yes)
            return;
    }

    QList<QSharedPointer<Host>> hostsWithAdMismatch;
    for (const QSharedPointer<Host>& host : supporters)
        if (!PoolJoinRules::CompatibleAdConfig(host, coordinator, false))
            hostsWithAdMismatch.append(host);

    if (!hostsWithAdMismatch.isEmpty())
    {
        QStringList hostNames;
        for (const QSharedPointer<Host>& host : hostsWithAdMismatch)
            if (host)
                hostNames.append(host->GetName());

        const QString msg = tr("Active Directory configuration will be updated for:\n%1").arg(hostNames.join("\n"));
        const auto result = WarningDialog::ShowYesNo(msg, tr("Active Directory"), this);
        if (result != WarningDialog::Result::Yes)
            return;
    }

    if (!getPermissionForCpuFeatureLevelling(supporters, coordinator->GetPoolOfOne(), this))
        return;

    QList<XenConnection*> memberConnections;
    QList<Host*> memberHosts;
    for (const QSharedPointer<Host>& host : supporters)
    {
        memberConnections.append(host->GetConnection());
        memberHosts.append(host.data());
    }

    CreatePoolAction* action = new CreatePoolAction(
        coordinator->GetConnection(),
        coordinator.data(),
        memberConnections,
        memberHosts,
        poolName,
        poolDescription,
        nullptr);

    action->RunAsync(true);
}
