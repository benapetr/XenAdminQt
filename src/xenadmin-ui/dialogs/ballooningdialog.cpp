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
#include "xen/host.h"
#include "xen/hostmetrics.h"
#include "xen/vm.h"
#include "xen/vmmetrics.h"
#include "xencache.h"
#include "operationprogressdialog.h"
#include "xen/actions/vm/changememorysettingsaction.h"
#include <QMessageBox>
#include <QDebug>
#include <QDomDocument>
#include <QSignalBlocker>

BallooningDialog::BallooningDialog(const QSharedPointer<VM> &vm, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::BallooningDialog),
      m_maxDynMin(-1.0),
      m_memorySpinnerMax(0)
{
    this->ui->setupUi(this);

    this->m_vm = vm;

    if (!this->m_vm || !this->m_vm->IsValid())
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load VM data"));
        return;
    }

    this->m_connection = vm->GetConnection();

    // Store original memory settings
    this->m_originalStaticMin = this->m_vm->GetMemoryStaticMin();
    this->m_originalStaticMax = this->m_vm->GetMemoryStaticMax();
    this->m_originalDynamicMin = this->m_vm->GetMemoryDynamicMin();
    this->m_originalDynamicMax = this->m_vm->GetMemoryDynamicMax();

    this->m_isTemplate = this->m_vm->IsTemplate();

    // Check if VM supports ballooning (DMC)
    // C# Reference: BallooningDialog.cs line 52 - vm.UsesBallooning()
    this->m_hasBallooning = vm->SupportsBallooning();
    this->m_memorySpinnerMax = this->getMemorySpinnerMax();

    // Connect signals
    connect(this->ui->radioFixed, &QRadioButton::toggled, this, &BallooningDialog::onFixedRadioToggled);
    connect(this->ui->radioDynamic, &QRadioButton::toggled, this, &BallooningDialog::onDynamicRadioToggled);
    connect(this->ui->spinnerFixed, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BallooningDialog::onFixedValueChanged);
    connect(this->ui->spinnerDynMin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BallooningDialog::onDynMinValueChanged);
    connect(this->ui->spinnerDynMax, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BallooningDialog::onDynMaxValueChanged);
    connect(this->ui->vmShinyBar, &VMShinyBar::sliderDragged, this, &BallooningDialog::onShinyBarSliderDragged);
    connect(this->ui->checkBoxAdvanced, &QCheckBox::toggled, this, &BallooningDialog::onAdvancedToggled);
    connect(this->ui->spinnerStaticMin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &BallooningDialog::onStaticMinValueChanged);
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &BallooningDialog::onAccepted);

    // Populate controls with current values
    this->populateControls();
    this->updateDMCAvailability();
}

BallooningDialog::~BallooningDialog()
{
    delete this->ui;
}

void BallooningDialog::populateControls()
{
    // C# Reference: VMMemoryControlsBasic.cs Populate() method

    // Set rubric text based on template status
    if (this->m_isTemplate)
    {
        this->ui->labelRubric->setText(tr("Specify memory allocation for this template. Dynamic Memory Control allows you to specify a minimum and maximum memory value."));
    }

    // Convert bytes to MB for spinners
    double staticMaxMB = this->bytesToMB(this->m_originalStaticMax);
    double dynamicMinMB = this->bytesToMB(this->m_originalDynamicMin);
    double dynamicMaxMB = this->bytesToMB(this->m_originalDynamicMax);

    // Set spinner values
    this->ui->spinnerFixed->setValue(staticMaxMB);
    this->ui->spinnerDynMin->setValue(dynamicMinMB);
    this->ui->spinnerDynMax->setValue(dynamicMaxMB);
    this->ui->spinnerStaticMin->setValue(this->bytesToMB(this->m_originalStaticMin));
    this->ui->advancedWidget->setVisible(false);
    this->ui->checkBoxAdvanced->setChecked(false);

    // Set initial radio button state
    // C# Reference: VMMemoryControlsBasic.cs line 63-68
    if (this->m_hasBallooning && this->m_originalDynamicMin != this->m_originalStaticMax)
    {
        this->ui->radioDynamic->setChecked(true);
    } else
    {
        this->ui->radioFixed->setChecked(true);
    }

    if (this->m_vm)
        this->ui->vmShinyBar->Populate({ this->m_vm }, true);

    QString powerState = this->m_vm ? this->m_vm->GetPowerState() : QString();
    //bool vmRunning = powerState != "Halted";
    this->ui->checkBoxDeferReboot->setVisible(false);

    // This feature doesn't work due to lack of support from xapi side
    /*
    this->checkboxDeferVisible = vmRunning;
    if (!vmRunning)
        this->ui->checkBoxDeferReboot->setChecked(false);
    */

    this->setIncrements();
    this->updateSpinnerRanges();
    this->updateShinyBar();
}

void BallooningDialog::updateDMCAvailability()
{
    // C# Reference: VMMemoryControlsBasic.cs Populate() line 59-149

    // Check if DMC (Dynamic Memory Control) is available
    bool dmcAvailable = this->m_hasBallooning;

    // Disable dynamic radio if DMC not supported
    if (!dmcAvailable)
    {
        this->ui->radioDynamic->setEnabled(false);
        this->ui->dynamicDetailsWidget->setEnabled(false);

        QString reason;
        if (this->m_isTemplate)
        {
            reason = tr("Dynamic Memory Control is not available for templates.");
        } else
        {
            // Check virtualization status from VM data
            // This is simplified - C# does more detailed checking
            QString powerState = this->m_vm->GetPowerState();
            if (powerState == "Halted")
            {
                reason = tr("Dynamic Memory Control requires the VM to have Xen VM Tools installed. "
                            "Start the VM and install the tools to enable this feature.");
            } else
            {
                reason = tr("Dynamic Memory Control is not available for this VM.");
            }
        }

        this->ui->labelDMCUnavailable->setText(reason);
        this->ui->labelDMCUnavailable->setVisible(true);
    } else
    {
        this->ui->dynamicDetailsWidget->setEnabled(true);
        this->ui->labelDMCUnavailable->setVisible(false);
    }
}

void BallooningDialog::updateSpinnerRanges()
{
    // C# Reference: VMMemoryControlsBasic.cs SetSpinnerRanges()

    // Set minimum allowed values (e.g., 128 MB)
    const double minMemoryMB = 128.0;

    QSignalBlocker blockFixed(this->ui->spinnerFixed);
    QSignalBlocker blockDynMin(this->ui->spinnerDynMin);
    QSignalBlocker blockDynMax(this->ui->spinnerDynMax);
    QSignalBlocker blockStaticMin(this->ui->spinnerStaticMin);

    // Get maximum allowed from VM or use reasonable default
    double maxMemoryMB = this->bytesToMB(this->m_memorySpinnerMax > 0 ? this->m_memorySpinnerMax : this->m_originalStaticMax);

    // Update ranges for all spinners
    this->ui->spinnerFixed->setMinimum(minMemoryMB);
    this->ui->spinnerFixed->setMaximum(maxMemoryMB);

    this->ui->spinnerDynMin->setMinimum(minMemoryMB);
    this->ui->spinnerDynMin->setMaximum(maxMemoryMB);

    this->ui->spinnerDynMax->setMinimum(minMemoryMB);
    this->ui->spinnerDynMax->setMaximum(maxMemoryMB);

    this->m_maxDynMin = this->calcMaxDynMin();

    double staticMaxMB = this->currentStaticMaxMB();
    double dynamicMinMB = this->currentDynamicMinMB();
    double dynamicMaxMB = this->currentDynamicMaxMB();
    double ratio = this->getMemoryRatio();

    double maxFixedMB = maxMemoryMB;
    if (this->m_maxDynMin >= 0 && this->bytesToMB(this->m_maxDynMin) <= maxMemoryMB)
        maxFixedMB = this->bytesToMB(this->m_maxDynMin);

    double staticMinMB = this->currentStaticMinMB();
    this->ui->spinnerFixed->setMinimum(staticMinMB > minMemoryMB ? staticMinMB : minMemoryMB);
    this->ui->spinnerFixed->setMaximum(maxFixedMB);

    double staticMinMaxMB = staticMaxMB;
    if (this->m_hasBallooning && this->ui->radioDynamic->isChecked())
    {
        if (dynamicMinMB < staticMinMaxMB)
            staticMinMaxMB = dynamicMinMB;
    }
    this->ui->spinnerStaticMin->setMinimum(minMemoryMB);
    this->ui->spinnerStaticMin->setMaximum(staticMinMaxMB);
    if (this->ui->spinnerStaticMin->value() > staticMinMaxMB)
        this->ui->spinnerStaticMin->setValue(staticMinMaxMB);

    if (!this->m_hasBallooning)
        return;

    double maxDynMinMB = dynamicMaxMB;
    if (this->m_maxDynMin >= 0 && this->bytesToMB(this->m_maxDynMin) < maxDynMinMB)
    {
        maxDynMinMB = this->bytesToMB(this->m_maxDynMin);
        if (maxDynMinMB < staticMinMB)
            maxDynMinMB = staticMinMB;
    }
    if (maxDynMinMB < dynamicMinMB)
        maxDynMinMB = dynamicMinMB;

    double minDynMinMB = staticMinMB;
    double ratioLimitMB = staticMaxMB * ratio;
    if (ratioLimitMB > minDynMinMB)
        minDynMinMB = ratioLimitMB;
    if (minDynMinMB > dynamicMinMB)
        minDynMinMB = dynamicMinMB;

    double maxStaticMaxMB = maxMemoryMB;
    if (this->m_maxDynMin >= 0 && maxMemoryMB * ratio > this->bytesToMB(this->m_maxDynMin))
        maxStaticMaxMB = this->bytesToMB(this->m_maxDynMin) / ratio;
    if (maxStaticMaxMB < staticMaxMB)
        maxStaticMaxMB = staticMaxMB;

    this->ui->spinnerDynMin->setMinimum(minDynMinMB);
    this->ui->spinnerDynMin->setMaximum(maxDynMinMB);

    double dynMaxMinMB = dynamicMinMB > minMemoryMB ? dynamicMinMB : minMemoryMB;
    this->ui->spinnerDynMax->setMinimum(dynMaxMinMB);
    this->ui->spinnerDynMax->setMaximum(maxStaticMaxMB);

    this->ui->vmShinyBar->SetRanges(this->mbToBytes(minDynMinMB), this->mbToBytes(maxDynMinMB),
                              this->mbToBytes(dynamicMinMB), this->mbToBytes(maxStaticMaxMB),
                              "MB");

    if (this->ui->spinnerDynMin->value() > this->ui->spinnerDynMax->value())
        this->ui->spinnerDynMin->setValue(this->ui->spinnerDynMax->value());
}

void BallooningDialog::updateShinyBar()
{
    if (!this->m_vm)
        return;

    double staticMaxMB = this->currentStaticMaxMB();
    double dynamicMinMB = this->currentDynamicMinMB();
    double dynamicMaxMB = this->currentDynamicMaxMB();
    double staticMinMB = this->currentStaticMinMB();

    this->ui->vmShinyBar->ChangeSettings(this->mbToBytes(staticMinMB),
                                   this->mbToBytes(dynamicMinMB),
                                   this->mbToBytes(dynamicMaxMB),
                                   this->mbToBytes(staticMaxMB));
}

void BallooningDialog::setIncrements()
{
    double staticMaxMB = this->currentStaticMaxMB();
    double incrementMB = this->calcIncrementMB(staticMaxMB);
    this->ui->spinnerFixed->setSingleStep(incrementMB);
    this->ui->spinnerDynMin->setSingleStep(incrementMB);
    this->ui->spinnerDynMax->setSingleStep(incrementMB);
    this->ui->spinnerStaticMin->setSingleStep(incrementMB);
    this->ui->vmShinyBar->SetIncrement(this->mbToBytes(incrementMB));
}

void BallooningDialog::setSpinnerEnabled(bool fixed, bool dynamic)
{
    this->ui->spinnerFixed->setEnabled(fixed);
    this->ui->spinnerDynMin->setEnabled(dynamic);
    this->ui->spinnerDynMax->setEnabled(dynamic);
    this->ui->labelDynMin->setEnabled(dynamic);
    this->ui->labelDynMax->setEnabled(dynamic);
    this->ui->dynamicDetailsWidget->setEnabled(dynamic);
}

void BallooningDialog::onFixedRadioToggled(bool checked)
{
    // C# Reference: VMMemoryControlsBasic.cs radioFixed.CheckedChanged
    if (checked)
    {
        this->setSpinnerEnabled(true, false);
        this->setIncrements();
        this->updateSpinnerRanges();
        this->updateShinyBar();
    }
}

void BallooningDialog::onDynamicRadioToggled(bool checked)
{
    // C# Reference: VMMemoryControlsBasic.cs radioDynamic.CheckedChanged
    if (checked)
    {
        this->setSpinnerEnabled(false, true);
        this->setIncrements();
        this->updateSpinnerRanges();
        this->updateShinyBar();
    }
}

void BallooningDialog::onFixedValueChanged(double /*value*/)
{
    // C# Reference: VMMemoryControlsBasic.cs FixedSpinner_ValueChanged
    this->ui->radioFixed->setChecked(true);
    this->setIncrements();
    this->updateSpinnerRanges();
    this->updateShinyBar();
}

void BallooningDialog::onDynMinValueChanged(double value)
{
    // C# Reference: VMMemoryControlsBasic.cs DynamicSpinners_ValueChanged
    this->ui->radioDynamic->setChecked(true);

    // Ensure min <= max
    if (value > this->ui->spinnerDynMax->value())
    {
        this->ui->spinnerDynMax->setValue(value);
    }

    this->setIncrements();
    this->updateSpinnerRanges();
    this->updateShinyBar();
}

void BallooningDialog::onDynMaxValueChanged(double value)
{
    // C# Reference: VMMemoryControlsBasic.cs DynamicSpinners_ValueChanged
    this->ui->radioDynamic->setChecked(true);

    // Ensure min <= max
    if (value < this->ui->spinnerDynMin->value())
    {
        this->ui->spinnerDynMin->setValue(value);
    }

    this->setIncrements();
    this->updateSpinnerRanges();
    this->updateShinyBar();
}

void BallooningDialog::onShinyBarSliderDragged()
{
    this->ui->radioDynamic->setChecked(true);

    double minMB = this->bytesToMB(static_cast<qint64>(this->ui->vmShinyBar->DynamicMin()));
    double maxMB = this->bytesToMB(static_cast<qint64>(this->ui->vmShinyBar->DynamicMax()));

    this->ui->spinnerDynMin->setValue(minMB);
    this->ui->spinnerDynMax->setValue(maxMB);

    this->setIncrements();
    this->updateSpinnerRanges();
}

void BallooningDialog::onAdvancedToggled(bool checked)
{
    this->ui->advancedWidget->setVisible(checked);
    this->updateSpinnerRanges();
    this->updateShinyBar();
}

void BallooningDialog::onStaticMinValueChanged(double /*value*/)
{
    if (this->ui->checkBoxAdvanced->isChecked())
    {
        this->updateSpinnerRanges();
        this->updateShinyBar();
    }
}

qint64 BallooningDialog::bytesToMB(qint64 bytes) const
{
    return bytes / (1024 * 1024);
}

qint64 BallooningDialog::mbToBytes(double mb) const
{
    return static_cast<qint64>(mb * 1024 * 1024);
}

double BallooningDialog::currentDynamicMinMB() const
{
    if (this->ui->radioDynamic->isChecked())
        return this->ui->spinnerDynMin->value();

    return this->ui->spinnerFixed->value();
}

double BallooningDialog::currentDynamicMaxMB() const
{
    if (this->ui->radioDynamic->isChecked())
        return this->ui->spinnerDynMax->value();

    return this->ui->spinnerFixed->value();
}

double BallooningDialog::currentStaticMaxMB() const
{
    if (this->ui->radioDynamic->isChecked())
        return this->ui->spinnerDynMax->value();

    return this->ui->spinnerFixed->value();
}

double BallooningDialog::currentStaticMinMB() const
{
    if (this->ui->checkBoxAdvanced->isChecked())
        return this->ui->spinnerStaticMin->value();

    return this->bytesToMB(this->m_originalStaticMin);
}

double BallooningDialog::calcIncrementMB(double staticMaxMB) const
{
    int increment = 1;
    while (increment < static_cast<int>(staticMaxMB / 8.0) && increment < 128)
        increment *= 2;

    return increment;
}

double BallooningDialog::getMemoryRatio() const
{
    const double defaultRatio = 0.25;
    if (!this->m_connection || !this->m_connection->GetCache())
        return defaultRatio;

    QList<QVariantMap> pools = this->m_connection->GetCache()->GetAllData("pool");
    if (pools.isEmpty())
        return defaultRatio;

    QVariantMap otherConfig = pools.first().value("other_config").toMap();
    QString key = "memory-ratio-pv";
    if (this->m_vm && this->m_vm->IsHVM())
        key = "memory-ratio-hvm";

    bool ok = false;
    double ratio = otherConfig.value(key).toString().toDouble(&ok);
    if (!ok || ratio <= 0.0 || ratio > 1.0)
        return defaultRatio;

    return ratio;
}

double BallooningDialog::calcMaxDynMin() const
{
    if (!this->m_connection || !this->m_connection->GetCache() || !this->m_vm)
        return -1.0;

    QString powerState = this->m_vm->GetPowerState();
    if (powerState != "Running" && powerState != "Paused")
        return -1.0;

    QSharedPointer<Host> host = this->m_vm->GetResidentOnHost();
    if (!host || !host->IsValid())
        return -1.0;

    QSharedPointer<HostMetrics> hostMetrics = host->GetMetrics();
    qint64 totalMemory = hostMetrics ? hostMetrics->GetMemoryTotal() : 0;
    if (totalMemory == 0)
        return -1.0;

    qint64 sumDynMin = host->MemoryOverhead();
    QList<QSharedPointer<VM>> residentVMs = host->GetResidentVMs();
    for (const QSharedPointer<VM>& vm : residentVMs)
    {
        if (!vm || !vm->IsValid())
            continue;

        sumDynMin += vm->MemoryOverhead();

        if (vm->IsControlDomain())
        {
            QSharedPointer<VMMetrics> vmMetrics = vm->GetMetrics();
            if (vmMetrics)
                sumDynMin += vmMetrics->GetMemoryActual();
        } else if (vm->OpaqueRef() != this->m_vm->OpaqueRef())
        {
            sumDynMin += vm->GetMemoryDynamicMin();
        }
    }

    qint64 maxDynMin = totalMemory - sumDynMin;
    if (maxDynMin < 0)
        maxDynMin = 0;

    return static_cast<double>(maxDynMin);
}

bool BallooningDialog::vmUsesBallooning() const
{
    return this->m_originalDynamicMax != this->m_originalStaticMax && this->m_vm->SupportsBallooning();
}

qint64 BallooningDialog::getMemorySpinnerMax() const
{
    qint64 maxAllowed = 0;
    QString recommendations = this->m_vm ? this->m_vm->Recommendations() : QString();
    if (!recommendations.isEmpty())
    {
        QDomDocument doc;
        if (doc.setContent(recommendations))
        {
            QDomNodeList restrictions = doc.elementsByTagName("restriction");
            for (int i = 0; i < restrictions.count(); ++i)
            {
                QDomElement element = restrictions.at(i).toElement();
                if (element.isNull())
                    continue;

                if (element.attribute("field") != "memory-static-max")
                    continue;

                QString valueText = element.attribute("max");
                bool ok = false;
                qint64 value = valueText.toLongLong(&ok);
                if (ok)
                {
                    maxAllowed = value;
                    break;
                }
            }
        }
    }

    qint64 maxValue = this->m_originalStaticMax;
    if (maxAllowed > maxValue)
        maxValue = maxAllowed;

    return maxValue;
}

bool BallooningDialog::validateMemorySettings() const
{
    // C# Reference: VMMemoryControlsEdit.cs ChangeMemorySettings()

    if (this->ui->radioFixed->isChecked())
    {
        // Fixed mode: all values are the same
        qint64 memoryBytes = this->mbToBytes(this->ui->spinnerFixed->value());
        if (memoryBytes < this->mbToBytes(128))
        {
            return false;
        }
        if (this->ui->checkBoxAdvanced->isChecked())
        {
            qint64 staticMin = this->mbToBytes(this->ui->spinnerStaticMin->value());
            if (staticMin > memoryBytes)
                return false;
        }
    } else
    {
        // Dynamic mode: validate range
        qint64 dynMin = this->mbToBytes(this->ui->spinnerDynMin->value());
        qint64 dynMax = this->mbToBytes(this->ui->spinnerDynMax->value());

        if (dynMin > dynMax)
        {
            return false;
        }

        if (dynMin < this->mbToBytes(128) || dynMax < this->mbToBytes(128))
        {
            return false;
        }
        if (this->ui->checkBoxAdvanced->isChecked())
        {
            qint64 staticMin = this->mbToBytes(this->ui->spinnerStaticMin->value());
            if (staticMin > dynMin || staticMin > dynMax)
                return false;
        }
    }

    return true;
}

bool BallooningDialog::applyMemoryChanges()
{
    // C# Reference: VMMemoryControlsEdit.cs ChangeMemorySettings()
    // C# Reference: BallooningDialog.cs buttonOK_Click line 78

    if (!this->validateMemorySettings())
    {
        QMessageBox::warning(this, tr("Invalid Memory Settings"),
                             tr("Please ensure memory values are valid."));
        return false;
    }

    qint64 staticMin, staticMax, dynamicMin, dynamicMax;

    if (this->ui->radioFixed->isChecked())
    {
        // Fixed allocation: all limits set to same value
        qint64 fixedBytes = this->mbToBytes(this->ui->spinnerFixed->value());
        staticMin = this->ui->checkBoxAdvanced->isChecked()
            ? this->mbToBytes(this->ui->spinnerStaticMin->value())
            : this->m_originalStaticMin;
        staticMax = fixedBytes;
        dynamicMin = fixedBytes;
        dynamicMax = fixedBytes;
    } else
    {
        // Dynamic allocation
        dynamicMin = this->mbToBytes(this->ui->spinnerDynMin->value());
        dynamicMax = this->mbToBytes(this->ui->spinnerDynMax->value());

        // Static max = dynamic max
        staticMax = dynamicMax;

        // Static min stays as original for dynamic allocation
        staticMin = this->ui->checkBoxAdvanced->isChecked()
            ? this->mbToBytes(this->ui->spinnerStaticMin->value())
            : this->m_originalStaticMin;
    }

    if (this->bytesToMB(this->m_originalStaticMax) == this->bytesToMB(staticMax))
    {
        // Avoid false changes from rounding
        if (dynamicMin == staticMax)
            dynamicMin = this->m_originalStaticMax;
        if (dynamicMax == staticMax)
            dynamicMax = this->m_originalStaticMax;
        staticMax = this->m_originalStaticMax;
    }

    // Check if anything actually changed
    if (staticMin == this->m_originalStaticMin &&
        staticMax == this->m_originalStaticMax &&
        dynamicMin == this->m_originalDynamicMin &&
        dynamicMax == this->m_originalDynamicMax)
    {
        // No changes
        return true;
    }

    bool staticChanged = staticMin != this->m_originalStaticMin || staticMax != this->m_originalStaticMax;
    bool deferReboot = false; // this->checkboxDeferVisible && this->ui->checkBoxDeferReboot->isChecked();

    if (staticChanged && !deferReboot)
    {
        QString powerState = this->m_vm ? this->m_vm->GetPowerState() : QString();
        if (powerState != "Halted")
        {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                tr("Confirm Memory Change"),
                tr("Changing the maximum memory for this VM requires it to be shut down and restarted. Continue?"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

            if (reply != QMessageBox::Yes)
                return false;
        }
    }

    // Create and execute the memory settings action
    if (!this->m_connection)
    {
        QMessageBox::critical(this, tr("Error"), tr("No connection available"));
        return false;
    }

    if (!this->m_vm || !this->m_vm->IsValid())
    {
        QMessageBox::critical(this, tr("Error"), tr("VM not found in cache"));
        return false;
    }

    ChangeMemorySettingsAction* action = new ChangeMemorySettingsAction(
        this->m_vm,
        staticMin,
        dynamicMin,
        dynamicMax,
        staticMax,
        deferReboot,
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

    if (this->applyMemoryChanges())
    {
        accept();
    } else
    {
        // Don't close dialog if action failed
        // User can try again or cancel
    }
}
