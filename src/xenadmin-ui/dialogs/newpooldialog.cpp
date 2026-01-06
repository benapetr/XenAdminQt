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
#include "operationprogressdialog.h"
#include "../mainwindow.h"

#include <xen/network/connection.h>
#include <xen/session.h>
#include <xen/host.h>
#include <xen/actions/pool/createpoolaction.h>
#include <xen/network/connectionsmanager.h>
#include <xencache.h>
#include <xen/xenapi/xenapi_Pool.h>

#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QDebug>

/**
 * @brief NewPoolDialog constructor
 *
 * Port of C# NewPoolDialog constructor
 * - Populates coordinator combobox with standalone servers
 * - Populates supporter list with checkboxes
 * - Sets up validation logic
 */
NewPoolDialog::NewPoolDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NewPoolDialog)
{
    this->ui->setupUi(this);

    // Set Create button text
    QPushButton* createBtn = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    if (createBtn)
    {
        createBtn->setText(tr("Create Pool"));
        createBtn->setEnabled(false);
    }

    // Connect signals
    this->connect(this->ui->comboBoxCoordinator, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NewPoolDialog::onCoordinatorChanged);
    this->connect(this->ui->listWidgetServers, &QListWidget::itemChanged, this, &NewPoolDialog::onServerItemChanged);
    this->connect(this->ui->lineEditName, &QLineEdit::textChanged, this, &NewPoolDialog::onPoolNameChanged);
    this->connect(this->ui->buttonAddServer, &QPushButton::clicked, this, &NewPoolDialog::onAddServerClicked);
    this->connect(this->ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &NewPoolDialog::onCreateClicked);

    // Populate connections from ConnectionsManager
    this->populateConnections();
    this->updateCreateButton();
}

NewPoolDialog::~NewPoolDialog()
{
    delete this->ui;
}

/**
 * @brief Populate coordinator combobox and server list with connected standalone servers
 *
 * Port of C# getAllCurrentConnections() and addConnectionsToComboBox()
 * Only includes connections that:
 * - Are connected
 * - Are standalone (not already part of a pool)
 */
void NewPoolDialog::populateConnections()
{
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    if (!connMgr)
    {
        qWarning() << "NewPoolDialog: No ConnectionsManager available";
        return;
    }

    // Get all connected connections
    QList<XenConnection*> allConnections = connMgr->GetConnectedConnections();

    // Filter to standalone servers only
    this->m_connections.clear();
    for (XenConnection* conn : allConnections)
    {
        if (conn && this->isStandaloneConnection(conn))
        {
            this->m_connections.append(conn);
        }
    }

    // Populate coordinator combobox
    this->ui->comboBoxCoordinator->clear();
    for (XenConnection* conn : this->m_connections)
    {
        QString displayName = conn->GetHostname();

        // Try to get host name from cache
        XenCache* cache = conn->GetCache();
        QList<QVariantMap> hosts = cache->GetAllData("host");
        if (!hosts.isEmpty())
        {
            QString hostName = hosts.first().value("name_label").toString();
            if (!hostName.isEmpty())
            {
                displayName = hostName;
            }
        }

        this->ui->comboBoxCoordinator->addItem(displayName, QVariant::fromValue(reinterpret_cast<quintptr>(conn)));
    }

    this->updateServerList();
}

/**
 * @brief Update server list based on selected coordinator
 *
 * Port of C# addConnectionsToListBox()
 * Shows checkboxes for servers that can be supporters
 * Excludes the currently selected coordinator
 */
void NewPoolDialog::updateServerList()
{
    this->ui->listWidgetServers->clear();

    XenConnection* coordinator = this->getCoordinatorConnection();

    for (XenConnection* conn : this->m_connections)
    {
        // Don't show coordinator in supporter list
        if (conn == coordinator)
        {
            continue;
        }

        QString displayName = conn->GetHostname();

        // Try to get host name from cache
        XenCache* cache = conn->GetCache();
        if (cache)
        {
            QList<QVariantMap> hosts = cache->GetAllData("host");
            if (!hosts.isEmpty())
            {
                QString hostName = hosts.first().value("name_label").toString();
                if (!hostName.isEmpty())
                {
                    displayName = hostName;
                }
            }
        }

        QListWidgetItem* item = new QListWidgetItem(displayName);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(conn)));
        this->ui->listWidgetServers->addItem(item);
    }
}

/**
 * @brief Check if a connection is to a standalone server (not in a pool)
 *
 * Port of C# CanBeCoordinator property from ConnectionWrapperWithMoreStuff
 * A server is standalone if:
 * - The pool has only one host (the coordinator)
 * - Pool.master == the only host in the pool
 */
bool NewPoolDialog::isStandaloneConnection(XenConnection* connection) const
{
    if (!connection || !connection->IsConnected())
    {
        return false;
    }

    XenCache* cache = connection->GetCache();

    // Check if pool has only one host
    QList<QVariantMap> hosts = cache->GetAllData("host");
    if (hosts.size() != 1)
    {
        return false; // Multi-host pool - not standalone
    }

    // It's a standalone server with one host
    return true;
}

XenConnection* NewPoolDialog::getCoordinatorConnection() const
{
    int index = this->ui->comboBoxCoordinator->currentIndex();
    if (index < 0)
    {
        return nullptr;
    }

    quintptr ptr = this->ui->comboBoxCoordinator->itemData(index).value<quintptr>();
    return reinterpret_cast<XenConnection*>(ptr);
}

QList<XenConnection*> NewPoolDialog::getSupporterConnections() const
{
    QList<XenConnection*> supporters;

    for (int i = 0; i < this->ui->listWidgetServers->count(); ++i)
    {
        QListWidgetItem* item = this->ui->listWidgetServers->item(i);
        if (item && item->checkState() == Qt::Checked)
        {
            quintptr ptr = item->data(Qt::UserRole).value<quintptr>();
            XenConnection* conn = reinterpret_cast<XenConnection*>(ptr);
            if (conn)
            {
                supporters.append(conn);
            }
        }
    }

    return supporters;
}

void NewPoolDialog::onCoordinatorChanged(int index)
{
    Q_UNUSED(index);
    this->updateServerList();
    this->updateCreateButton();
}

void NewPoolDialog::onServerItemChanged(QListWidgetItem* item)
{
    Q_UNUSED(item);
    this->updateCreateButton();
}

void NewPoolDialog::onPoolNameChanged(const QString& text)
{
    Q_UNUSED(text);
    this->updateCreateButton();
}

void NewPoolDialog::onAddServerClicked()
{
    // Open connect dialog to add new server
    /*ConnectDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
    {
        // After successful connection, refresh the list
        // The new connection will be added through ConnectionsManager signals
        // For now, we just repopulate after a short delay
        QTimer::singleShot(1000, this, &NewPoolDialog::populateConnections);
    }*/
}

/**
 * @brief Update Create button enabled state based on validation
 *
 * Port of C# updateButtons() and validToClose property
 * Create is enabled when:
 * - Pool name is not empty
 * - A coordinator is selected
 * - (Optional: supporters selected, but not required)
 */
void NewPoolDialog::updateCreateButton()
{
    QPushButton* createBtn = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    if (!createBtn)
        return;

    QString poolName = this->ui->lineEditName->text().trimmed();
    XenConnection* coordinator = this->getCoordinatorConnection();

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
    }

    this->ui->labelStatus->setText(statusText);
    createBtn->setEnabled(canCreate);
}

void NewPoolDialog::onCreateClicked()
{
    this->createPool();
}

/**
 * @brief Create the pool with selected coordinator and supporters
 *
 * Port of C# createPool() from NewPoolDialog.cs
 * Uses CreatePoolAction to handle the async pool creation with progress tracking.
 */
void NewPoolDialog::createPool()
{
    QString poolName = this->ui->lineEditName->text().trimmed();
    QString poolDescription = this->ui->lineEditDescription->text().trimmed();
    XenConnection* coordinatorConn = this->getCoordinatorConnection();
    QList<XenConnection*> supporterConns = this->getSupporterConnections();

    if (!coordinatorConn)
    {
        QMessageBox::warning(this, tr("Error"), tr("No coordinator selected."));
        return;
    }

    XenAPI::Session* coordinatorSession = coordinatorConn->GetSession();
    if (!coordinatorSession || !coordinatorSession->IsLoggedIn())
    {
        QMessageBox::warning(this, tr("Error"), tr("Coordinator is not connected."));
        return;
    }

    qDebug() << "Creating pool:" << poolName << "with coordinator:" << coordinatorConn->GetHostname();
    qDebug() << "Supporters:" << supporterConns.size();

    // Create the action using the existing CreatePoolAction class
    // Note: Host* parameters are nullptr - the action doesn't use them in current implementation
    QList<Host*> emptyHostList; // Placeholder - action uses connections directly

    CreatePoolAction* action = new CreatePoolAction(
        coordinatorConn,
        nullptr, // coordinator Host* - not used in current implementation
        supporterConns,
        emptyHostList, // member Host* list - not used in current implementation
        poolName,
        poolDescription,
        this);

    // Show progress dialog and run the action
    OperationProgressDialog progressDialog(action, this);
    progressDialog.setWindowTitle(tr("Creating Pool"));

    // Connect completion signals
    this->connect(action, &CreatePoolAction::completed, this, [this, poolName]()
    {
        QMessageBox::information(this, tr("Success"), tr("Pool '%1' created successfully.").arg(poolName));
        this->accept();
    });

    this->connect(action, &CreatePoolAction::failed, this, [this](const QString& error) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to create pool: %1").arg(error));
    });

    // Start the action - progress dialog handles display
    action->RunAsync();
    progressDialog.exec();
}
