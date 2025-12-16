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

#include "homeservereditpage.h"
#include "ui_homeservereditpage.h"
#include "../../xenlib/xen/asyncoperation.h"
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/api.h"
#include <QTableWidgetItem>

HomeServerEditPage::HomeServerEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::HomeServerEditPage)
{
    ui->setupUi(this);

    connect(ui->radioButtonStaticHomeServer, &QRadioButton::toggled,
            this, &HomeServerEditPage::onStaticHomeServerToggled);
}

HomeServerEditPage::~HomeServerEditPage()
{
    delete ui;
}

QString HomeServerEditPage::text() const
{
    return tr("Home Server");
}

QString HomeServerEditPage::subText() const
{
    if (ui->radioButtonNoHomeServer->isChecked())
    {
        return tr("None defined");
    }

    QString hostRef = getSelectedHostRef();
    if (hostRef.isEmpty())
    {
        return tr("None selected");
    }

    // Get host name from hosts map
    if (m_hosts.contains(hostRef))
    {
        QVariantMap hostData = m_hosts[hostRef].toMap();
        QString name = hostData.value("name_label").toString();
        if (!name.isEmpty())
        {
            return name;
        }
    }

    return tr("Unknown host");
}

QIcon HomeServerEditPage::image() const
{
    return QIcon::fromTheme("network-server");
}

void HomeServerEditPage::setXenObjects(const QString& objectRef,
                                       const QString& objectType,
                                       const QVariantMap& objectDataBefore,
                                       const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectType);
    Q_UNUSED(objectDataCopy);

    m_vmRef = objectRef;

    // Get VM's current affinity
    m_originalAffinityRef = objectDataBefore.value("affinity").toString();

    // Get list of hosts from connection
    // NOTE: In real implementation, this should request hosts from XenLib
    // For now, we'll populate from the connection's cached data
    loadHosts();

    // Select appropriate radio button
    if (m_originalAffinityRef.isEmpty() || m_originalAffinityRef == "OpaqueRef:NULL")
    {
        ui->radioButtonNoHomeServer->setChecked(true);
    } else
    {
        ui->radioButtonStaticHomeServer->setChecked(true);

        // Select the host in the table
        for (int row = 0; row < ui->tableWidgetServers->rowCount(); ++row)
        {
            QTableWidgetItem* item = ui->tableWidgetServers->item(row, 0);
            if (item && item->data(Qt::UserRole).toString() == m_originalAffinityRef)
            {
                ui->tableWidgetServers->selectRow(row);
                break;
            }
        }
    }
}

AsyncOperation* HomeServerEditPage::saveSettings()
{
    if (!hasChanged())
    {
        return nullptr;
    }

    // Determine new affinity
    QString newAffinityRef;
    if (ui->radioButtonNoHomeServer->isChecked())
    {
        newAffinityRef = "OpaqueRef:NULL";
    } else
    {
        newAffinityRef = getSelectedHostRef();
    }

    // Return inline AsyncOperation
    class SetAffinityOperation : public AsyncOperation
    {
    public:
        SetAffinityOperation(XenConnection* conn,
                             const QString& vmRef,
                             const QString& affinityRef,
                             QObject* parent)
            : AsyncOperation(conn, tr("Set Home Server"),
                             tr("Setting VM home server..."), parent),
              m_vmRef(vmRef), m_affinityRef(affinityRef)
        {}

    protected:
        void run() override
        {
            XenRpcAPI api(connection()->getSession());

            setPercentComplete(30);

            QVariantList params;
            params << connection()->getSessionId() << m_vmRef << m_affinityRef;
            QByteArray request = api.buildJsonRpcCall("VM.set_affinity", params);
            connection()->sendRequest(request);

            setPercentComplete(100);
        }

    private:
        QString m_vmRef;
        QString m_affinityRef;
    };

    return new SetAffinityOperation(m_connection, m_vmRef, newAffinityRef, this);
}

bool HomeServerEditPage::isValidToSave() const
{
    // Valid if either no home server selected, or a host is selected
    if (ui->radioButtonNoHomeServer->isChecked())
    {
        return true;
    }

    return !getSelectedHostRef().isEmpty();
}

void HomeServerEditPage::showLocalValidationMessages()
{
    // Could show message if static selected but no host chosen
}

void HomeServerEditPage::hideLocalValidationMessages()
{
    // Nothing to hide
}

void HomeServerEditPage::cleanup()
{
    // Nothing to clean up
}

bool HomeServerEditPage::hasChanged() const
{
    QString currentAffinityRef;

    if (ui->radioButtonNoHomeServer->isChecked())
    {
        currentAffinityRef = "OpaqueRef:NULL";
    } else
    {
        currentAffinityRef = getSelectedHostRef();
    }

    // Normalize empty and NULL refs
    QString origRef = m_originalAffinityRef;
    if (origRef.isEmpty() || origRef == "OpaqueRef:NULL")
    {
        origRef = "OpaqueRef:NULL";
    }

    if (currentAffinityRef.isEmpty() || currentAffinityRef == "OpaqueRef:NULL")
    {
        currentAffinityRef = "OpaqueRef:NULL";
    }

    return origRef != currentAffinityRef;
}

void HomeServerEditPage::onStaticHomeServerToggled(bool checked)
{
    ui->tableWidgetServers->setEnabled(checked);

    if (checked && ui->tableWidgetServers->selectedItems().isEmpty() && ui->tableWidgetServers->rowCount() > 0)
    {
        // Auto-select first host if none selected
        ui->tableWidgetServers->selectRow(0);
    }
}

void HomeServerEditPage::loadHosts()
{
    ui->tableWidgetServers->setRowCount(0);

    // In a real implementation, this should request hosts from XenLib
    // For now, we'll demonstrate with a simplified approach
    // The connection should provide access to cached host data

    // TODO: Request hosts via connection()->getXenLib()->requestHosts()
    // and populate m_hosts map

    // For demonstration, we'll show how to populate the table
    // when host data becomes available:
    /*
    foreach (const QString& hostRef, m_hosts.keys()) {
        QVariantMap hostData = m_hosts[hostRef].toMap();
        QString hostName = hostData.value("name_label").toString();
        QString hostStatus = hostData.value("enabled").toBool() ? tr("Online") : tr("Offline");

        int row = ui->tableWidgetServers->rowCount();
        ui->tableWidgetServers->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(hostName);
        nameItem->setData(Qt::UserRole, hostRef);
        ui->tableWidgetServers->setItem(row, 0, nameItem);

        QTableWidgetItem* statusItem = new QTableWidgetItem(hostStatus);
        ui->tableWidgetServers->setItem(row, 1, statusItem);
    }
    */
}

QString HomeServerEditPage::getSelectedHostRef() const
{
    QList<QTableWidgetItem*> selectedItems = ui->tableWidgetServers->selectedItems();
    if (selectedItems.isEmpty())
    {
        return QString();
    }

    // Get the host ref from the first column item's UserRole data
    int row = selectedItems.first()->row();
    QTableWidgetItem* item = ui->tableWidgetServers->item(row, 0);
    if (item)
    {
        return item->data(Qt::UserRole).toString();
    }

    return QString();
}
