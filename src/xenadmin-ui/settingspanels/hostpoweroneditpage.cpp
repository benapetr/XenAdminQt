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

#include "hostpoweroneditpage.h"
#include "ui_hostpoweroneditpage.h"
#include "xenlib/xen/actions/host/savepoweronsettingsaction.h"
#include "xenlib/xen/api.h"
#include "xenlib/xen/xenapi/xenapi_Secret.h"
#include "xenlib/xen/apiversion.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/xenobject.h"
#include <QMessageBox>
#include <QHostAddress>
#include <QTableWidgetItem>
#include <QToolTip>

// PowerOnMode methods are moved to a separate helper since they're shared with action
namespace
{
    QString powerOnModeToString(const PowerOnMode& mode)
    {
        return mode.toString();
    }
    
    QString powerOnModeFriendlyName(const PowerOnMode& mode)
    {
        switch (mode.type)
        {
            case PowerOnMode::Disabled: return HostPowerONEditPage::tr("Disabled");
            case PowerOnMode::WakeOnLAN: return HostPowerONEditPage::tr("Wake-on-LAN");
            case PowerOnMode::iLO: return "iLO";
            case PowerOnMode::DRAC: return "DRAC";
            case PowerOnMode::Custom: return mode.customMode.isEmpty() ? HostPowerONEditPage::tr("Custom") : mode.customMode;
        }
        return HostPowerONEditPage::tr("Unknown");
    }
    
    PowerOnMode powerOnModeFromHostData(const QVariantMap& powerOnConfig, const QString& powerOnModeStr)
    {
        PowerOnMode mode;
        
        if (powerOnModeStr.isEmpty())
        {
            mode.type = PowerOnMode::Disabled;
        } else if (powerOnModeStr == "wake-on-lan")
        {
            mode.type = PowerOnMode::WakeOnLAN;
        } else if (powerOnModeStr == "iLO")
        {
            mode.type = PowerOnMode::iLO;
            mode.ipAddress = powerOnConfig.value("power_on_ip").toString();
            mode.username = powerOnConfig.value("power_on_user").toString();
            mode.passwordSecretUuid = powerOnConfig.value("power_on_password_secret").toString();
        } else if (powerOnModeStr == "DRAC")
        {
            mode.type = PowerOnMode::DRAC;
            mode.ipAddress = powerOnConfig.value("power_on_ip").toString();
            mode.username = powerOnConfig.value("power_on_user").toString();
            mode.passwordSecretUuid = powerOnConfig.value("power_on_password_secret").toString();
        } else
        {
            mode.type = PowerOnMode::Custom;
            mode.customMode = powerOnModeStr;
            // Load custom config
            for (auto it = powerOnConfig.begin(); it != powerOnConfig.end(); ++it)
            {
                mode.customConfig[it.key()] = it.value().toString();
            }
            mode.passwordSecretUuid = powerOnConfig.value("power_on_password_secret").toString();
        }
        
        return mode;
    }
}

// HostPowerONEditPage implementation
HostPowerONEditPage::HostPowerONEditPage(QWidget *parent)
    : IEditPage(parent)
    , ui(new Ui::HostPowerONEditPage)
    , m_programmaticUpdate(false)
    , m_loaded(false)
    , m_validationWidget(nullptr)
{
    this->ui->setupUi(this);
    
    // Connect radio button signals
    connect(this->ui->radioDisabled, &QRadioButton::toggled, this, &HostPowerONEditPage::onRadioDisabledToggled);
    connect(this->ui->radioWakeOnLAN, &QRadioButton::toggled, this, &HostPowerONEditPage::onRadioWakeOnLANToggled);
    connect(this->ui->radioILO, &QRadioButton::toggled, this, &HostPowerONEditPage::onRadioILOToggled);
    connect(this->ui->radioDRAC, &QRadioButton::toggled, this, &HostPowerONEditPage::onRadioDRACToggled);
    connect(this->ui->radioCustom, &QRadioButton::toggled, this, &HostPowerONEditPage::onRadioCustomToggled);
    
    // Connect text field signals
    connect(this->ui->textInterface, &QLineEdit::textChanged, this, &HostPowerONEditPage::onTextInterfaceChanged);
    connect(this->ui->textUser, &QLineEdit::textChanged, this, &HostPowerONEditPage::onTextUserChanged);
    connect(this->ui->textPassword, &QLineEdit::textChanged, this, &HostPowerONEditPage::onTextPasswordChanged);
    connect(this->ui->textCustomMode, &QLineEdit::textChanged, this, &HostPowerONEditPage::onTextCustomModeChanged);
    
    // Connect table signals
    connect(this->ui->tableCustomParams, &QTableWidget::cellChanged, this, &HostPowerONEditPage::onCustomParamsCellChanged);
    
    // Initialize table
    this->ui->tableCustomParams->horizontalHeader()->setStretchLastSection(true);
    
    // Hide pool-specific UI (single host mode only for now)
    this->ui->labelPool->setVisible(false);
    this->ui->hostsWidget->setVisible(false);
    
    // Default to disabled mode
    this->ui->radioDisabled->setChecked(true);
    this->updateUIForMode(PowerOnMode::Disabled);
}

HostPowerONEditPage::~HostPowerONEditPage()
{
    delete this->ui;
}

QString HostPowerONEditPage::GetText() const
{
    return tr("Power On");
}

QString HostPowerONEditPage::GetSubText() const
{
    return powerOnModeFriendlyName(this->m_currentMode);
}

QIcon HostPowerONEditPage::GetImage() const
{
    // Matches C# Images.StaticImages._001_PowerOn_h32bit_16
    return QIcon(":/icons/power_on.png");
}

void HostPowerONEditPage::SetXenObject(QSharedPointer<XenObject> object,
                                       const QVariantMap& objectDataBefore,
                                       const QVariantMap& objectDataCopy)
{
    this->m_object = object;
    this->m_hostRef.clear();
    this->m_objectDataBefore.clear();
    this->m_objectDataCopy.clear();
    this->m_currentMode = PowerOnMode();
    this->m_originalMode = PowerOnMode();
    this->m_loaded = false;

    if (object.isNull() || object->GetObjectType() != XenObjectType::Host)
        return;

    this->m_hostRef = object->OpaqueRef();
    this->m_objectDataBefore = objectDataBefore;
    this->m_objectDataCopy = objectDataCopy;
    
    // Load power-on mode from host data
    QString powerOnModeStr = objectDataCopy.value("power_on_mode").toString();
    QVariantMap powerOnConfig = objectDataCopy.value("power_on_config").toMap();
    
    this->m_originalMode = powerOnModeFromHostData(powerOnConfig, powerOnModeStr);
    this->m_currentMode = this->m_originalMode;

    // C# Host.PowerOnMode.Load resolves password secrets when available.
    if (this->connection() && this->connection()->GetSession())
    {
        auto loadSecret = [this](PowerOnMode& mode)
        {
            if (mode.passwordSecretUuid.isEmpty())
                return;
            try
            {
                QString secretRef = XenAPI::Secret::get_by_uuid(this->connection()->GetSession(), mode.passwordSecretUuid);
                if (!secretRef.isEmpty())
                {
                    XenRpcAPI api(this->connection()->GetSession());
                    QVariantList params;
                    params << this->connection()->GetSession()->GetSessionID() << secretRef;
                    QByteArray request = api.BuildJsonRpcCall("secret.get_value", params);
                    QByteArray response = this->connection()->SendRequest(request);
                    mode.password = api.ParseJsonRpcResponse(response).toString();
                }
            }
            catch (...)
            {
                // Keep empty password if secret cannot be read.
                mode.password.clear();
            }
        };
        loadSecret(this->m_originalMode);
        loadSecret(this->m_currentMode);
    }

    // C# defensively disables iLO on Stockholm+.
    if (this->connection() && this->connection()->GetSession() &&
        this->connection()->GetSession()->ApiVersionMeets(APIVersion::API_2_15))
    {
        this->ui->radioILO->setVisible(false);
        if (this->m_currentMode.type == PowerOnMode::iLO)
        {
            this->m_currentMode.type = PowerOnMode::Disabled;
            this->m_currentMode.ipAddress.clear();
            this->m_currentMode.username.clear();
            this->m_currentMode.password.clear();
            this->m_currentMode.passwordSecretUuid.clear();
        }
        if (this->m_originalMode.type == PowerOnMode::iLO)
        {
            this->m_originalMode.type = PowerOnMode::Disabled;
            this->m_originalMode.ipAddress.clear();
            this->m_originalMode.username.clear();
            this->m_originalMode.password.clear();
            this->m_originalMode.passwordSecretUuid.clear();
        }
    } else
    {
        this->ui->radioILO->setVisible(true);
    }
    
    // Update UI to show current mode
    this->m_programmaticUpdate = true;
    
    switch (this->m_currentMode.type)
    {
        case PowerOnMode::Disabled:
            this->ui->radioDisabled->setChecked(true);
            break;
        case PowerOnMode::WakeOnLAN:
            this->ui->radioWakeOnLAN->setChecked(true);
            break;
        case PowerOnMode::iLO:
            this->ui->radioILO->setChecked(true);
            this->ui->textInterface->setText(this->m_currentMode.ipAddress);
            this->ui->textUser->setText(this->m_currentMode.username);
            this->ui->textPassword->setText(this->m_currentMode.password);
            break;
        case PowerOnMode::DRAC:
            this->ui->radioDRAC->setChecked(true);
            this->ui->textInterface->setText(this->m_currentMode.ipAddress);
            this->ui->textUser->setText(this->m_currentMode.username);
            this->ui->textPassword->setText(this->m_currentMode.password);
            break;
        case PowerOnMode::Custom:
            this->ui->radioCustom->setChecked(true);
            this->ui->textCustomMode->setText(this->m_currentMode.customMode);
            // Load custom params
            this->ui->tableCustomParams->setRowCount(0);
            for (auto it = this->m_currentMode.customConfig.begin(); it != this->m_currentMode.customConfig.end(); ++it)
            {
                int row = this->ui->tableCustomParams->rowCount();
                this->ui->tableCustomParams->insertRow(row);
                this->ui->tableCustomParams->setItem(row, 0, new QTableWidgetItem(it.key()));
                this->ui->tableCustomParams->setItem(row, 1, new QTableWidgetItem(it.value()));
            }
            break;
    }
    
    this->m_programmaticUpdate = false;
    this->m_loaded = true;
    
    emit this->populated();
}

AsyncOperation* HostPowerONEditPage::SaveSettings()
{
    if (!this->HasChanged())
        return nullptr;
    
    QList<QPair<QString, PowerOnMode>> changedHosts;
    changedHosts.append(qMakePair(this->m_hostRef, this->m_currentMode));
    
    return new SavePowerOnSettingsAction(this->connection(), changedHosts, this);
}

bool HostPowerONEditPage::HasChanged() const
{
    if (!this->m_loaded)
        return false;
    
    return this->hasHostChanged(this->m_originalMode, this->m_currentMode);
}

bool HostPowerONEditPage::hasHostChanged(const PowerOnMode& originalMode, const PowerOnMode& currentMode) const
{
    if (originalMode.type != currentMode.type)
        return true;
    
    if (powerOnModeToString(originalMode) != powerOnModeToString(currentMode))
        return true;
    
    if (currentMode.type == PowerOnMode::iLO || currentMode.type == PowerOnMode::DRAC)
    {
        if (originalMode.ipAddress != currentMode.ipAddress ||
            originalMode.username != currentMode.username ||
            originalMode.password != currentMode.password)
            return true;
    }
    
    if (currentMode.type == PowerOnMode::Custom)
    {
        if (originalMode.customConfig != currentMode.customConfig)
            return true;
    }
    
    return false;
}

bool HostPowerONEditPage::IsValidToSave() const
{
    if (!this->m_loaded)
        return true;
    
    // Reset validation state
    const_cast<HostPowerONEditPage*>(this)->m_validationMessage.clear();
    const_cast<HostPowerONEditPage*>(this)->m_validationWidget = nullptr;
    
    if (this->m_currentMode.type == PowerOnMode::iLO || this->m_currentMode.type == PowerOnMode::DRAC)
    {
        // Validate IP address
        // Matches C# StringUtility.IsIPAddress() check
        QHostAddress addr(this->m_currentMode.ipAddress);
        if (addr.isNull())
        {
            const_cast<HostPowerONEditPage*>(this)->m_validationMessage = tr("Invalid IP address");
            const_cast<HostPowerONEditPage*>(this)->m_validationWidget = this->ui->textInterface;
            return false;
        }
    }
    
    if (this->m_currentMode.type == PowerOnMode::Custom && this->m_currentMode.customMode.trimmed().isEmpty())
    {
        const_cast<HostPowerONEditPage*>(this)->m_validationMessage = tr("Please specify a custom power-on mode");
        const_cast<HostPowerONEditPage*>(this)->m_validationWidget = this->ui->textCustomMode;
        return false;
    }
    
    return true;
}

void HostPowerONEditPage::Cleanup()
{
    // Nothing to cleanup
}

void HostPowerONEditPage::onRadioDisabledToggled(bool checked)
{
    if (!checked)
        return;
    
    this->updateUIForMode(PowerOnMode::Disabled);
    
    if (this->m_programmaticUpdate)
        return;
    
    this->m_currentMode.type = PowerOnMode::Disabled;
}

void HostPowerONEditPage::onRadioWakeOnLANToggled(bool checked)
{
    if (!checked)
        return;
    
    this->updateUIForMode(PowerOnMode::WakeOnLAN);
    
    if (this->m_programmaticUpdate)
        return;
    
    this->m_currentMode.type = PowerOnMode::WakeOnLAN;
}

void HostPowerONEditPage::onRadioILOToggled(bool checked)
{
    if (!checked)
        return;
    
    this->updateUIForMode(PowerOnMode::iLO);
    
    if (this->m_programmaticUpdate)
        return;
    
    this->m_currentMode.type = PowerOnMode::iLO;
    this->updateModeFromCredentials();
}

void HostPowerONEditPage::onRadioDRACToggled(bool checked)
{
    if (!checked)
        return;
    
    this->updateUIForMode(PowerOnMode::DRAC);
    
    if (this->m_programmaticUpdate)
        return;
    
    this->m_currentMode.type = PowerOnMode::DRAC;
    this->updateModeFromCredentials();
}

void HostPowerONEditPage::onRadioCustomToggled(bool checked)
{
    if (!checked)
        return;
    
    this->updateUIForMode(PowerOnMode::Custom);
    
    if (this->m_programmaticUpdate)
        return;
    
    this->m_currentMode.type = PowerOnMode::Custom;
    this->updateModeFromCustom();
}

void HostPowerONEditPage::onTextInterfaceChanged(const QString& text)
{
    Q_UNUSED(text);
    if (!this->m_programmaticUpdate)
        this->updateModeFromCredentials();
}

void HostPowerONEditPage::onTextUserChanged(const QString& text)
{
    Q_UNUSED(text);
    if (!this->m_programmaticUpdate)
        this->updateModeFromCredentials();
}

void HostPowerONEditPage::onTextPasswordChanged(const QString& text)
{
    Q_UNUSED(text);
    if (!this->m_programmaticUpdate)
        this->updateModeFromCredentials();
}

void HostPowerONEditPage::onTextCustomModeChanged(const QString& text)
{
    Q_UNUSED(text);
    if (!this->m_programmaticUpdate)
        this->updateModeFromCustom();
}

void HostPowerONEditPage::onCustomParamsCellChanged(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    if (!this->m_programmaticUpdate)
        this->updateModeFromCustom();
}

void HostPowerONEditPage::updateModeFromCredentials()
{
    if (this->m_currentMode.type == PowerOnMode::iLO || this->m_currentMode.type == PowerOnMode::DRAC) {
        this->m_currentMode.ipAddress = this->ui->textInterface->text();
        this->m_currentMode.username = this->ui->textUser->text();
        const QString newPassword = this->ui->textPassword->text();
        if (!newPassword.isEmpty())
        {
            this->m_currentMode.password = newPassword;
            this->m_currentMode.passwordSecretUuid.clear();
        }
    }
}

void HostPowerONEditPage::updateModeFromCustom()
{
    if (this->m_currentMode.type == PowerOnMode::Custom)
    {
        this->m_currentMode.customMode = this->ui->textCustomMode->text();
        
        // Update custom config from table
        this->m_currentMode.customConfig.clear();
        this->m_currentMode.passwordSecretUuid.clear();
        for (int row = 0; row < this->ui->tableCustomParams->rowCount(); ++row)
        {
            QTableWidgetItem* keyItem = this->ui->tableCustomParams->item(row, 0);
            QTableWidgetItem* valueItem = this->ui->tableCustomParams->item(row, 1);
            
            if (keyItem && valueItem)
            {
                QString key = keyItem->text().trimmed();
                QString value = valueItem->text().trimmed();
                
                if (!key.isEmpty() && !value.isEmpty())
                {
                    this->m_currentMode.customConfig[key] = value;
                    if (key == "power_on_password_secret")
                        this->m_currentMode.passwordSecretUuid = value;
                }
            }
        }
    }
}

void HostPowerONEditPage::updateUIForMode(PowerOnMode::Type type)
{
    switch (type)
    {
        case PowerOnMode::Disabled:
        case PowerOnMode::WakeOnLAN:
            this->ui->groupBoxCredentials->setVisible(false);
            this->ui->textCustomMode->setVisible(false);
            break;
        case PowerOnMode::iLO:
        case PowerOnMode::DRAC:
            this->ui->groupBoxCredentials->setVisible(true);
            this->ui->credentialsWidget->setVisible(true);
            this->ui->tableCustomParams->setVisible(false);
            this->ui->textCustomMode->setVisible(false);
            break;
        case PowerOnMode::Custom:
            this->ui->groupBoxCredentials->setVisible(true);
            this->ui->credentialsWidget->setVisible(false);
            this->ui->tableCustomParams->setVisible(true);
            this->ui->textCustomMode->setVisible(true);
            break;
    }
}

void HostPowerONEditPage::ShowLocalValidationMessages()
{
    // Show validation errors as tooltip near the invalid widget
    // Matches C# HelpersGUI.ShowBalloonMessage(ctrl, _invalidParamToolTip)
    if (this->m_validationWidget && !this->m_validationMessage.isEmpty())
    {
        QToolTip::showText(
            this->m_validationWidget->mapToGlobal(QPoint(0, this->m_validationWidget->height())),
            this->m_validationMessage,
            this->m_validationWidget
        );
        
        // Optionally set focus to the invalid field
        this->m_validationWidget->setFocus();
    }
}

void HostPowerONEditPage::HideLocalValidationMessages()
{
    // Hide tooltip
    // Matches C# _invalidParamToolTip.Hide(ctrl)
    if (this->m_validationWidget)
    {
        QToolTip::hideText();
    }
}
