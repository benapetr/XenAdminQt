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

#include "editvmhaprioritiesdialog.h"
#include "../operations/operationmanager.h"
#include "../dialogs/operationprogressdialog.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/actions/pool/sethaprioritiesaction.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QDialogButtonBox>

EditVmHaPrioritiesDialog::EditVmHaPrioritiesDialog(QSharedPointer<Pool> pool, QWidget* parent)
    : QDialog(parent),
      m_pool(pool),
      m_ntol(0),
      m_maxNtol(0)
{
    // Get pool data
    this->m_poolName = this->m_pool ? this->m_pool->GetName() : QString("Pool");
    this->m_originalNtol = this->m_pool ? this->m_pool->HAHostFailuresToTolerate() : 0;
    this->m_ntol = this->m_originalNtol;

    this->setWindowTitle(tr("Edit VM HA Priorities - '%1'").arg(this->m_poolName));
    this->setMinimumSize(650, 500);
    this->resize(700, 550);

    this->setupUi();
    this->populateVMTable();
    this->updateNtolCalculation();
    this->updateOkButtonState();
}

void EditVmHaPrioritiesDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Warning area for dead hosts
    QHBoxLayout* warningLayout = new QHBoxLayout();
    this->m_warningIcon = new QLabel(this);
    this->m_warningIcon->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(24, 24));
    this->m_warningIcon->setVisible(false);
    warningLayout->addWidget(this->m_warningIcon);

    this->m_warningLabel = new QLabel(this);
    this->m_warningLabel->setWordWrap(true);
    this->m_warningLabel->setStyleSheet("color: #b8860b;");
    this->m_warningLabel->setVisible(false);
    warningLayout->addWidget(this->m_warningLabel, 1);
    mainLayout->addLayout(warningLayout);

    // NTOL configuration group
    this->m_ntolGroup = new QGroupBox(tr("Host Failure Tolerance"), this);
    QVBoxLayout* ntolLayout = new QVBoxLayout(this->m_ntolGroup);

    QHBoxLayout* ntolSpinLayout = new QHBoxLayout();
    QLabel* ntolLabel = new QLabel(tr("Number of host failures to tolerate:"), this->m_ntolGroup);
    this->m_ntolSpinBox = new QSpinBox(this->m_ntolGroup);
    this->m_ntolSpinBox->setRange(0, 10);
    this->m_ntolSpinBox->setValue(this->m_ntol);
    connect(this->m_ntolSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EditVmHaPrioritiesDialog::onNtolChanged);
    ntolSpinLayout->addWidget(ntolLabel);
    ntolSpinLayout->addWidget(this->m_ntolSpinBox);
    ntolSpinLayout->addStretch();
    ntolLayout->addLayout(ntolSpinLayout);

    this->m_maxNtolLabel = new QLabel(this->m_ntolGroup);
    this->m_maxNtolLabel->setStyleSheet("color: gray;");
    ntolLayout->addWidget(this->m_maxNtolLabel);

    this->m_ntolStatusLabel = new QLabel(this->m_ntolGroup);
    ntolLayout->addWidget(this->m_ntolStatusLabel);

    mainLayout->addWidget(this->m_ntolGroup);

    // VM priorities table
    QLabel* vmLabel = new QLabel(tr("VM Restart Priorities:"), this);
    mainLayout->addWidget(vmLabel);

    this->m_vmTable = new QTableWidget(this);
    this->m_vmTable->setColumnCount(4);
    this->m_vmTable->setHorizontalHeaderLabels({tr("VM"), tr("Restart Priority"), tr("Start Order"), tr("Start Delay (s)")});
    this->m_vmTable->horizontalHeader()->setStretchLastSection(true);
    this->m_vmTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    this->m_vmTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    this->m_vmTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    this->m_vmTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    this->m_vmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->m_vmTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->m_vmTable->verticalHeader()->setVisible(false);
    mainLayout->addWidget(this->m_vmTable, 1);

    // Button box
    QDialogButtonBox* buttonBox = new QDialogButtonBox(this);
    this->m_okButton = buttonBox->addButton(QDialogButtonBox::Ok);
    this->m_cancelButton = buttonBox->addButton(QDialogButtonBox::Cancel);
    this->m_okButton->setText(tr("OK"));
    this->m_cancelButton->setText(tr("Cancel"));

    connect(this->m_okButton, &QPushButton::clicked, this, &EditVmHaPrioritiesDialog::accept);
    connect(this->m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

void EditVmHaPrioritiesDialog::populateVMTable()
{
    this->m_vmTable->blockSignals(true);
    this->m_vmTable->setRowCount(0);
    this->m_originalSettings.clear();

    if (!this->m_pool)
        return;

    // Check for dead hosts
    bool hasDeadHosts = false;
    QList<QSharedPointer<Host>> hosts = this->m_pool->GetCache()->GetAll<Host>("host");
    for (const QSharedPointer<Host>& host : hosts)
    {
        if (host && !host->IsLive())
        {
            hasDeadHosts = true;
            break;
        }
    }

    if (hasDeadHosts)
    {
        this->m_warningIcon->setVisible(true);
        this->m_warningLabel->setVisible(true);
        this->m_warningLabel->setText(tr("Cannot edit HA priorities while hosts are offline. "
                                   "Ensure all hosts in the pool are online before modifying settings."));
        this->m_vmTable->setEnabled(false);
        this->m_ntolGroup->setEnabled(false);
        this->m_okButton->setEnabled(false);
        return;
    }

    // Get all VMs from cache
    QList<QSharedPointer<VM>> vms = this->m_pool->GetCache()->GetAll<VM>("vm");
    for (const QSharedPointer<VM>& vm : vms)
    {
        if (!vm || !vm->IsValid())
            continue;

        // Skip templates
        if (vm->IsTemplate())
            continue;

        // Skip control domains
        if (vm->IsControlDomain())
            continue;

        // Skip snapshots
        if (vm->IsSnapshot())
            continue;

        QString vmName = vm->GetName();
        QString currentPriority = vm->HARestartPriority();
        qint64 order = vm->Order();
        qint64 startDelay = vm->StartDelay();

        // Store original settings for change detection
        QVariantMap originalSetting;
        originalSetting["ha_restart_priority"] = currentPriority;
        originalSetting["order"] = order;
        originalSetting["start_delay"] = startDelay;
        this->m_originalSettings[vm->OpaqueRef()] = originalSetting;

        int row = this->m_vmTable->rowCount();
        this->m_vmTable->insertRow(row);

        // VM name
        QTableWidgetItem* nameItem = new QTableWidgetItem(vmName);
        nameItem->setData(Qt::UserRole, vm->OpaqueRef());
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        this->m_vmTable->setItem(row, 0, nameItem);

        // Priority combo box
        QComboBox* priorityCombo = new QComboBox();
        priorityCombo->addItem(tr("Restart"), "restart");
        priorityCombo->addItem(tr("Best Effort"), "best-effort");
        priorityCombo->addItem(tr("Do Not Restart"), "");

        // Set current priority
        if (currentPriority == "restart" || currentPriority == "always_restart" ||
            currentPriority == "always_restart_high_priority")
        {
            priorityCombo->setCurrentIndex(0); // Restart
        } else if (currentPriority == "best-effort" || currentPriority == "best_effort")
        {
            priorityCombo->setCurrentIndex(1); // Best Effort
        } else
        {
            priorityCombo->setCurrentIndex(2); // Do Not Restart
        }

        connect(priorityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
                    updateNtolCalculation();
                    updateOkButtonState();
                });
        this->m_vmTable->setCellWidget(row, 1, priorityCombo);

        // Start order spinbox
        QSpinBox* orderSpin = new QSpinBox();
        orderSpin->setRange(0, 9999);
        orderSpin->setValue(order);
        connect(orderSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &EditVmHaPrioritiesDialog::updateOkButtonState);
        this->m_vmTable->setCellWidget(row, 2, orderSpin);

        // Start delay spinbox
        QSpinBox* delaySpin = new QSpinBox();
        delaySpin->setRange(0, 600);
        delaySpin->setValue(startDelay);
        delaySpin->setSuffix(tr(" sec"));
        connect(delaySpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &EditVmHaPrioritiesDialog::updateOkButtonState);
        this->m_vmTable->setCellWidget(row, 3, delaySpin);
    }

    this->m_vmTable->blockSignals(false);
}

void EditVmHaPrioritiesDialog::onNtolChanged(int value)
{
    this->m_ntol = value;
    this->updateNtolCalculation();
    this->updateOkButtonState();
}

void EditVmHaPrioritiesDialog::updateNtolCalculation()
{
    this->m_ntol = this->m_ntolSpinBox->value();

    if (!this->m_pool)
        return;

    // Count hosts in pool
    QList<QSharedPointer<Host>> hosts = this->m_pool->GetCache()->GetAll<Host>("host");
    int hostCount = hosts.size();

    // Maximum NTOL is number of hosts - 1
    this->m_maxNtol = qMax(0, hostCount - 1);
    this->m_ntolSpinBox->setMaximum(this->m_maxNtol);

    this->m_maxNtolLabel->setText(tr("Maximum: %1 (pool has %2 hosts)").arg(this->m_maxNtol).arg(hostCount));

    // Count protected VMs
    int protectedVMs = 0;
    for (int row = 0; row < this->m_vmTable->rowCount(); ++row)
    {
        QComboBox* combo = qobject_cast<QComboBox*>(this->m_vmTable->cellWidget(row, 1));
        if (combo)
        {
            QString priority = combo->currentData().toString();
            if (priority == "restart")
            {
                protectedVMs++;
            }
        }
    }

    if (this->m_ntol > 0 && protectedVMs > 0)
    {
        this->m_ntolStatusLabel->setStyleSheet("color: green;");
        this->m_ntolStatusLabel->setText(tr("✓ Pool can tolerate %1 host failure(s) with %2 protected VM(s)")
                                       .arg(this->m_ntol)
                                       .arg(protectedVMs));
    } else if (this->m_ntol == 0)
    {
        this->m_ntolStatusLabel->setStyleSheet("color: #b8860b;");
        this->m_ntolStatusLabel->setText(tr("⚠ NTOL is 0 - HA will not automatically restart VMs on host failure"));
    } else
    {
        this->m_ntolStatusLabel->setStyleSheet("color: #b8860b;");
        this->m_ntolStatusLabel->setText(tr("⚠ No VMs set to 'Restart' priority"));
    }
}

void EditVmHaPrioritiesDialog::updateOkButtonState()
{
    // Enable OK if changes were made
    this->m_okButton->setEnabled(this->hasChanges());
}

bool EditVmHaPrioritiesDialog::hasChanges() const
{
    // Check NTOL change
    if (this->m_ntolSpinBox->value() != this->m_originalNtol)
        return true;

    // Check VM settings changes
    for (int row = 0; row < this->m_vmTable->rowCount(); ++row)
    {
        QTableWidgetItem* vmItem = this->m_vmTable->item(row, 0);
        if (!vmItem)
            continue;

        QString vmRef = vmItem->data(Qt::UserRole).toString();
        if (!this->m_originalSettings.contains(vmRef))
            continue;

        const QVariantMap& original = this->m_originalSettings[vmRef];

        QComboBox* priorityCombo = qobject_cast<QComboBox*>(this->m_vmTable->cellWidget(row, 1));
        QSpinBox* orderSpin = qobject_cast<QSpinBox*>(this->m_vmTable->cellWidget(row, 2));
        QSpinBox* delaySpin = qobject_cast<QSpinBox*>(this->m_vmTable->cellWidget(row, 3));

        if (priorityCombo)
        {
            QString currentPriority = priorityCombo->currentData().toString();
            QString origPriority = original.value("ha_restart_priority", "").toString();

            // Normalize priority names for comparison
            bool currentIsRestart = (currentPriority == "restart");
            bool origIsRestart = (origPriority == "restart" || origPriority == "always_restart" ||
                                  origPriority == "always_restart_high_priority");
            bool currentIsBestEffort = (currentPriority == "best-effort");
            bool origIsBestEffort = (origPriority == "best-effort" || origPriority == "best_effort");
            bool currentIsDoNotRestart = (currentPriority.isEmpty());
            bool origIsDoNotRestart = (origPriority.isEmpty() || origPriority == "do_not_restart");

            if (!((currentIsRestart && origIsRestart) ||
                  (currentIsBestEffort && origIsBestEffort) ||
                  (currentIsDoNotRestart && origIsDoNotRestart)))
            {
                return true;
            }
        }

        if (orderSpin && orderSpin->value() != original.value("order", 0).toLongLong())
            return true;

        if (delaySpin && delaySpin->value() != original.value("start_delay", 0).toLongLong())
            return true;
    }

    return false;
}

QMap<QString, QVariantMap> EditVmHaPrioritiesDialog::buildVmStartupOptions() const
{
    QMap<QString, QVariantMap> options;

    for (int row = 0; row < this->m_vmTable->rowCount(); ++row)
    {
        QTableWidgetItem* vmItem = this->m_vmTable->item(row, 0);
        if (!vmItem)
            continue;

        QString vmRef = vmItem->data(Qt::UserRole).toString();

        QComboBox* priorityCombo = qobject_cast<QComboBox*>(this->m_vmTable->cellWidget(row, 1));
        QSpinBox* orderSpin = qobject_cast<QSpinBox*>(this->m_vmTable->cellWidget(row, 2));
        QSpinBox* delaySpin = qobject_cast<QSpinBox*>(this->m_vmTable->cellWidget(row, 3));

        QVariantMap vmOptions;
        vmOptions["ha_restart_priority"] = priorityCombo ? priorityCombo->currentData().toString() : "";
        vmOptions["order"] = orderSpin ? orderSpin->value() : 0;
        vmOptions["start_delay"] = delaySpin ? delaySpin->value() : 0;

        options[vmRef] = vmOptions;
    }

    return options;
}

void EditVmHaPrioritiesDialog::accept()
{
    qint64 newNtol = this->m_ntolSpinBox->value();

    // Warn if NTOL is being set to 0
    if (newNtol == 0 && this->m_originalNtol != 0)
    {
        QMessageBox::StandardButton result = QMessageBox::warning(
            this, tr("NTOL is Zero"),
            tr("Setting host failures to tolerate to 0 means HA will not automatically restart VMs on host failure.\n\n"
               "Are you sure you want to continue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (result != QMessageBox::Yes)
            return;
    }

    // Resolve Pool object from cache
    if (!this->m_pool || !this->m_pool->IsValid())
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to resolve pool object"));
        return;
    }

    // Build startup options
    QMap<QString, QVariantMap> vmOptions = this->buildVmStartupOptions();

    // Create and run SetHaPrioritiesAction
    SetHaPrioritiesAction* action = new SetHaPrioritiesAction(this->m_pool, vmOptions, newNtol, this);

    // Register with OperationManager
    OperationManager::instance()->RegisterOperation(action);

    // Show progress dialog
    OperationProgressDialog* progressDialog = new OperationProgressDialog(action, this);

    connect(action, &AsyncOperation::completed, [this, progressDialog]()
    {
        progressDialog->close();
        QDialog::accept();
    });

    connect(action, &AsyncOperation::failed, [this, progressDialog](const QString& error)
    {
        progressDialog->close();
        QMessageBox::critical(this, tr("Failed to Update HA Priorities"),
                              tr("Failed to update HA priorities:\n\n%1").arg(error));
    });

    action->RunAsync();
    progressDialog->exec();
}
