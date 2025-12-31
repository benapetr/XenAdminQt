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

#include "networkoptionseditpage.h"
#include "ui_networkoptionseditpage.h"
#include "xenlib/xen/actions/pool/setpoolpropertyaction.h"

NetworkOptionsEditPage::NetworkOptionsEditPage(QWidget* parent)
    : IEditPage(parent)
    , ui(new Ui::NetworkOptionsEditPage)
{
    this->ui->setupUi(this);
}

NetworkOptionsEditPage::~NetworkOptionsEditPage()
{
    delete this->ui;
}

QString NetworkOptionsEditPage::text() const
{
    return tr("Network Options");
}

QString NetworkOptionsEditPage::subText() const
{
    return this->ui->radioButtonEnable->isChecked() 
        ? tr("IGMP snooping enabled") 
        : tr("IGMP snooping disabled");
}

QIcon NetworkOptionsEditPage::image() const
{
    return QIcon(":/icons/network_16.png");
}

void NetworkOptionsEditPage::setXenObjects(const QString& objectRef,
                                           const QString& objectType,
                                           const QVariantMap& objectDataBefore,
                                           const QVariantMap& objectDataCopy)
{
    this->m_poolRef_ = objectRef;
    this->m_objectDataBefore_ = objectDataBefore;
    this->m_objectDataCopy_ = objectDataCopy;

    // Read igmp_snooping_enabled property from pool
    bool enabled = objectDataCopy.value("igmp_snooping_enabled", false).toBool();
    
    if (enabled)
    {
        this->ui->radioButtonEnable->setChecked(true);
    } else
    {
        this->ui->radioButtonDisable->setChecked(true);
    }
}

AsyncOperation* NetworkOptionsEditPage::saveSettings()
{
    bool enableValue = this->ui->radioButtonEnable->isChecked();
    
    QString title = enableValue 
        ? tr("Enabling IGMP snooping") 
        : tr("Disabling IGMP snooping");
    
    return new SetPoolPropertyAction(
        this->connection(),
        this->m_poolRef_,
        "igmp_snooping_enabled",
        enableValue,
        title,
        this
    );
}

bool NetworkOptionsEditPage::hasChanged() const
{
    bool originalEnabled = this->m_objectDataBefore_.value("igmp_snooping_enabled", false).toBool();
    bool currentEnabled = this->ui->radioButtonEnable->isChecked();
    return originalEnabled != currentEnabled;
}

bool NetworkOptionsEditPage::isValidToSave() const
{
    return true;
}

void NetworkOptionsEditPage::showLocalValidationMessages()
{
    // No validation needed
}

void NetworkOptionsEditPage::hideLocalValidationMessages()
{
    // No validation needed
}

void NetworkOptionsEditPage::cleanup()
{
    // Nothing to clean up
}
