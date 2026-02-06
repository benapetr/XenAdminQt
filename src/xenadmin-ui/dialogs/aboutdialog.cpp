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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "globals.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xencache.h"

#include <QApplication>
#include <QSysInfo>
#include <QDateTime>

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent), ui(new Ui::AboutDialog)
{
    this->ui->setupUi(this);

    this->ui->labelVersion->setText(this->getVersionInfo());

    QString infoHtml = QString(
                           "<h3>System Information</h3>"
                           "%1"
                           "<h3>Connection Information</h3>"
                           "%2"
                           "<h3>License Information</h3>"
                           "%3"
                           "<h3>Copyright</h3>"
                           "<p>Copyright © 2025-2026 XenAdmin Qt Project Contributors</p>"
                           "<p>Based on XenCenter/XenAdmin by Cloud Software Group, Inc.</p>"
                           "<h3>License</h3>"
                           "<p>This software is open source and distributed under the BSD license.</p>")
                           .arg(this->getSystemInfo(), this->getConnectionInfo(), this->getLicenseDetails());

    this->ui->textBrowserInfo->setHtml(infoHtml);

    connect(this->ui->buttonOk, &QPushButton::clicked, this, &QDialog::accept);
}

AboutDialog::~AboutDialog()
{
    delete this->ui;
}

QString AboutDialog::getVersionInfo() const
{
    return QString("Version " + QString(XENADMIN_VERSION));
}

QString AboutDialog::getSystemInfo() const
{
    QString info;
    info += QString("<p><b>Qt Version:</b> %1</p>").arg(qVersion());
    info += QString("<p><b>Build Date:</b> %1 %2</p>").arg(__DATE__, __TIME__);
    info += QString("<p><b>Operating System:</b> %1</p>").arg(QSysInfo::prettyProductName());
    info += QString("<p><b>Kernel:</b> %1 %2</p>").arg(QSysInfo::kernelType(), QSysInfo::kernelVersion());
    info += QString("<p><b>CPU Architecture:</b> %1</p>").arg(QSysInfo::currentCpuArchitecture());

    return info;
}

QString AboutDialog::getConnectionInfo() const
{
    // C# AboutDialog pattern: iterate through ConnectionsManager connections
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    QList<XenConnection*> connections = connMgr->GetConnectedConnections();

    if (connections.isEmpty())
    {
        return "<p><i>Not connected to any XenServer</i></p>";
    }

    QString info;
    int totalHosts = 0;
    int totalVMs = 0;

    // Iterate through all connected connections
    for (XenConnection* conn : connections)
    {
        if (!conn || !conn->IsConnected())
            continue;

        XenCache* cache = conn->GetCache();
        if (!cache)
            continue;

        // Get pool information
        QSharedPointer<Pool> pool = cache->GetPoolOfOne();
        if (pool && pool->IsValid())
        {
            QString poolName = pool->GetName().isEmpty() ? "Unnamed Pool" : pool->GetName();
            info += QString("<p><b>Pool:</b> %1</p>").arg(poolName);
        }

        // Count hosts
        QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>();
        totalHosts += hosts.size();

        // Count VMs
        QList<QSharedPointer<VM>> vms = cache->GetAll<VM>();
        totalVMs += vms.size();
    }

    info += QString("<p><b>Total Connections:</b> %1</p>").arg(connections.size());
    info += QString("<p><b>Total Hosts:</b> %1</p>").arg(totalHosts);
    info += QString("<p><b>Total VMs:</b> %1</p>").arg(totalVMs);

    return info;
}

QString AboutDialog::getLicenseDetails() const
{
    // C# AboutDialog.GetLicenseDetails() pattern
    Xen::ConnectionsManager* connMgr = Xen::ConnectionsManager::instance();
    QList<XenConnection*> connections = connMgr->GetConnectedConnections();

    if (connections.isEmpty())
    {
        return "<p><i>No license information available</i></p>";
    }

    QStringList companies;

    // Iterate through all connected connections and collect license company info
    for (XenConnection* conn : connections)
    {
        if (!conn || !conn->IsConnected())
            continue;

        XenCache* cache = conn->GetCache();
        if (!cache)
            continue;

        QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>();
        for (const QSharedPointer<Host>& host : hosts)
        {
            if (!host || !host->IsValid())
                continue;

            QVariantMap licenseParams = host->LicenseParams();
            if (licenseParams.contains("company"))
            {
                QString company = licenseParams.value("company").toString();
                if (!company.isEmpty() && !companies.contains(company))
                {
                    companies.append(company);
                }
            }
        }
    }

    if (companies.isEmpty())
    {
        return "<p><i>No license company information available</i></p>";
    }

    QString info = "<p>";
    info += companies.join("<br>");
    info += "</p>";

    return info;
}
