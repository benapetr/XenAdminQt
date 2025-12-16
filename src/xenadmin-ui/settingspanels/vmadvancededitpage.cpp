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
#include "../../xenlib/xen/asyncoperation.h"
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/api.h"
#include <QMessageBox>

VMAdvancedEditPage::VMAdvancedEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::VMAdvancedEditPage), m_originalShadowMultiplier(1.0)
{
    ui->setupUi(this);

    connect(ui->radioButtonGeneral, &QRadioButton::toggled,
            this, &VMAdvancedEditPage::onGeneralRadioToggled);
    connect(ui->radioButtonCitrix, &QRadioButton::toggled,
            this, &VMAdvancedEditPage::onCitrixRadioToggled);
    connect(ui->radioButtonManual, &QRadioButton::toggled,
            this, &VMAdvancedEditPage::onManualRadioToggled);
    connect(ui->spinBoxShadowMultiplier, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
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
    if (ui->radioButtonGeneral->isChecked())
    {
        return tr("General use");
    } else if (ui->radioButtonCitrix->isChecked())
    {
        return tr("Citrix Provisioning Services");
    } else
    {
        return tr("Shadow memory multiplier: %1").arg(ui->spinBoxShadowMultiplier->value(), 0, 'f', 1);
    }
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

    // Get shadow multiplier from HVM_shadow_multiplier
    m_originalShadowMultiplier = objectDataBefore.value("HVM_shadow_multiplier").toDouble();
    if (m_originalShadowMultiplier < 1.0)
    {
        m_originalShadowMultiplier = 1.0; // Default to 1.0 if invalid
    }

    // Check if VM is suspended or paused
    bool isSuspendedOrPaused = (m_powerState == "Suspended" || m_powerState == "Paused");

    if (isSuspendedOrPaused)
    {
        ui->radioButtonGeneral->setEnabled(false);
        ui->radioButtonCitrix->setEnabled(false);
        ui->radioButtonManual->setEnabled(false);
        ui->spinBoxShadowMultiplier->setEnabled(false);
        ui->widgetWarning->setVisible(true);
    } else
    {
        ui->widgetWarning->setVisible(false);
    }

    // Set radio button based on shadow multiplier value
    if (qAbs(m_originalShadowMultiplier - SHADOW_MULTIPLIER_GENERAL) < 0.01)
    {
        ui->radioButtonGeneral->setChecked(true);
    } else if (qAbs(m_originalShadowMultiplier - SHADOW_MULTIPLIER_CPS) < 0.01)
    {
        ui->radioButtonCitrix->setChecked(true);
    } else
    {
        ui->radioButtonManual->setChecked(true);
    }

    ui->spinBoxShadowMultiplier->setValue(m_originalShadowMultiplier);
}

AsyncOperation* VMAdvancedEditPage::saveSettings()
{
    if (!hasChanged())
    {
        return nullptr;
    }

    double newMultiplier = getCurrentShadowMultiplier();

    // If VM is running, use set_shadow_multiplier_live
    // Otherwise, modify objectDataCopy (handled by parent dialog)
    if (m_powerState == "Running")
    {
        class SetShadowMultiplierOperation : public AsyncOperation
        {
        public:
            SetShadowMultiplierOperation(XenConnection* conn,
                                         const QString& vmRef,
                                         double multiplier,
                                         QObject* parent)
                : AsyncOperation(conn, tr("Change Shadow Multiplier"),
                                 tr("Changing shadow memory multiplier..."), parent),
                  m_vmRef(vmRef), m_multiplier(multiplier)
            {}

        protected:
            void run() override
            {
                XenRpcAPI api(connection()->getSession());

                setPercentComplete(30);

                QVariantList params;
                params << connection()->getSessionId() << m_vmRef << m_multiplier;
                QByteArray request = api.buildJsonRpcCall("VM.set_shadow_multiplier_live", params);
                connection()->sendRequest(request);

                setPercentComplete(100);
            }

        private:
            QString m_vmRef;
            double m_multiplier;
        };

        return new SetShadowMultiplierOperation(m_connection, m_vmRef, newMultiplier, this);
    } else
    {
        // For halted/stopped VMs, update objectDataCopy
        m_objectDataCopy["HVM_shadow_multiplier"] = newMultiplier;

        // Return inline AsyncOperation for set_HVM_shadow_multiplier
        class SetHVMShadowMultiplierOperation : public AsyncOperation
        {
        public:
            SetHVMShadowMultiplierOperation(XenConnection* conn,
                                            const QString& vmRef,
                                            double multiplier,
                                            QObject* parent)
                : AsyncOperation(conn, tr("Change Shadow Multiplier"),
                                 tr("Changing shadow memory multiplier..."), parent),
                  m_vmRef(vmRef), m_multiplier(multiplier)
            {}

        protected:
            void run() override
            {
                XenRpcAPI api(connection()->getSession());

                setPercentComplete(30);

                QVariantList params;
                params << connection()->getSessionId() << m_vmRef << m_multiplier;
                QByteArray request = api.buildJsonRpcCall("VM.set_HVM_shadow_multiplier", params);
                connection()->sendRequest(request);

                setPercentComplete(100);
            }

        private:
            QString m_vmRef;
            double m_multiplier;
        };

        return new SetHVMShadowMultiplierOperation(m_connection, m_vmRef, newMultiplier, this);
    }
}

bool VMAdvancedEditPage::isValidToSave() const
{
    return getCurrentShadowMultiplier() >= 1.0;
}

void VMAdvancedEditPage::showLocalValidationMessages()
{
    if (getCurrentShadowMultiplier() < 1.0)
    {
        QMessageBox::warning(const_cast<VMAdvancedEditPage*>(this),
                             tr("Invalid Shadow Multiplier"),
                             tr("Shadow memory multiplier must be at least 1.0"));
    }
}

void VMAdvancedEditPage::hideLocalValidationMessages()
{
    // No persistent validation messages to hide
}

void VMAdvancedEditPage::cleanup()
{
    // Nothing to clean up
}

bool VMAdvancedEditPage::hasChanged() const
{
    return qAbs(getCurrentShadowMultiplier() - m_originalShadowMultiplier) > 0.01;
}

void VMAdvancedEditPage::onGeneralRadioToggled(bool checked)
{
    if (checked)
    {
        ui->spinBoxShadowMultiplier->setValue(SHADOW_MULTIPLIER_GENERAL);
        ui->spinBoxShadowMultiplier->setEnabled(false);
    }
}

void VMAdvancedEditPage::onCitrixRadioToggled(bool checked)
{
    if (checked)
    {
        ui->spinBoxShadowMultiplier->setValue(SHADOW_MULTIPLIER_CPS);
        ui->spinBoxShadowMultiplier->setEnabled(false);
    }
}

void VMAdvancedEditPage::onManualRadioToggled(bool checked)
{
    ui->spinBoxShadowMultiplier->setEnabled(checked);
}

void VMAdvancedEditPage::onShadowMultiplierChanged(double value)
{
    Q_UNUSED(value);

    // If user manually changes value, switch to manual mode
    if (ui->spinBoxShadowMultiplier->hasFocus())
    {
        ui->radioButtonManual->setChecked(true);
    }
}

double VMAdvancedEditPage::getCurrentShadowMultiplier() const
{
    return ui->spinBoxShadowMultiplier->value();
}
