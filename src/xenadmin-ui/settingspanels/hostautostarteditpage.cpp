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

#include "hostautostarteditpage.h"
#include "ui_hostautostarteditpage.h"
#include "../../xenlib/xen/actions/host/changehostautostartaction.h"
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/xencache.h"
#include <QIcon>

HostAutostartEditPage::HostAutostartEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::HostAutostartEditPage), m_originalAutostartEnabled(false)
{
    this->ui->setupUi(this);
}

HostAutostartEditPage::~HostAutostartEditPage()
{
    delete this->ui;
}

QString HostAutostartEditPage::text() const
{
    return tr("Autostart");
}

QString HostAutostartEditPage::subText() const
{
    return this->getAutostartEnabled() ? tr("Enabled") : tr("Disabled");
}

QIcon HostAutostartEditPage::image() const
{
    // Matches C# Properties.Resources._000_EnablePowerControl_h32bit_16
    return QIcon(":/icons/enable_power_control_16.png");
}

void HostAutostartEditPage::setXenObjects(const QString& objectRef,
                                          const QString& objectType,
                                          const QVariantMap& objectDataBefore,
                                          const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);

    this->m_hostRef = objectRef;
    this->m_hostType = objectType;

    // m_connection is set by VerticallyTabbedDialog before calling this

    this->repopulate();
}

void HostAutostartEditPage::repopulate()
{
    if (!this->m_connection || this->m_hostRef.isEmpty())
    {
        return;
    }

    // Get autostart enabled state from pool's other_config
    // In C#: host.GetVmAutostartEnabled() -> pool.GetVmAutostartEnabled()
    // This reads pool.other_config["auto_poweron"]
    
    this->m_originalAutostartEnabled = false; // Default to false
    
    XenCache* cache = this->m_connection->getCache();
    if (cache)
    {
        // Get the pool for this host
        // In C#: Pool p = Helpers.GetPoolOfOne(Connection);
        QVariantMap hostData = cache->resolve("host", this->m_hostRef);
        if (!hostData.isEmpty())
        {
            // Get pool reference from host
            QStringList poolRefs = cache->getAllRefs("pool");
            if (!poolRefs.isEmpty())
            {
                // Typically there's only one pool
                QString poolRef = poolRefs.first();
                QVariantMap poolData = cache->resolve("pool", poolRef);
                
                if (!poolData.isEmpty())
                {
                    // Read pool.other_config["auto_poweron"]
                    // In C#: string auto_poweron = Get(other_config, "auto_poweron");
                    QVariantMap otherConfig = poolData.value("other_config").toMap();
                    QString autoPoweron = otherConfig.value("auto_poweron").toString();
                    
                    this->m_originalAutostartEnabled = (autoPoweron == "true");
                }
            }
        }
    }

    this->ui->checkBoxEnableAutostart->setChecked(this->m_originalAutostartEnabled);
}

bool HostAutostartEditPage::getAutostartEnabled() const
{
    return this->ui->checkBoxEnableAutostart->isChecked();
}

AsyncOperation* HostAutostartEditPage::saveSettings()
{
    if (!this->hasChanged())
    {
        return nullptr;
    }

    // Create action to change autostart setting
    return new ChangeHostAutostartAction(
        this->m_connection,
        this->m_hostRef,
        this->ui->checkBoxEnableAutostart->isChecked(),
        true // suppressHistory
    );
}

bool HostAutostartEditPage::isValidToSave() const
{
    // No validation needed - checkbox is always valid
    return true;
}

void HostAutostartEditPage::showLocalValidationMessages()
{
    // No validation messages needed
}

void HostAutostartEditPage::hideLocalValidationMessages()
{
    // No validation messages needed
}

void HostAutostartEditPage::cleanup()
{
    // No cleanup needed
}

bool HostAutostartEditPage::hasChanged() const
{
    return this->ui->checkBoxEnableAutostart->isChecked() != this->m_originalAutostartEnabled;
}
