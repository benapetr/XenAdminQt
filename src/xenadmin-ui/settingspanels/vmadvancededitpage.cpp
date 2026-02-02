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

#include "vmadvancededitpage.h"
#include "ui_vmadvancededitpage.h"
#include "../../xenlib/xen/actions/delegatedasyncoperation.h"
#include "../../xenlib/xen/network/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/xenapi/xenapi_VM.h"
#include <QToolTip>

VMAdvancedEditPage::VMAdvancedEditPage(QWidget* parent)
    : IEditPage(parent),
      ui(new Ui::VMAdvancedEditPage),
      m_originalShadowMultiplier(1.0),
      m_showCpsOptimisation(true)
{
    this->ui->setupUi(this);

    connect(this->ui->GeneralOptimizationRadioButton, &QRadioButton::toggled, this, &VMAdvancedEditPage::onGeneralRadioToggled);
    connect(this->ui->CPSOptimizationRadioButton, &QRadioButton::toggled, this, &VMAdvancedEditPage::onCitrixRadioToggled);
    connect(this->ui->ManualOptimizationRadioButton, &QRadioButton::toggled, this, &VMAdvancedEditPage::onManualRadioToggled);
    connect(this->ui->ShadowMultiplierTextBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &VMAdvancedEditPage::onShadowMultiplierChanged);
}

VMAdvancedEditPage::~VMAdvancedEditPage()
{
    delete this->ui;
}

QString VMAdvancedEditPage::GetText() const
{
    return tr("Advanced");
}

QString VMAdvancedEditPage::GetSubText() const
{
    if (this->ui->GeneralOptimizationRadioButton->isChecked())
        return this->ui->GeneralOptimizationRadioButton->text().remove('&');

    if (this->ui->CPSOptimizationRadioButton->isChecked())
        return this->ui->CPSOptimizationRadioButton->text().remove('&');

    return tr("Shadow memory multiplier: %1").arg(this->ui->ShadowMultiplierTextBox->text());
}

QIcon VMAdvancedEditPage::GetImage() const
{
    return QIcon(":/icons/configure_16.png");
}

void VMAdvancedEditPage::SetXenObjects(const QString& objectRef,
                                       const QString& objectType,
                                       const QVariantMap& objectDataBefore,
                                       const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectType);

    this->m_vmRef = objectRef;
    this->m_objectDataCopy = objectDataCopy;
    this->m_powerState = objectDataBefore.value("power_state").toString();

    this->ui->CPSOptimizationRadioButton->setVisible(this->m_showCpsOptimisation);

    this->m_originalShadowMultiplier = objectDataCopy.value("HVM_shadow_multiplier").toDouble();
    if (this->m_originalShadowMultiplier < 1.0)
        this->m_originalShadowMultiplier = 1.0;

    bool isSuspendedOrPaused = (this->m_powerState == "Suspended" || this->m_powerState == "Paused");
    this->ui->GeneralOptimizationRadioButton->setEnabled(!isSuspendedOrPaused);
    this->ui->CPSOptimizationRadioButton->setEnabled(!isSuspendedOrPaused);
    this->ui->ManualOptimizationRadioButton->setEnabled(!isSuspendedOrPaused);
    this->ui->labelShadowMultiplier->setEnabled(!isSuspendedOrPaused);
    this->ui->ShadowMultiplierTextBox->setEnabled(!isSuspendedOrPaused);
    this->ui->iconWarning->setVisible(isSuspendedOrPaused);
    this->ui->labelWarning->setVisible(isSuspendedOrPaused);

    if (qAbs(this->m_originalShadowMultiplier - SHADOW_MULTIPLIER_GENERAL) < 0.0001)
    {
        this->ui->GeneralOptimizationRadioButton->setChecked(true);
    }
    else if (qAbs(this->m_originalShadowMultiplier - SHADOW_MULTIPLIER_CPS) < 0.0001 && this->m_showCpsOptimisation)
    {
        this->ui->CPSOptimizationRadioButton->setChecked(true);
    }
    else
    {
        this->ui->ManualOptimizationRadioButton->setChecked(true);
    }

    this->ui->ShadowMultiplierTextBox->setValue(this->m_originalShadowMultiplier);
}

AsyncOperation* VMAdvancedEditPage::SaveSettings()
{
    if (!this->HasChanged())
        return nullptr;

    double newMultiplier = this->getCurrentShadowMultiplier();

    if (this->m_powerState == "Running")
    {
        QString vmName = this->m_objectDataCopy.value("name_label").toString();
        auto* op = new DelegatedAsyncOperation(
            this->m_connection,
            tr("Change shadow multiplier"),
            tr("Changing shadow multiplier for '%1'...").arg(vmName),
            [vmRef = this->m_vmRef, newMultiplier](DelegatedAsyncOperation* self) {
                XenAPI::Session* session = self->GetConnection()->GetSession();
                if (!session || !session->IsLoggedIn())
                    throw std::runtime_error("No valid session");
                XenAPI::VM::set_shadow_multiplier_live(session, vmRef, newMultiplier);
            },
            this);
        op->AddApiMethodToRoleCheck("vm.set_shadow_multiplier_live");
        return op;
    }

    this->m_objectDataCopy["HVM_shadow_multiplier"] = newMultiplier;
    return nullptr;
}

bool VMAdvancedEditPage::IsValidToSave() const
{
    return this->getCurrentShadowMultiplier() >= 1.0;
}

void VMAdvancedEditPage::ShowLocalValidationMessages()
{
    if (!IsValidToSave())
    {
        QToolTip::showText(
            this->ui->ShadowMultiplierTextBox->mapToGlobal(QPoint(0, this->ui->ShadowMultiplierTextBox->height())),
            tr("Value should be a number greater than or equal to 1.0"),
            this->ui->ShadowMultiplierTextBox);
        this->ui->ShadowMultiplierTextBox->setFocus();
    }
}

void VMAdvancedEditPage::HideLocalValidationMessages()
{
    QToolTip::hideText();
}

void VMAdvancedEditPage::Cleanup()
{
    QToolTip::hideText();
}

bool VMAdvancedEditPage::HasChanged() const
{
    return qAbs(this->getCurrentShadowMultiplier() - this->m_originalShadowMultiplier) > 0.0001;
}

QVariantMap VMAdvancedEditPage::GetModifiedObjectData() const
{
    QVariantMap data;
    if (this->m_powerState != "Running" && this->HasChanged())
        data["HVM_shadow_multiplier"] = this->getCurrentShadowMultiplier();
    return data;
}

void VMAdvancedEditPage::onGeneralRadioToggled(bool checked)
{
    if (checked)
        this->ui->ShadowMultiplierTextBox->setValue(SHADOW_MULTIPLIER_GENERAL);
}

void VMAdvancedEditPage::onCitrixRadioToggled(bool checked)
{
    if (checked)
        this->ui->ShadowMultiplierTextBox->setValue(SHADOW_MULTIPLIER_CPS);
}

void VMAdvancedEditPage::onManualRadioToggled(bool checked)
{
    Q_UNUSED(checked);
}

void VMAdvancedEditPage::onShadowMultiplierChanged(double value)
{
    Q_UNUSED(value);

    if (this->ui->ShadowMultiplierTextBox->hasFocus())
        this->ui->ManualOptimizationRadioButton->setChecked(true);
}

double VMAdvancedEditPage::getCurrentShadowMultiplier() const
{
    return this->ui->ShadowMultiplierTextBox->value();
}
