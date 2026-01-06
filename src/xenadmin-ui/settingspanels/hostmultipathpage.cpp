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

#include "hostmultipathpage.h"
#include "ui_hostmultipathpage.h"
#include "xen/host.h"
#include "xen/network/connection.h"
#include "xen/actions/host/editmultipathaction.h"
#include "xencache.h"
#include "xen/hostmetrics.h"
#include <QDebug>
#include <QSharedPointer>

HostMultipathPage::HostMultipathPage(QWidget* parent) : IEditPage(parent), ui(new Ui::HostMultipathPage), m_originalMultipathEnabled(false)
{
    this->ui->setupUi(this);

    // Connect signals
    connect(this->ui->multipathCheckBox, &QCheckBox::stateChanged, this, &HostMultipathPage::onMultipathCheckBoxChanged);
}

HostMultipathPage::~HostMultipathPage()
{
    delete this->ui;
}

QString HostMultipathPage::GetText() const
{
    return tr("Multipathing");
}

QString HostMultipathPage::GetSubText() const
{
    return this->ui->multipathCheckBox->isChecked() 
        ? tr("Active") 
        : tr("Not active");
}

QIcon HostMultipathPage::GetImage() const
{
    // Matches C# Images.StaticImages._000_Storage_h32bit_16
    return QIcon(":/icons/storage.png");
}

void HostMultipathPage::SetXenObjects(const QString& objectRef,
                                     const QString& objectType,
                                     const QVariantMap& objectDataBefore,
                                     const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectType);
    
    this->m_hostRef = objectRef;
    this->m_objectDataBefore = objectDataBefore;
    this->m_objectDataCopy = objectDataCopy;

    // Get multipath setting from other_config
    // Reference: C# Host.MultipathEnabled() in XenModel/XenObjects/Host.cs
    QVariantMap otherConfig = objectDataCopy.value("other_config").toMap();
    QString multipathValue = otherConfig.value("multipathing", "false").toString();
    this->m_originalMultipathEnabled = (multipathValue.toLower() == "true");

    this->ui->multipathCheckBox->setChecked(this->m_originalMultipathEnabled);

    // Update maintenance mode warning
    this->updateMaintenanceWarning();
}

AsyncOperation* HostMultipathPage::SaveSettings()
{
    if (!this->HasChanged())
        return nullptr;

    // Create Host object
    XenConnection* conn = this->connection();
    if (!conn)
    {
        qWarning() << "HostMultipathPage::saveSettings: No connection available";
        return nullptr;
    }

    QSharedPointer<Host> host = QSharedPointer<Host>(new Host(conn, this->m_hostRef));
    
    // Create and return action
    bool enableMultipath = this->ui->multipathCheckBox->isChecked();

    // TODO check if this really works, the actions shouldn't be owned, this could lead to segfault
    return new EditMultipathAction(host, enableMultipath, this);
}

bool HostMultipathPage::IsValidToSave() const
{
    // Always valid - checkbox controls the setting
    return true;
}

void HostMultipathPage::ShowLocalValidationMessages()
{
    // No validation messages needed
}

void HostMultipathPage::HideLocalValidationMessages()
{
    // No validation messages to hide
}

void HostMultipathPage::Cleanup()
{
    // No cleanup needed
}

bool HostMultipathPage::HasChanged() const
{
    return this->ui->multipathCheckBox->isChecked() != this->m_originalMultipathEnabled;
}

void HostMultipathPage::onMultipathCheckBoxChanged(int state)
{
    Q_UNUSED(state);
    this->updateMaintenanceWarning();
}

void HostMultipathPage::updateMaintenanceWarning()
{
    // Match C# HostMultipathPage.UpdateMaintenanceWarning()
    // Show warning if NOT in maintenance mode
    bool inMaintenanceMode = this->isInMaintenanceMode();

    this->ui->maintenanceWarningImage->setVisible(!inMaintenanceMode);
    this->ui->maintenanceWarningLabel->setVisible(!inMaintenanceMode);
    this->ui->multipathCheckBox->setEnabled(inMaintenanceMode);
}

bool HostMultipathPage::isInMaintenanceMode() const
{
    // Match C# HostMultipathPage.MaintenanceMode getter
    // Check if host is in maintenance mode or not live
    
    // Check enabled field (false = maintenance mode)
    bool enabled = this->m_objectDataCopy.value("enabled", true).toBool();
    if (!enabled)
        return true; // In maintenance mode

    // Check metrics to see if host is live
    // In C#: Host_metrics metrics = host.Connection.Resolve(host.metrics);
    // return host.MaintenanceMode() || (metrics != null && !metrics.live);
    QString metricsRef = this->m_objectDataCopy.value("metrics").toString();
    if (!metricsRef.isEmpty() && this->connection())
    {
        XenCache* cache = this->connection()->GetCache();
        if (cache)
        {
            QSharedPointer<HostMetrics> metrics = cache->ResolveObject<HostMetrics>("host_metrics", metricsRef);
            if (metrics && !metrics->live())
                return true; // Host metrics indicate not live (equivalent to maintenance)
        }
    }

    return false; // Not in maintenance mode
}
