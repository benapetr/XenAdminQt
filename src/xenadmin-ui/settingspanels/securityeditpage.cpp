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

#include "securityeditpage.h"
#include "ui_securityeditpage.h"
#include "../../xenlib/xen/actions/pool/setssllegacyaction.h"

SecurityEditPage::SecurityEditPage(QWidget* parent)
    : IEditPage(parent)
    , ui(new Ui::SecurityEditPage)
    , m_isHost_(false)
{
    this->ui->setupUi(this);

    connect(this->ui->radioButtonTLS, &QRadioButton::toggled, this, &SecurityEditPage::onRadioButtonChanged);
    connect(this->ui->radioButtonSSL, &QRadioButton::toggled, this, &SecurityEditPage::onRadioButtonChanged);
}

SecurityEditPage::~SecurityEditPage()
{
    delete this->ui;
}

QString SecurityEditPage::text() const
{
    return tr("Security");
}

QString SecurityEditPage::subText() const
{
    return this->ui->radioButtonTLS->isChecked() 
        ? tr("TLS verification enabled") 
        : tr("SSL legacy protocol");
}

QIcon SecurityEditPage::image() const
{
    return QIcon(":/icons/padlock.png");
}

void SecurityEditPage::setXenObjects(const QString& objectRef,
                                     const QString& objectType,
                                     const QVariantMap& objectDataBefore,
                                     const QVariantMap& objectDataCopy)
{
    this->m_poolRef_ = objectRef;
    this->m_objectDataBefore_ = objectDataBefore;
    this->m_objectDataCopy_ = objectDataCopy;
    this->m_isHost_ = (objectType == "host");

    // Adjust text for host vs pool
    if (this->m_isHost_)
    {
        this->ui->labelRubric->setText(tr("The security mode of this server determines which SSL/TLS protocol versions can be used to connect to this server."));
        this->ui->labelDisruption->setText(tr("Changing this setting will require all hosts in the pool to restart their management services. This will cause temporary connection disruption."));
    } else
    {
        this->ui->labelRubric->setText(tr("The security mode of this pool determines which SSL/TLS protocol versions can be used to connect to servers in this pool."));
        this->ui->labelDisruption->setText(tr("Changing this setting will require all hosts in the pool to restart their management services. This will cause temporary connection disruption."));
    }

    // Read ssl_legacy from pool other_config
    // In XenServer, ssl_legacy() checks: other_config["ssl_legacy"] == "true"
    QVariantMap otherConfig = objectDataCopy.value("other_config").toMap();
    bool sslLegacy = (otherConfig.value("ssl_legacy").toString() == "true");

    if (sslLegacy)
    {
        this->ui->radioButtonSSL->setChecked(true);
    } else
    {
        this->ui->radioButtonTLS->setChecked(true);
    }

    this->updateWarningVisibility();
}

AsyncOperation* SecurityEditPage::saveSettings()
{
    bool enableSslLegacy = this->ui->radioButtonSSL->isChecked();
    
    return new SetSslLegacyAction(        this->connection(),        this->m_poolRef_,
        enableSslLegacy,
        this
    );
}

bool SecurityEditPage::hasChanged() const
{
    QVariantMap otherConfig = this->m_objectDataBefore_.value("other_config").toMap();
    bool originalSslLegacy = (otherConfig.value("ssl_legacy").toString() == "true");
    bool currentSslLegacy = this->ui->radioButtonSSL->isChecked();
    
    return originalSslLegacy != currentSslLegacy;
}

bool SecurityEditPage::isValidToSave() const
{
    return true;
}

void SecurityEditPage::showLocalValidationMessages()
{
    // No validation needed
}

void SecurityEditPage::hideLocalValidationMessages()
{
    // No validation needed
}

void SecurityEditPage::cleanup()
{
    // Nothing to clean up
}

void SecurityEditPage::onRadioButtonChanged()
{
    this->updateWarningVisibility();
}

void SecurityEditPage::updateWarningVisibility()
{
    // Show warning only if settings have changed
    bool showWarning = this->hasChanged();
    this->ui->labelDisruption->setVisible(showWarning);
    this->ui->pictureBoxDisruption->setVisible(showWarning);
}
