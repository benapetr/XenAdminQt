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
#include "xenlib.h"
#include "xencache.h"
#include "xen/connection.h"
#include "xen/session.h"
#include "xen/actions/pool/sethaprioritiesaction.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QDialogButtonBox>

EditVmHaPrioritiesDialog::EditVmHaPrioritiesDialog(XenLib* xenLib, const QString& poolRef, QWidget* parent)
    : QDialog(parent),
      m_xenLib(xenLib),
      m_poolRef(poolRef),
      m_ntol(0),
      m_maxNtol(0)
{
    // Get pool data
    QVariantMap poolData = m_xenLib->getCache()->resolve("pool", m_poolRef);
    m_poolName = poolData.value("name_label", "Pool").toString();
    m_originalNtol = poolData.value("ha_host_failures_to_tolerate", 0).toLongLong();
    m_ntol = m_originalNtol;

    setWindowTitle(tr("Edit VM HA Priorities - '%1'").arg(m_poolName));
    setMinimumSize(650, 500);
    resize(700, 550);

    setupUi();
    populateVMTable();
    updateNtolCalculation();
    updateOkButtonState();
}

void EditVmHaPrioritiesDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Warning area for dead hosts
    QHBoxLayout* warningLayout = new QHBoxLayout();
    m_warningIcon = new QLabel(this);
    m_warningIcon->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(24, 24));
    m_warningIcon->setVisible(false);
    warningLayout->addWidget(m_warningIcon);

    m_warningLabel = new QLabel(this);
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setStyleSheet("color: #b8860b;");
    m_warningLabel->setVisible(false);
    warningLayout->addWidget(m_warningLabel, 1);
    mainLayout->addLayout(warningLayout);

    // NTOL configuration group
    m_ntolGroup = new QGroupBox(tr("Host Failure Tolerance"), this);
    QVBoxLayout* ntolLayout = new QVBoxLayout(m_ntolGroup);

    QHBoxLayout* ntolSpinLayout = new QHBoxLayout();
    QLabel* ntolLabel = new QLabel(tr("Number of host failures to tolerate:"), m_ntolGroup);
    m_ntolSpinBox = new QSpinBox(m_ntolGroup);
    m_ntolSpinBox->setRange(0, 10);
    m_ntolSpinBox->setValue(m_ntol);
    connect(m_ntolSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EditVmHaPrioritiesDialog::onNtolChanged);
    ntolSpinLayout->addWidget(ntolLabel);
    ntolSpinLayout->addWidget(m_ntolSpinBox);
    ntolSpinLayout->addStretch();
    ntolLayout->addLayout(ntolSpinLayout);

    m_maxNtolLabel = new QLabel(m_ntolGroup);
    m_maxNtolLabel->setStyleSheet("color: gray;");
    ntolLayout->addWidget(m_maxNtolLabel);

    m_ntolStatusLabel = new QLabel(m_ntolGroup);
    ntolLayout->addWidget(m_ntolStatusLabel);

    mainLayout->addWidget(m_ntolGroup);

    // VM priorities table
    QLabel* vmLabel = new QLabel(tr("VM Restart Priorities:"), this);
    mainLayout->addWidget(vmLabel);

    m_vmTable = new QTableWidget(this);
    m_vmTable->setColumnCount(4);
    m_vmTable->setHorizontalHeaderLabels({tr("VM"), tr("Restart Priority"), tr("Start Order"), tr("Start Delay (s)")});
    m_vmTable->horizontalHeader()->setStretchLastSection(true);
    m_vmTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_vmTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_vmTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_vmTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_vmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_vmTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_vmTable->verticalHeader()->setVisible(false);
    mainLayout->addWidget(m_vmTable, 1);

    // Button box
    QDialogButtonBox* buttonBox = new QDialogButtonBox(this);
    m_okButton = buttonBox->addButton(QDialogButtonBox::Ok);
    m_cancelButton = buttonBox->addButton(QDialogButtonBox::Cancel);
    m_okButton->setText(tr("OK"));
    m_cancelButton->setText(tr("Cancel"));

    connect(m_okButton, &QPushButton::clicked, this, &EditVmHaPrioritiesDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

void EditVmHaPrioritiesDialog::populateVMTable()
{
    m_vmTable->blockSignals(true);
    m_vmTable->setRowCount(0);
    m_originalSettings.clear();

    // Check for dead hosts
    QStringList hostRefs = m_xenLib->getCache()->getAllRefs("host");
    bool hasDeadHosts = false;
    for (const QString& hostRef : hostRefs)
    {
        QVariantMap hostData = m_xenLib->getCache()->resolve("host", hostRef);
        QVariantMap metrics = m_xenLib->getCache()->resolve("host_metrics",
                                                            hostData.value("metrics", "").toString());
        bool isLive = metrics.value("live", true).toBool();
        if (!isLive)
        {
            hasDeadHosts = true;
            break;
        }
    }

    if (hasDeadHosts)
    {
        m_warningIcon->setVisible(true);
        m_warningLabel->setVisible(true);
        m_warningLabel->setText(tr("Cannot edit HA priorities while hosts are offline. "
                                   "Ensure all hosts in the pool are online before modifying settings."));
        m_vmTable->setEnabled(false);
        m_ntolGroup->setEnabled(false);
        m_okButton->setEnabled(false);
        return;
    }

    // Get all VMs from cache
    QStringList vmRefs = m_xenLib->getCache()->getAllRefs("vm");

    for (const QString& vmRef : vmRefs)
    {
        QVariantMap vmData = m_xenLib->getCache()->resolve("vm", vmRef);

        // Skip templates
        bool isTemplate = vmData.value("is_a_template", false).toBool();
        if (isTemplate)
            continue;

        // Skip control domains
        bool isControlDomain = vmData.value("is_control_domain", false).toBool();
        if (isControlDomain)
            continue;

        // Skip snapshots
        bool isSnapshot = vmData.value("is_a_snapshot", false).toBool();
        if (isSnapshot)
            continue;

        QString vmName = vmData.value("name_label", "Unknown").toString();
        QString currentPriority = vmData.value("ha_restart_priority", "").toString();
        qint64 order = vmData.value("order", 0).toLongLong();
        qint64 startDelay = vmData.value("start_delay", 0).toLongLong();

        // Store original settings for change detection
        QVariantMap originalSetting;
        originalSetting["ha_restart_priority"] = currentPriority;
        originalSetting["order"] = order;
        originalSetting["start_delay"] = startDelay;
        m_originalSettings[vmRef] = originalSetting;

        int row = m_vmTable->rowCount();
        m_vmTable->insertRow(row);

        // VM name
        QTableWidgetItem* nameItem = new QTableWidgetItem(vmName);
        nameItem->setData(Qt::UserRole, vmRef);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_vmTable->setItem(row, 0, nameItem);

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
        m_vmTable->setCellWidget(row, 1, priorityCombo);

        // Start order spinbox
        QSpinBox* orderSpin = new QSpinBox();
        orderSpin->setRange(0, 9999);
        orderSpin->setValue(order);
        connect(orderSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &EditVmHaPrioritiesDialog::updateOkButtonState);
        m_vmTable->setCellWidget(row, 2, orderSpin);

        // Start delay spinbox
        QSpinBox* delaySpin = new QSpinBox();
        delaySpin->setRange(0, 600);
        delaySpin->setValue(startDelay);
        delaySpin->setSuffix(tr(" sec"));
        connect(delaySpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &EditVmHaPrioritiesDialog::updateOkButtonState);
        m_vmTable->setCellWidget(row, 3, delaySpin);
    }

    m_vmTable->blockSignals(false);
}

void EditVmHaPrioritiesDialog::onNtolChanged(int value)
{
    m_ntol = value;
    updateNtolCalculation();
    updateOkButtonState();
}

void EditVmHaPrioritiesDialog::updateNtolCalculation()
{
    m_ntol = m_ntolSpinBox->value();

    // Count hosts in pool
    QStringList hostRefs = m_xenLib->getCache()->getAllRefs("host");
    int hostCount = hostRefs.size();

    // Maximum NTOL is number of hosts - 1
    m_maxNtol = qMax(0, hostCount - 1);
    m_ntolSpinBox->setMaximum(m_maxNtol);

    m_maxNtolLabel->setText(tr("Maximum: %1 (pool has %2 hosts)").arg(m_maxNtol).arg(hostCount));

    // Count protected VMs
    int protectedVMs = 0;
    for (int row = 0; row < m_vmTable->rowCount(); ++row)
    {
        QComboBox* combo = qobject_cast<QComboBox*>(m_vmTable->cellWidget(row, 1));
        if (combo)
        {
            QString priority = combo->currentData().toString();
            if (priority == "restart")
            {
                protectedVMs++;
            }
        }
    }

    if (m_ntol > 0 && protectedVMs > 0)
    {
        m_ntolStatusLabel->setStyleSheet("color: green;");
        m_ntolStatusLabel->setText(tr("✓ Pool can tolerate %1 host failure(s) with %2 protected VM(s)")
                                       .arg(m_ntol)
                                       .arg(protectedVMs));
    } else if (m_ntol == 0)
    {
        m_ntolStatusLabel->setStyleSheet("color: #b8860b;");
        m_ntolStatusLabel->setText(tr("⚠ NTOL is 0 - HA will not automatically restart VMs on host failure"));
    } else
    {
        m_ntolStatusLabel->setStyleSheet("color: #b8860b;");
        m_ntolStatusLabel->setText(tr("⚠ No VMs set to 'Restart' priority"));
    }
}

void EditVmHaPrioritiesDialog::updateOkButtonState()
{
    // Enable OK if changes were made
    m_okButton->setEnabled(hasChanges());
}

bool EditVmHaPrioritiesDialog::hasChanges() const
{
    // Check NTOL change
    if (m_ntolSpinBox->value() != m_originalNtol)
        return true;

    // Check VM settings changes
    for (int row = 0; row < m_vmTable->rowCount(); ++row)
    {
        QTableWidgetItem* vmItem = m_vmTable->item(row, 0);
        if (!vmItem)
            continue;

        QString vmRef = vmItem->data(Qt::UserRole).toString();
        if (!m_originalSettings.contains(vmRef))
            continue;

        const QVariantMap& original = m_originalSettings[vmRef];

        QComboBox* priorityCombo = qobject_cast<QComboBox*>(m_vmTable->cellWidget(row, 1));
        QSpinBox* orderSpin = qobject_cast<QSpinBox*>(m_vmTable->cellWidget(row, 2));
        QSpinBox* delaySpin = qobject_cast<QSpinBox*>(m_vmTable->cellWidget(row, 3));

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

    for (int row = 0; row < m_vmTable->rowCount(); ++row)
    {
        QTableWidgetItem* vmItem = m_vmTable->item(row, 0);
        if (!vmItem)
            continue;

        QString vmRef = vmItem->data(Qt::UserRole).toString();

        QComboBox* priorityCombo = qobject_cast<QComboBox*>(m_vmTable->cellWidget(row, 1));
        QSpinBox* orderSpin = qobject_cast<QSpinBox*>(m_vmTable->cellWidget(row, 2));
        QSpinBox* delaySpin = qobject_cast<QSpinBox*>(m_vmTable->cellWidget(row, 3));

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
    qint64 newNtol = m_ntolSpinBox->value();

    // Warn if NTOL is being set to 0
    if (newNtol == 0 && m_originalNtol != 0)
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

    // Build startup options
    QMap<QString, QVariantMap> vmOptions = buildVmStartupOptions();

    // Create and run SetHaPrioritiesAction
    SetHaPrioritiesAction* action = new SetHaPrioritiesAction(
        m_xenLib->getConnection(),
        m_poolRef,
        vmOptions,
        newNtol,
        this);

    // Register with OperationManager
    OperationManager::instance()->registerOperation(action);

    // Show progress dialog
    OperationProgressDialog* progressDialog = new OperationProgressDialog(action, this);

    connect(action, &AsyncOperation::completed, [this, progressDialog]() {
        progressDialog->close();
        QDialog::accept();
    });

    connect(action, &AsyncOperation::failed, [this, progressDialog](const QString& error) {
        progressDialog->close();
        QMessageBox::critical(this, tr("Failed to Update HA Priorities"),
                              tr("Failed to update HA priorities:\n\n%1").arg(error));
    });

    action->runAsync();
    progressDialog->exec();
}
