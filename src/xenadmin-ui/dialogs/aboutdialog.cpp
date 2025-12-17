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
#include "xenlib.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QSysInfo>
#include <QDateTime>

AboutDialog::AboutDialog(XenLib* xenLib, QWidget* parent) : QDialog(parent), m_xenLib(xenLib), m_pendingRequests(0)
{
    this->setupUI();

    // Connect async signals if we have a valid XenLib instance
    if (this->m_xenLib && this->m_xenLib->isConnected())
    {
        connect(this->m_xenLib, &XenLib::poolsReceived, this, &AboutDialog::onPoolsReceived);
        connect(this->m_xenLib, &XenLib::hostsReceived, this, &AboutDialog::onHostsReceived);
        connect(this->m_xenLib, &XenLib::virtualMachinesReceived, this, &AboutDialog::onVirtualMachinesReceived);

        // Request data asynchronously
        this->requestConnectionInfo();
    }
}

void AboutDialog::setupUI()
{
    this->setWindowTitle("About XenAdmin Qt");
    this->setMinimumSize(500, 400);
    this->resize(550, 450);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Title and version
    this->m_titleLabel = new QLabel("<h2>XenAdmin Qt</h2>", this);
    this->m_titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(this->m_titleLabel);

    this->m_versionLabel = new QLabel(this->getVersionInfo(), this);
    this->m_versionLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(this->m_versionLabel);

    // Separator
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    // Information text browser
    this->m_infoText = new QTextBrowser(this);
    this->m_infoText->setOpenExternalLinks(false);
    this->m_infoText->setReadOnly(true);

    QString infoHtml = QString(
                           "<h3>System Information</h3>"
                           "%1"
                           "<h3>Connection Information</h3>"
                           "<p><i>Loading...</i></p>"
                           "<h3>Copyright</h3>"
                           "<p>Copyright © 2024-2025 XenAdmin Qt Project Contributors</p>"
                           "<p>Based on XenCenter/XenAdmin by Cloud Software Group, Inc.</p>"
                           "<h3>License</h3>"
                           "<p>This software is open source and distributed under the BSD license.</p>")
                           .arg(this->getSystemInfo());

    this->m_infoText->setHtml(infoHtml);
    mainLayout->addWidget(this->m_infoText);

    // OK button
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    this->m_okButton = new QPushButton("OK", this);
    this->m_okButton->setDefault(true);
    connect(this->m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(this->m_okButton);
    mainLayout->addLayout(buttonLayout);
}

QString AboutDialog::getVersionInfo() const
{
    return QString("Version 0.1.0 (Development Build)");
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

void AboutDialog::requestConnectionInfo()
{
    if (!this->m_xenLib || !this->m_xenLib->isConnected())
    {
        this->updateConnectionInfo();
        return;
    }

    // Request data asynchronously - will arrive via signals
    this->m_pendingRequests = 3;
    this->m_xenLib->requestPools();
    this->m_xenLib->requestHosts();
    this->m_xenLib->requestVirtualMachines();
}

void AboutDialog::onPoolsReceived(const QVariantList& pools)
{
    this->m_pools = pools;
    this->m_pendingRequests--;

    if (this->m_pendingRequests == 0)
    {
        this->updateConnectionInfo();
    }
}

void AboutDialog::onHostsReceived(const QVariantList& hosts)
{
    this->m_hosts = hosts;
    this->m_pendingRequests--;

    if (this->m_pendingRequests == 0)
    {
        this->updateConnectionInfo();
    }
}

void AboutDialog::onVirtualMachinesReceived(const QVariantList& vms)
{
    this->m_vms = vms;
    this->m_pendingRequests--;

    if (this->m_pendingRequests == 0)
    {
        this->updateConnectionInfo();
    }
}

void AboutDialog::updateConnectionInfo()
{
    QString info;

    if (!this->m_xenLib || !this->m_xenLib->isConnected())
    {
        info = "<p><i>Not connected to any XenServer</i></p>";
    } else
    {
        QString connInfo = this->m_xenLib->getConnectionInfo();
        info += QString("<p><b>Connected to:</b> %1</p>").arg(connInfo);

        // Get pool information if available
        if (!this->m_pools.isEmpty())
        {
            QVariantMap pool = this->m_pools.first().toMap();
            QString poolName = pool.value("name_label", "Unnamed Pool").toString();
            QString poolUuid = pool.value("uuid", "Unknown").toString();

            info += QString("<p><b>Pool:</b> %1</p>").arg(poolName);
            info += QString("<p><b>Pool UUID:</b> %2</p>").arg(poolUuid);
        }

        // Get host information
        if (!this->m_hosts.isEmpty())
        {
            info += QString("<p><b>Number of Hosts:</b> %1</p>").arg(this->m_hosts.size());
        }

        // Get VM count
        if (!this->m_vms.isEmpty())
        {
            info += QString("<p><b>Number of VMs:</b> %1</p>").arg(this->m_vms.size());
        }
    }

    // Update the HTML with connection info
    QString infoHtml = QString(
                           "<h3>System Information</h3>"
                           "%1"
                           "<h3>Connection Information</h3>"
                           "%2"
                           "<h3>Copyright</h3>"
                           "<p>Copyright © 2024-2025 XenAdmin Qt Project Contributors</p>"
                           "<p>Based on XenCenter/XenAdmin by Cloud Software Group, Inc.</p>"
                           "<h3>License</h3>"
                           "<p>This software is open source and distributed under the BSD license.</p>")
                           .arg(this->getSystemInfo(), info);

    this->m_infoText->setHtml(infoHtml);
}
