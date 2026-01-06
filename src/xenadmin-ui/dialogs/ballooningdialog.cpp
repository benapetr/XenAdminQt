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

#include "ballooningdialog.h"
#include "ui_ballooningdialog.h"
#include "xen/network/connection.h"
#include "xencache.h"
#include "operationprogressdialog.h"
#include "xen/actions/vm/changememorysettingsaction.h"
#include <QMessageBox>
#include <QDebug>

BallooningDialog::BallooningDialog(const QString& vmRef, XenConnection* connection, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::BallooningDialog),
      m_vmRef(vmRef),
      m_connection(connection),
      m_hasBallooning(false),
      m_isTemplate(false)
{
    ui->setupUi(this);

    // Get VM data from cache
    if (m_connection && m_connection->GetCache())
    {
        m_vmData = m_connection->GetCache()->ResolveObjectData("vm", m_vmRef);
    }

    if (m_vmData.isEmpty())
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load VM data"));
        return;
    }

    // Store original memory settings
    m_originalStaticMin = m_vmData.value("memory_static_min").toLongLong();
    m_originalStaticMax = m_vmData.value("memory_static_max").toLongLong();
    m_originalDynamicMin = m_vmData.value("memory_dynamic_min").toLongLong();
    m_originalDynamicMax = m_vmData.value("memory_dynamic_max").toLongLong();

    m_isTemplate = m_vmData.value("is_a_template", false).toBool();

    // Check if VM supports ballooning (DMC)
    // C# Reference: BallooningDialog.cs line 52 - vm.UsesBallooning()
    m_hasBallooning = (m_originalDynamicMin != m_originalStaticMin ||
                       m_originalDynamicMax != m_originalStaticMax);

    // Connect signals
    connect(ui->radioFixed, &QRadioButton::toggled,
            this, &BallooningDialog::onFixedRadioToggled);
    connect(ui->radioDynamic, &QRadioButton::toggled,
            this, &BallooningDialog::onDynamicRadioToggled);

    connect(ui->spinnerFixed, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BallooningDialog::onFixedValueChanged);
    connect(ui->spinnerDynMin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BallooningDialog::onDynMinValueChanged);
    connect(ui->spinnerDynMax, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BallooningDialog::onDynMaxValueChanged);

    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &BallooningDialog::onAccepted);

    // Populate controls with current values
    populateControls();
    updateDMCAvailability();
}

BallooningDialog::~BallooningDialog()
{
    delete ui;
}

void BallooningDialog::populateControls()
{
    // C# Reference: VMMemoryControlsBasic.cs Populate() method

    // Set rubric text based on template status
    if (m_isTemplate)
    {
        ui->labelRubric->setText(tr("Specify memory allocation for this template. "
                                    "Dynamic Memory Control allows you to specify a minimum and maximum memory value."));
    }

    // Convert bytes to MB for spinners
    double staticMaxMB = bytesToMB(m_originalStaticMax);
    double dynamicMinMB = bytesToMB(m_originalDynamicMin);
    double dynamicMaxMB = bytesToMB(m_originalDynamicMax);

    // Set spinner values
    ui->spinnerFixed->setValue(staticMaxMB);
    ui->spinnerDynMin->setValue(dynamicMinMB);
    ui->spinnerDynMax->setValue(dynamicMaxMB);

    // Set initial radio button state
    // C# Reference: VMMemoryControlsBasic.cs line 63-68
    if (m_hasBallooning && m_originalDynamicMin != m_originalStaticMax)
    {
        ui->radioDynamic->setChecked(true);
    } else
    {
        ui->radioFixed->setChecked(true);
    }

    updateSpinnerRanges();
}

void BallooningDialog::updateDMCAvailability()
{
    // C# Reference: VMMemoryControlsBasic.cs Populate() line 59-149

    // Check if DMC (Dynamic Memory Control) is available
    bool dmcAvailable = m_hasBallooning;

    // Disable dynamic radio if DMC not supported
    if (!dmcAvailable)
    {
        ui->radioDynamic->setEnabled(false);
        ui->dynamicWidget->setEnabled(false);

        QString reason;
        if (m_isTemplate)
        {
            reason = tr("Dynamic Memory Control is not available for templates.");
        } else
        {
            // Check virtualization status from VM data
            // This is simplified - C# does more detailed checking
            QString powerState = m_vmData.value("power_state").toString();
            if (powerState == "Halted")
            {
                reason = tr("Dynamic Memory Control requires the VM to have Xen VM Tools installed. "
                            "Start the VM and install the tools to enable this feature.");
            } else
            {
                reason = tr("Dynamic Memory Control is not available for this VM.");
            }
        }

        ui->labelDMCUnavailable->setText(reason);
        ui->labelDMCUnavailable->setVisible(true);
    } else
    {
        ui->labelDMCUnavailable->setVisible(false);
    }
}

void BallooningDialog::updateSpinnerRanges()
{
    // C# Reference: VMMemoryControlsBasic.cs SetSpinnerRanges()

    // Set minimum allowed values (e.g., 128 MB)
    const double minMemoryMB = 128.0;

    // Get maximum allowed from VM or use reasonable default
    double maxMemoryMB = 999999.0; // Very large default

    // Update ranges for all spinners
    ui->spinnerFixed->setMinimum(minMemoryMB);
    ui->spinnerFixed->setMaximum(maxMemoryMB);

    ui->spinnerDynMin->setMinimum(minMemoryMB);
    ui->spinnerDynMin->setMaximum(maxMemoryMB);

    ui->spinnerDynMax->setMinimum(minMemoryMB);
    ui->spinnerDynMax->setMaximum(maxMemoryMB);

    // Ensure dynamic min <= dynamic max
    if (ui->spinnerDynMin->value() > ui->spinnerDynMax->value())
    {
        ui->spinnerDynMin->setValue(ui->spinnerDynMax->value());
    }
}

void BallooningDialog::setSpinnerEnabled(bool fixed, bool dynamic)
{
    ui->spinnerFixed->setEnabled(fixed);
    ui->spinnerDynMin->setEnabled(dynamic);
    ui->spinnerDynMax->setEnabled(dynamic);
    ui->labelDynMin->setEnabled(dynamic);
    ui->labelDynMax->setEnabled(dynamic);
}

void BallooningDialog::onFixedRadioToggled(bool checked)
{
    // C# Reference: VMMemoryControlsBasic.cs radioFixed.CheckedChanged
    if (checked)
    {
        setSpinnerEnabled(true, false);
    }
}

void BallooningDialog::onDynamicRadioToggled(bool checked)
{
    // C# Reference: VMMemoryControlsBasic.cs radioDynamic.CheckedChanged
    if (checked)
    {
        setSpinnerEnabled(false, true);
    }
}

void BallooningDialog::onFixedValueChanged(double /*value*/)
{
    // C# Reference: VMMemoryControlsBasic.cs FixedSpinner_ValueChanged
    ui->radioFixed->setChecked(true);
    updateSpinnerRanges();
}

void BallooningDialog::onDynMinValueChanged(double value)
{
    // C# Reference: VMMemoryControlsBasic.cs DynamicSpinners_ValueChanged
    ui->radioDynamic->setChecked(true);

    // Ensure min <= max
    if (value > ui->spinnerDynMax->value())
    {
        ui->spinnerDynMax->setValue(value);
    }

    updateSpinnerRanges();
}

void BallooningDialog::onDynMaxValueChanged(double value)
{
    // C# Reference: VMMemoryControlsBasic.cs DynamicSpinners_ValueChanged
    ui->radioDynamic->setChecked(true);

    // Ensure min <= max
    if (value < ui->spinnerDynMin->value())
    {
        ui->spinnerDynMin->setValue(value);
    }

    updateSpinnerRanges();
}

qint64 BallooningDialog::bytesToMB(qint64 bytes) const
{
    return bytes / (1024 * 1024);
}

qint64 BallooningDialog::mbToBytes(double mb) const
{
    return static_cast<qint64>(mb * 1024 * 1024);
}

bool BallooningDialog::validateMemorySettings() const
{
    // C# Reference: VMMemoryControlsEdit.cs ChangeMemorySettings()

    if (ui->radioFixed->isChecked())
    {
        // Fixed mode: all values are the same
        qint64 memoryBytes = mbToBytes(ui->spinnerFixed->value());
        if (memoryBytes < mbToBytes(128))
        {
            return false;
        }
    } else
    {
        // Dynamic mode: validate range
        qint64 dynMin = mbToBytes(ui->spinnerDynMin->value());
        qint64 dynMax = mbToBytes(ui->spinnerDynMax->value());

        if (dynMin > dynMax)
        {
            return false;
        }

        if (dynMin < mbToBytes(128) || dynMax < mbToBytes(128))
        {
            return false;
        }
    }

    return true;
}

bool BallooningDialog::applyMemoryChanges()
{
    // C# Reference: VMMemoryControlsEdit.cs ChangeMemorySettings()
    // C# Reference: BallooningDialog.cs buttonOK_Click line 78

    if (!validateMemorySettings())
    {
        QMessageBox::warning(this, tr("Invalid Memory Settings"),
                             tr("Please ensure memory values are valid."));
        return false;
    }

    qint64 staticMin, staticMax, dynamicMin, dynamicMax;

    if (ui->radioFixed->isChecked())
    {
        // Fixed allocation: all limits set to same value
        qint64 fixedBytes = mbToBytes(ui->spinnerFixed->value());
        staticMin = fixedBytes;
        staticMax = fixedBytes;
        dynamicMin = fixedBytes;
        dynamicMax = fixedBytes;
    } else
    {
        // Dynamic allocation
        dynamicMin = mbToBytes(ui->spinnerDynMin->value());
        dynamicMax = mbToBytes(ui->spinnerDynMax->value());

        // Static max = dynamic max
        staticMax = dynamicMax;

        // Static min stays as original or use dynamic min
        staticMin = m_originalStaticMin;
        if (dynamicMin < staticMin)
        {
            staticMin = dynamicMin;
        }
    }

    // Check if anything actually changed
    if (staticMin == m_originalStaticMin &&
        staticMax == m_originalStaticMax &&
        dynamicMin == m_originalDynamicMin &&
        dynamicMax == m_originalDynamicMax)
    {
        // No changes
        return true;
    }

    // Create and execute the memory settings action
    if (!m_connection)
    {
        QMessageBox::critical(this, tr("Error"), tr("No connection available"));
        return false;
    }

    ChangeMemorySettingsAction* action = new ChangeMemorySettingsAction(
        m_connection,
        m_vmRef,
        staticMin,
        dynamicMin,
        dynamicMax,
        staticMax,
        this);

    // Show progress dialog with the action
    OperationProgressDialog progressDialog(action, this);
    progressDialog.setWindowTitle(tr("Changing Memory Settings"));

    // Start the operation asynchronously
    action->RunAsync();

    // Show modal progress dialog (blocks until operation completes)
    int result = progressDialog.exec();

    return (result == QDialog::Accepted);
}

void BallooningDialog::onAccepted()
{
    // C# Reference: BallooningDialog.cs buttonOK_Click

    if (applyMemoryChanges())
    {
        accept();
    } else
    {
        // Don't close dialog if action failed
        // User can try again or cancel
    }
}
