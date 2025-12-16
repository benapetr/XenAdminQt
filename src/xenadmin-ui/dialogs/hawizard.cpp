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

#include "hawizard.h"
#include "../operations/operationmanager.h"
#include "../dialogs/operationprogressdialog.h"
#include "xenlib.h"
#include "xencache.h"
#include "xen/connection.h"
#include "xen/session.h"
#include "xen/actions/pool/enablehaaction.h"
#include "xen/xenapi/xenapi_Pool.h"
#include "xen/xenapi/xenapi_SR.h"
#include "xen/xenapi/xenapi_VM.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QIcon>
#include <QTimer>
#include <QApplication>

HAWizard::HAWizard(XenLib* xenLib, const QString& poolRef, QWidget* parent)
    : QWizard(parent),
      m_xenLib(xenLib),
      m_poolRef(poolRef),
      m_ntol(1),
      m_maxNtol(0)
{
    setWindowTitle(tr("Configure High Availability"));
    setWizardStyle(QWizard::ModernStyle);
    setMinimumSize(700, 500);

    // Get pool name for display
    QVariantMap poolData = m_xenLib->getCache()->resolve("pool", m_poolRef);
    m_poolName = poolData.value("name_label", "Pool").toString();

    // Create wizard pages
    addPage(createIntroPage());
    addPage(createChooseSRPage());
    addPage(createAssignPrioritiesPage());
    addPage(createFinishPage());

    setButtonText(QWizard::FinishButton, tr("Enable HA"));
}

QWizardPage* HAWizard::createIntroPage()
{
    QWizardPage* page = new QWizardPage(this);
    page->setTitle(tr("High Availability"));
    page->setSubTitle(tr("Introduction to High Availability"));

    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* introLabel = new QLabel(page);
    introLabel->setWordWrap(true);
    introLabel->setText(tr(
        "<p>High Availability (HA) provides automated VM restart capabilities in case of host failure.</p>"
        "<p>When HA is enabled:</p>"
        "<ul>"
        "<li>The pool monitors the health of all hosts</li>"
        "<li>If a host fails, protected VMs are automatically restarted on surviving hosts</li>"
        "<li>A heartbeat storage repository (SR) is used to detect host failures</li>"
        "</ul>"
        "<p><b>Requirements:</b></p>"
        "<ul>"
        "<li>At least one shared storage repository accessible from all hosts</li>"
        "<li>At least 2 hosts in the pool for meaningful protection</li>"
        "<li>Sufficient spare capacity on surviving hosts to restart failed VMs</li>"
        "</ul>"
        "<p>Click <b>Next</b> to select a heartbeat SR and configure VM restart priorities.</p>"));

    layout->addWidget(introLabel);
    layout->addStretch();

    return page;
}

QWizardPage* HAWizard::createChooseSRPage()
{
    QWizardPage* page = new QWizardPage(this);
    page->setTitle(tr("Choose Heartbeat SR"));
    page->setSubTitle(tr("Select a storage repository for the HA heartbeat and statefile"));

    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* descLabel = new QLabel(page);
    descLabel->setWordWrap(true);
    descLabel->setText(tr(
        "The heartbeat SR stores the HA statefile and is used by hosts to communicate their health status. "
        "Choose a reliable shared storage repository that is accessible from all hosts in the pool."));
    layout->addWidget(descLabel);

    layout->addSpacing(10);

    // Progress bar for scanning
    m_scanProgress = new QProgressBar(page);
    m_scanProgress->setRange(0, 0); // Indeterminate
    m_scanProgress->setVisible(false);
    layout->addWidget(m_scanProgress);

    // No SRs available label
    m_noSRsLabel = new QLabel(tr("No suitable shared storage repositories found."), page);
    m_noSRsLabel->setStyleSheet("color: red; font-weight: bold;");
    m_noSRsLabel->setVisible(false);
    layout->addWidget(m_noSRsLabel);

    // SR table
    m_srTable = new QTableWidget(page);
    m_srTable->setColumnCount(3);
    m_srTable->setHorizontalHeaderLabels({tr("Name"), tr("Description"), tr("Status")});
    m_srTable->horizontalHeader()->setStretchLastSection(true);
    m_srTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_srTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_srTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_srTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_srTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_srTable->verticalHeader()->setVisible(false);
    layout->addWidget(m_srTable, 1);

    connect(m_srTable, &QTableWidget::itemSelectionChanged,
            this, &HAWizard::onHeartbeatSRSelectionChanged);

    // Rescan button
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_rescanButton = new QPushButton(tr("Rescan"), page);
    connect(m_rescanButton, &QPushButton::clicked, this, &HAWizard::scanForHeartbeatSRs);
    buttonLayout->addWidget(m_rescanButton);
    layout->addLayout(buttonLayout);

    return page;
}

QWizardPage* HAWizard::createAssignPrioritiesPage()
{
    QWizardPage* page = new QWizardPage(this);
    page->setTitle(tr("Assign VM Restart Priorities"));
    page->setSubTitle(tr("Configure which VMs should be restarted in case of host failure"));

    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* descLabel = new QLabel(page);
    descLabel->setWordWrap(true);
    descLabel->setText(tr(
        "Assign restart priorities to VMs. Higher priority VMs are guaranteed to be restarted, "
        "while lower priority VMs are restarted on a best-effort basis."));
    layout->addWidget(descLabel);

    layout->addSpacing(10);

    // NTOL configuration group
    QGroupBox* ntolGroup = new QGroupBox(tr("Host Failure Tolerance"), page);
    QVBoxLayout* ntolLayout = new QVBoxLayout(ntolGroup);

    QHBoxLayout* ntolSpinLayout = new QHBoxLayout();
    QLabel* ntolLabel = new QLabel(tr("Number of host failures to tolerate:"), ntolGroup);
    m_ntolSpinBox = new QSpinBox(ntolGroup);
    m_ntolSpinBox->setRange(0, 10);
    m_ntolSpinBox->setValue(1);
    connect(m_ntolSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &HAWizard::onNtolChanged);
    ntolSpinLayout->addWidget(ntolLabel);
    ntolSpinLayout->addWidget(m_ntolSpinBox);
    ntolSpinLayout->addStretch();
    ntolLayout->addLayout(ntolSpinLayout);

    m_maxNtolLabel = new QLabel(ntolGroup);
    m_maxNtolLabel->setStyleSheet("color: gray;");
    ntolLayout->addWidget(m_maxNtolLabel);

    m_ntolStatusLabel = new QLabel(ntolGroup);
    ntolLayout->addWidget(m_ntolStatusLabel);

    layout->addWidget(ntolGroup);

    layout->addSpacing(10);

    // VM priorities table
    QLabel* vmLabel = new QLabel(tr("VM Restart Priorities:"), page);
    layout->addWidget(vmLabel);

    m_vmTable = new QTableWidget(page);
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
    layout->addWidget(m_vmTable, 1);

    connect(m_vmTable, &QTableWidget::cellChanged,
            this, &HAWizard::onPriorityChanged);

    return page;
}

QWizardPage* HAWizard::createFinishPage()
{
    QWizardPage* page = new QWizardPage(this);
    page->setTitle(tr("Confirm HA Configuration"));
    page->setSubTitle(tr("Review your settings before enabling High Availability"));

    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* descLabel = new QLabel(page);
    descLabel->setWordWrap(true);
    descLabel->setText(tr("Click <b>Enable HA</b> to enable High Availability with the following settings:"));
    layout->addWidget(descLabel);

    layout->addSpacing(15);

    // Summary group
    QGroupBox* summaryGroup = new QGroupBox(tr("Configuration Summary"), page);
    QFormLayout* formLayout = new QFormLayout(summaryGroup);
    formLayout->setLabelAlignment(Qt::AlignRight);

    m_finishSRLabel = new QLabel(summaryGroup);
    formLayout->addRow(tr("Heartbeat SR:"), m_finishSRLabel);

    m_finishNtolLabel = new QLabel(summaryGroup);
    formLayout->addRow(tr("Failures to tolerate:"), m_finishNtolLabel);

    m_finishRestartLabel = new QLabel(summaryGroup);
    formLayout->addRow(tr("Protected VMs (Restart):"), m_finishRestartLabel);

    m_finishBestEffortLabel = new QLabel(summaryGroup);
    formLayout->addRow(tr("Best Effort VMs:"), m_finishBestEffortLabel);

    m_finishDoNotRestartLabel = new QLabel(summaryGroup);
    formLayout->addRow(tr("Do Not Restart VMs:"), m_finishDoNotRestartLabel);

    layout->addWidget(summaryGroup);

    layout->addSpacing(15);

    // Warning section
    QHBoxLayout* warningLayout = new QHBoxLayout();
    m_finishWarningIcon = new QLabel(page);
    m_finishWarningIcon->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(24, 24));
    m_finishWarningIcon->setVisible(false);
    warningLayout->addWidget(m_finishWarningIcon);

    m_finishWarningLabel = new QLabel(page);
    m_finishWarningLabel->setWordWrap(true);
    m_finishWarningLabel->setStyleSheet("color: #b8860b;"); // Dark goldenrod
    m_finishWarningLabel->setVisible(false);
    warningLayout->addWidget(m_finishWarningLabel, 1);
    layout->addLayout(warningLayout);

    layout->addStretch();

    return page;
}

void HAWizard::initializePage(int id)
{
    switch (id)
    {
    case Page_ChooseSR:
        // Scan for heartbeat SRs when entering this page
        QTimer::singleShot(100, this, &HAWizard::scanForHeartbeatSRs);
        break;

    case Page_AssignPriorities:
        populateVMTable();
        updateNtolCalculation();
        break;

    case Page_Finish:
        updateFinishPage();
        break;

    default:
        break;
    }

    QWizard::initializePage(id);
}

bool HAWizard::validateCurrentPage()
{
    int id = currentId();

    switch (id)
    {
    case Page_ChooseSR:
        if (m_selectedHeartbeatSR.isEmpty())
        {
            QMessageBox::warning(this, tr("No SR Selected"),
                                 tr("Please select a storage repository for the HA heartbeat."));
            return false;
        }
        break;

    case Page_AssignPriorities:
        // Validate NTOL is reasonable
        if (m_ntol > m_maxNtol && m_maxNtol > 0)
        {
            QMessageBox::warning(this, tr("Invalid NTOL"),
                                 tr("The number of failures to tolerate (%1) exceeds the maximum (%2) based on current VM priorities and available resources.")
                                     .arg(m_ntol)
                                     .arg(m_maxNtol));
            return false;
        }
        break;

    default:
        break;
    }

    return QWizard::validateCurrentPage();
}

void HAWizard::accept()
{
    // Build startup options map from the VM table
    m_vmStartupOptions.clear();

    for (int row = 0; row < m_vmTable->rowCount(); ++row)
    {
        QTableWidgetItem* vmItem = m_vmTable->item(row, 0);
        if (!vmItem)
            continue;

        QString vmRef = vmItem->data(Qt::UserRole).toString();

        // Get priority from combo box
        QComboBox* priorityCombo = qobject_cast<QComboBox*>(m_vmTable->cellWidget(row, 1));
        QString priority = priorityCombo ? priorityCombo->currentData().toString() : "";

        // Get order from spinbox
        QSpinBox* orderSpin = qobject_cast<QSpinBox*>(m_vmTable->cellWidget(row, 2));
        qint64 order = orderSpin ? orderSpin->value() : 0;

        // Get delay from spinbox
        QSpinBox* delaySpin = qobject_cast<QSpinBox*>(m_vmTable->cellWidget(row, 3));
        qint64 delay = delaySpin ? delaySpin->value() : 0;

        QVariantMap options;
        options["ha_restart_priority"] = priority;
        options["order"] = order;
        options["start_delay"] = delay;

        m_vmStartupOptions[vmRef] = options;
    }

    // Create and run EnableHAAction
    EnableHAAction* action = new EnableHAAction(
        m_xenLib->getConnection(),
        m_poolRef,
        QStringList{m_selectedHeartbeatSR},
        m_ntol,
        m_vmStartupOptions,
        this);

    // Register with OperationManager for history tracking
    OperationManager::instance()->registerOperation(action);

    // Show progress dialog
    OperationProgressDialog* progressDialog = new OperationProgressDialog(action, this);

    connect(action, &AsyncOperation::completed, [this, progressDialog]() {
        progressDialog->close();
        QMessageBox::information(this, tr("HA Enabled"),
                                 tr("High Availability has been successfully enabled on pool '%1'.")
                                     .arg(m_poolName));
        QWizard::accept();
    });

    connect(action, &AsyncOperation::failed, [this, progressDialog](const QString& error) {
        progressDialog->close();
        QMessageBox::critical(this, tr("Failed to Enable HA"),
                              tr("Failed to enable High Availability:\n\n%1").arg(error));
        // Don't close wizard on failure - let user retry
    });

    action->runAsync();
    progressDialog->exec();
}

void HAWizard::scanForHeartbeatSRs()
{
    m_scanProgress->setVisible(true);
    m_rescanButton->setEnabled(false);
    m_srTable->setEnabled(false);
    m_noSRsLabel->setVisible(false);

    QApplication::processEvents();

    m_heartbeatSRs.clear();
    m_srTable->setRowCount(0);

    try
    {
        XenSession* session = m_xenLib->getConnection()->getSession();
        if (!session || !session->isLoggedIn())
        {
            throw std::runtime_error("Not connected");
        }

        // Get all SRs from cache
        QStringList srRefs = m_xenLib->getCache()->getAllRefs("sr");

        for (const QString& srRef : srRefs)
        {
            QVariantMap srData = m_xenLib->getCache()->resolve("sr", srRef);

            // Check if SR is suitable for heartbeat:
            // - Must be shared
            // - Must have at least one connected PBD
            // - Not a tools SR
            // - Not broken

            bool isShared = srData.value("shared", false).toBool();
            QString srType = srData.value("type", "").toString();

            // Skip non-shared SRs
            if (!isShared)
                continue;

            // Skip tools SRs
            if (srType == "xs-tools" || srType == "iso")
                continue;

            // Skip udev/local SRs
            if (srType == "udev" || srType == "local")
                continue;

            // Check if SR has working PBDs
            QVariantList pbds = srData.value("PBDs", QVariantList()).toList();
            bool hasConnectedPBD = false;
            for (const QVariant& pbdVar : pbds)
            {
                QString pbdRef = pbdVar.toString();
                QVariantMap pbdData = m_xenLib->getCache()->resolve("pbd", pbdRef);
                if (pbdData.value("currently_attached", false).toBool())
                {
                    hasConnectedPBD = true;
                    break;
                }
            }

            if (!hasConnectedPBD)
                continue;

            QString srName = srData.value("name_label", "Unknown").toString();
            QString srDesc = srData.value("name_description", "").toString();

            // For now, all shared SRs with connected PBDs are considered suitable
            // TODO: Add XenAPI::SR::assert_can_host_ha_statefile check when implemented
            QString status = tr("Available");

            m_heartbeatSRs.append({srRef, srName});

            int row = m_srTable->rowCount();
            m_srTable->insertRow(row);

            QTableWidgetItem* nameItem = new QTableWidgetItem(srName);
            nameItem->setData(Qt::UserRole, srRef);
            m_srTable->setItem(row, 0, nameItem);

            m_srTable->setItem(row, 1, new QTableWidgetItem(srDesc));
            m_srTable->setItem(row, 2, new QTableWidgetItem(status));

            // Disable rows that are not suitable
            if (status != tr("Available"))
            {
                for (int col = 0; col < 3; ++col)
                {
                    QTableWidgetItem* item = m_srTable->item(row, col);
                    if (item)
                    {
                        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
                        item->setForeground(Qt::gray);
                    }
                }
            }
        }

    } catch (const std::exception& e)
    {
        QMessageBox::warning(this, tr("Scan Failed"),
                             tr("Failed to scan for heartbeat SRs: %1").arg(e.what()));
    }

    m_scanProgress->setVisible(false);
    m_rescanButton->setEnabled(true);
    m_srTable->setEnabled(true);

    if (m_srTable->rowCount() == 0)
    {
        m_noSRsLabel->setVisible(true);
    }

    // Re-select previous selection if still valid
    if (!m_selectedHeartbeatSR.isEmpty())
    {
        for (int row = 0; row < m_srTable->rowCount(); ++row)
        {
            QTableWidgetItem* item = m_srTable->item(row, 0);
            if (item && item->data(Qt::UserRole).toString() == m_selectedHeartbeatSR)
            {
                if (item->flags() & Qt::ItemIsEnabled)
                {
                    m_srTable->selectRow(row);
                }
                break;
            }
        }
    }
}

void HAWizard::onHeartbeatSRSelectionChanged()
{
    QList<QTableWidgetItem*> selected = m_srTable->selectedItems();
    if (!selected.isEmpty())
    {
        QTableWidgetItem* item = m_srTable->item(selected.first()->row(), 0);
        if (item && (item->flags() & Qt::ItemIsEnabled))
        {
            m_selectedHeartbeatSR = item->data(Qt::UserRole).toString();
            m_selectedHeartbeatSRName = item->text();
        } else
        {
            m_selectedHeartbeatSR.clear();
            m_selectedHeartbeatSRName.clear();
        }
    } else
    {
        m_selectedHeartbeatSR.clear();
        m_selectedHeartbeatSRName.clear();
    }
}

void HAWizard::populateVMTable()
{
    m_vmTable->blockSignals(true);
    m_vmTable->setRowCount(0);

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
                this, [this](int) { updateNtolCalculation(); });
        m_vmTable->setCellWidget(row, 1, priorityCombo);

        // Start order spinbox
        QSpinBox* orderSpin = new QSpinBox();
        orderSpin->setRange(0, 9999);
        orderSpin->setValue(order);
        m_vmTable->setCellWidget(row, 2, orderSpin);

        // Start delay spinbox
        QSpinBox* delaySpin = new QSpinBox();
        delaySpin->setRange(0, 600);
        delaySpin->setValue(startDelay);
        delaySpin->setSuffix(tr(" sec"));
        m_vmTable->setCellWidget(row, 3, delaySpin);
    }

    m_vmTable->blockSignals(false);
}

void HAWizard::onPriorityChanged(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    updateNtolCalculation();
}

void HAWizard::onNtolChanged(int value)
{
    m_ntol = value;
    updateNtolCalculation();
}

void HAWizard::updateNtolCalculation()
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

void HAWizard::updateFinishPage()
{
    m_finishSRLabel->setText(m_selectedHeartbeatSRName);
    m_finishNtolLabel->setText(QString::number(m_ntol));

    // Count VMs by priority
    int restartCount = 0;
    int bestEffortCount = 0;
    int doNotRestartCount = 0;

    for (int row = 0; row < m_vmTable->rowCount(); ++row)
    {
        QComboBox* combo = qobject_cast<QComboBox*>(m_vmTable->cellWidget(row, 1));
        if (combo)
        {
            QString priority = combo->currentData().toString();
            if (priority == "restart")
            {
                restartCount++;
            } else if (priority == "best-effort")
            {
                bestEffortCount++;
            } else
            {
                doNotRestartCount++;
            }
        }
    }

    m_finishRestartLabel->setText(QString::number(restartCount));
    m_finishBestEffortLabel->setText(QString::number(bestEffortCount));
    m_finishDoNotRestartLabel->setText(QString::number(doNotRestartCount));

    // Show warnings if needed
    bool showWarning = false;
    QString warningText;

    if (restartCount + bestEffortCount == 0 && doNotRestartCount > 0)
    {
        showWarning = true;
        warningText = tr("No VMs are configured for restart. HA will be enabled but no VMs will be protected.");
    } else if (m_ntol == 0)
    {
        showWarning = true;
        warningText = tr("Host failures to tolerate is set to 0. HA monitoring will be enabled but VMs will not be automatically restarted.");
    }

    m_finishWarningIcon->setVisible(showWarning);
    m_finishWarningLabel->setVisible(showWarning);
    m_finishWarningLabel->setText(warningText);
}

QString HAWizard::priorityToString(HaRestartPriority priority) const
{
    switch (priority)
    {
    case AlwaysRestartHighPriority:
        return "always_restart_high_priority";
    case AlwaysRestart:
        return "restart";
    case BestEffort:
        return "best-effort";
    case DoNotRestart:
    default:
        return "";
    }
}

HAWizard::HaRestartPriority HAWizard::stringToPriority(const QString& str) const
{
    if (str == "always_restart_high_priority")
        return AlwaysRestartHighPriority;
    if (str == "restart" || str == "always_restart")
        return AlwaysRestart;
    if (str == "best-effort" || str == "best_effort")
        return BestEffort;
    return DoNotRestart;
}

int HAWizard::countVMsByPriority(HaRestartPriority priority) const
{
    int count = 0;
    for (auto it = m_vmPriorities.constBegin(); it != m_vmPriorities.constEnd(); ++it)
    {
        if (it.value() == priority)
        {
            count++;
        }
    }
    return count;
}
