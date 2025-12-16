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

AboutDialog::AboutDialog(XenLib* xenLib, QWidget* parent)
    : QDialog(parent), m_xenLib(xenLib), m_pendingRequests(0)
{
    setupUI();

    // Connect async signals if we have a valid XenLib instance
    if (m_xenLib && m_xenLib->isConnected())
    {
        connect(m_xenLib, &XenLib::poolsReceived,
                this, &AboutDialog::onPoolsReceived);
        connect(m_xenLib, &XenLib::hostsReceived,
                this, &AboutDialog::onHostsReceived);
        connect(m_xenLib, &XenLib::virtualMachinesReceived,
                this, &AboutDialog::onVirtualMachinesReceived);

        // Request data asynchronously
        requestConnectionInfo();
    }
}

void AboutDialog::setupUI()
{
    setWindowTitle("About XenAdmin Qt");
    setMinimumSize(500, 400);
    resize(550, 450);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Title and version
    m_titleLabel = new QLabel("<h2>XenAdmin Qt</h2>", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_titleLabel);

    m_versionLabel = new QLabel(getVersionInfo(), this);
    m_versionLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_versionLabel);

    // Separator
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    // Information text browser
    m_infoText = new QTextBrowser(this);
    m_infoText->setOpenExternalLinks(false);
    m_infoText->setReadOnly(true);

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
                           .arg(getSystemInfo());

    m_infoText->setHtml(infoHtml);
    mainLayout->addWidget(m_infoText);

    // OK button
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_okButton = new QPushButton("OK", this);
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_okButton);
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
    if (!m_xenLib || !m_xenLib->isConnected())
    {
        updateConnectionInfo();
        return;
    }

    // Request data asynchronously - will arrive via signals
    m_pendingRequests = 3;
    m_xenLib->requestPools();
    m_xenLib->requestHosts();
    m_xenLib->requestVirtualMachines();
}

void AboutDialog::onPoolsReceived(const QVariantList& pools)
{
    m_pools = pools;
    m_pendingRequests--;

    if (m_pendingRequests == 0)
    {
        updateConnectionInfo();
    }
}

void AboutDialog::onHostsReceived(const QVariantList& hosts)
{
    m_hosts = hosts;
    m_pendingRequests--;

    if (m_pendingRequests == 0)
    {
        updateConnectionInfo();
    }
}

void AboutDialog::onVirtualMachinesReceived(const QVariantList& vms)
{
    m_vms = vms;
    m_pendingRequests--;

    if (m_pendingRequests == 0)
    {
        updateConnectionInfo();
    }
}

void AboutDialog::updateConnectionInfo()
{
    QString info;

    if (!m_xenLib || !m_xenLib->isConnected())
    {
        info = "<p><i>Not connected to any XenServer</i></p>";
    } else
    {
        QString connInfo = m_xenLib->getConnectionInfo();
        info += QString("<p><b>Connected to:</b> %1</p>").arg(connInfo);

        // Get pool information if available
        if (!m_pools.isEmpty())
        {
            QVariantMap pool = m_pools.first().toMap();
            QString poolName = pool.value("name_label", "Unnamed Pool").toString();
            QString poolUuid = pool.value("uuid", "Unknown").toString();

            info += QString("<p><b>Pool:</b> %1</p>").arg(poolName);
            info += QString("<p><b>Pool UUID:</b> %2</p>").arg(poolUuid);
        }

        // Get host information
        if (!m_hosts.isEmpty())
        {
            info += QString("<p><b>Number of Hosts:</b> %1</p>").arg(m_hosts.size());
        }

        // Get VM count
        if (!m_vms.isEmpty())
        {
            info += QString("<p><b>Number of VMs:</b> %1</p>").arg(m_vms.size());
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
                           .arg(getSystemInfo(), info);

    m_infoText->setHtml(infoHtml);
}
