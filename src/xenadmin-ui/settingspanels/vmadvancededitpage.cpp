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
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/xenapi/xenapi_VM.h"
#include <QToolTip>

VMAdvancedEditPage::VMAdvancedEditPage(QWidget* parent)
    : IEditPage(parent),
      ui(new Ui::VMAdvancedEditPage),
      m_originalShadowMultiplier(1.0),
      m_showCpsOptimisation(true)
{
    ui->setupUi(this);

    connect(ui->GeneralOptimizationRadioButton, &QRadioButton::toggled,
            this, &VMAdvancedEditPage::onGeneralRadioToggled);
    connect(ui->CPSOptimizationRadioButton, &QRadioButton::toggled,
            this, &VMAdvancedEditPage::onCitrixRadioToggled);
    connect(ui->ManualOptimizationRadioButton, &QRadioButton::toggled,
            this, &VMAdvancedEditPage::onManualRadioToggled);
    connect(ui->ShadowMultiplierTextBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &VMAdvancedEditPage::onShadowMultiplierChanged);
}

VMAdvancedEditPage::~VMAdvancedEditPage()
{
    delete ui;
}

QString VMAdvancedEditPage::text() const
{
    return tr("Advanced");
}

QString VMAdvancedEditPage::subText() const
{
    if (ui->GeneralOptimizationRadioButton->isChecked())
        return ui->GeneralOptimizationRadioButton->text().remove('&');

    if (ui->CPSOptimizationRadioButton->isChecked())
        return ui->CPSOptimizationRadioButton->text().remove('&');

    return tr("Shadow memory multiplier: %1").arg(ui->ShadowMultiplierTextBox->text());
}

QIcon VMAdvancedEditPage::image() const
{
    return QIcon::fromTheme("preferences-system");
}

void VMAdvancedEditPage::setXenObjects(const QString& objectRef,
                                       const QString& objectType,
                                       const QVariantMap& objectDataBefore,
                                       const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectType);

    m_vmRef = objectRef;
    m_objectDataCopy = objectDataCopy;
    m_powerState = objectDataBefore.value("power_state").toString();

    ui->CPSOptimizationRadioButton->setVisible(m_showCpsOptimisation);

    m_originalShadowMultiplier = objectDataCopy.value("HVM_shadow_multiplier").toDouble();
    if (m_originalShadowMultiplier < 1.0)
        m_originalShadowMultiplier = 1.0;

    bool isSuspendedOrPaused = (m_powerState == "Suspended" || m_powerState == "Paused");
    ui->GeneralOptimizationRadioButton->setEnabled(!isSuspendedOrPaused);
    ui->CPSOptimizationRadioButton->setEnabled(!isSuspendedOrPaused);
    ui->ManualOptimizationRadioButton->setEnabled(!isSuspendedOrPaused);
    ui->labelShadowMultiplier->setEnabled(!isSuspendedOrPaused);
    ui->ShadowMultiplierTextBox->setEnabled(!isSuspendedOrPaused);
    ui->iconWarning->setVisible(isSuspendedOrPaused);
    ui->labelWarning->setVisible(isSuspendedOrPaused);

    if (qAbs(m_originalShadowMultiplier - SHADOW_MULTIPLIER_GENERAL) < 0.0001)
    {
        ui->GeneralOptimizationRadioButton->setChecked(true);
    }
    else if (qAbs(m_originalShadowMultiplier - SHADOW_MULTIPLIER_CPS) < 0.0001 && m_showCpsOptimisation)
    {
        ui->CPSOptimizationRadioButton->setChecked(true);
    }
    else
    {
        ui->ManualOptimizationRadioButton->setChecked(true);
    }

    ui->ShadowMultiplierTextBox->setValue(m_originalShadowMultiplier);
}

AsyncOperation* VMAdvancedEditPage::saveSettings()
{
    if (!hasChanged())
        return nullptr;

    double newMultiplier = getCurrentShadowMultiplier();

    if (m_powerState == "Running")
    {
        QString vmName = m_objectDataCopy.value("name_label").toString();
        auto* op = new DelegatedAsyncOperation(
            m_connection,
            tr("Change shadow multiplier"),
            tr("Changing shadow multiplier for '%1'...").arg(vmName),
            [vmRef = m_vmRef, newMultiplier](DelegatedAsyncOperation* self) {
                XenSession* session = self->connection()->getSession();
                if (!session || !session->isLoggedIn())
                    throw std::runtime_error("No valid session");
                XenAPI::VM::set_shadow_multiplier_live(session, vmRef, newMultiplier);
            },
            this);
        op->addApiMethodToRoleCheck("vm.set_shadow_multiplier_live");
        return op;
    }

    m_objectDataCopy["HVM_shadow_multiplier"] = newMultiplier;
    return nullptr;
}

bool VMAdvancedEditPage::isValidToSave() const
{
    return getCurrentShadowMultiplier() >= 1.0;
}

void VMAdvancedEditPage::showLocalValidationMessages()
{
    if (!isValidToSave())
    {
        QToolTip::showText(
            ui->ShadowMultiplierTextBox->mapToGlobal(QPoint(0, ui->ShadowMultiplierTextBox->height())),
            tr("Value should be a number greater than or equal to 1.0"),
            ui->ShadowMultiplierTextBox);
        ui->ShadowMultiplierTextBox->setFocus();
    }
}

void VMAdvancedEditPage::hideLocalValidationMessages()
{
    QToolTip::hideText();
}

void VMAdvancedEditPage::cleanup()
{
    QToolTip::hideText();
}

bool VMAdvancedEditPage::hasChanged() const
{
    return qAbs(getCurrentShadowMultiplier() - m_originalShadowMultiplier) > 0.0001;
}

QVariantMap VMAdvancedEditPage::getModifiedObjectData() const
{
    QVariantMap data;
    if (m_powerState != "Running" && hasChanged())
        data["HVM_shadow_multiplier"] = getCurrentShadowMultiplier();
    return data;
}

void VMAdvancedEditPage::onGeneralRadioToggled(bool checked)
{
    if (checked)
        ui->ShadowMultiplierTextBox->setValue(SHADOW_MULTIPLIER_GENERAL);
}

void VMAdvancedEditPage::onCitrixRadioToggled(bool checked)
{
    if (checked)
        ui->ShadowMultiplierTextBox->setValue(SHADOW_MULTIPLIER_CPS);
}

void VMAdvancedEditPage::onManualRadioToggled(bool checked)
{
    Q_UNUSED(checked);
}

void VMAdvancedEditPage::onShadowMultiplierChanged(double value)
{
    Q_UNUSED(value);

    if (ui->ShadowMultiplierTextBox->hasFocus())
        ui->ManualOptimizationRadioButton->setChecked(true);
}

double VMAdvancedEditPage::getCurrentShadowMultiplier() const
{
    return ui->ShadowMultiplierTextBox->value();
}
