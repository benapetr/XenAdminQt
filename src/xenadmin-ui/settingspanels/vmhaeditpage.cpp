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

#include "vmhaeditpage.h"
#include "ui_vmhaeditpage.h"
#include "../dialogs/hawizard.h"
#include "../dialogs/editvmhaprioritiesdialog.h"
#include "../mainwindow.h"
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xencache.h"
#include "../../xenlib/xen/xenapi/xenapi_VM.h"
#include "../../xenlib/xen/xenapi/xenapi_Pool.h"
#include "../../xenlib/xen/actions/pool/sethaprioritiesaction.h"
#include "../../xenlib/xen/actions/vm/setvmstartupoptionsaction.h"
#include <QComboBox>
#include <QLabel>
#include <QPointer>
#include <QThread>
#include <limits>

namespace
{
    const char* kPriorityRestart = "restart";
    const char* kPriorityBestEffort = "best-effort";
    const char* kPriorityAlwaysRestart = "always_restart";
    const char* kPriorityAlwaysRestartHigh = "always_restart_high_priority";
}

VMHAEditPage::VMHAEditPage(QWidget* parent)
    : IEditPage(parent),
      ui(new Ui::VMHAEditPage),
      m_origStartOrder(0),
      m_origStartDelay(0),
      m_origNtol(0),
      m_vmIsAgile(false),
      m_agilityKnown(false),
      m_ntolUpdateInProgress(false),
      m_ntol(-1),
      m_ntolMax(-1),
      m_ntolRequestId(0)
{
    ui->setupUi(this);

    ui->spinBoxStartOrder->setMaximum(std::numeric_limits<int>::max());
    ui->spinBoxStartDelay->setMaximum(std::numeric_limits<int>::max());

    ui->labelPriorityDescription->setWordWrap(true);
    ui->labelHaStatus->setWordWrap(true);
    ui->labelNtol->setWordWrap(true);
    ui->labelNtolMax->setWordWrap(true);

    ui->linkLabel->setTextFormat(Qt::RichText);
    ui->linkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->linkLabel->setOpenExternalLinks(false);

    connect(ui->comboBoxRestartPriority,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &VMHAEditPage::onPriorityChanged);
    connect(ui->linkLabel, &QLabel::linkActivated, this, &VMHAEditPage::onLinkActivated);
}

VMHAEditPage::~VMHAEditPage()
{
    delete ui;
}

QString VMHAEditPage::text() const
{
    return tr("High Availability");
}

QString VMHAEditPage::subText() const
{
    QVariantMap poolData = getPoolData();
    if (poolData.isEmpty())
        return tr("HA is not available on standalone servers.");

    bool haEnabled = poolData.value("ha_enabled", false).toBool();
    QString poolName = ellipsiseName(poolData.value("name_label", "").toString(), 30);

    if (!haEnabled)
        return tr("HA is not currently configured on pool '%1'.").arg(poolName);

    return restartPriorityDisplay(selectedPriority());
}

QIcon VMHAEditPage::image() const
{
    return QIcon(":/icons/reboot_vm_16.png");
}

void VMHAEditPage::setXenObjects(const QString& objectRef,
                                 const QString& objectType,
                                 const QVariantMap& objectDataBefore,
                                 const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectType);

    m_vmRef = objectRef;
    m_objectDataBefore = objectDataBefore;
    m_objectDataCopy = objectDataCopy;

    m_origRestartPriority = normalizePriority(objectDataBefore.value("ha_restart_priority", "").toString());
    m_origStartOrder = objectDataBefore.value("order", 0).toLongLong();
    m_origStartDelay = objectDataBefore.value("start_delay", 0).toLongLong();

    ui->spinBoxStartOrder->setValue(static_cast<int>(m_origStartOrder));
    ui->spinBoxStartDelay->setValue(static_cast<int>(m_origStartDelay));

    m_poolRef.clear();
    m_origNtol = 0;
    if (connection() && connection()->getCache())
    {
        QStringList poolRefs = connection()->getCache()->GetAllRefs("pool");
        if (!poolRefs.isEmpty())
        {
            m_poolRef = poolRefs.first();
            QVariantMap poolData = connection()->getCache()->ResolveObjectData("pool", m_poolRef);
            m_origNtol = poolData.value("ha_host_failures_to_tolerate", 0).toLongLong();
        }

        connect(connection()->getCache(),
                &XenCache::objectChanged,
                this,
                &VMHAEditPage::onCacheObjectChanged,
                Qt::UniqueConnection);
    }

    ui->scanningWidget->setVisible(true);
    ui->groupBoxHA->setVisible(false);
    m_agilityKnown = false;

    updateEnablement();
    startVmAgilityCheck();
    emit populated();
}

AsyncOperation* VMHAEditPage::saveSettings()
{
    if (!hasChanged())
        return nullptr;

    bool haChanges = isHaEditable() &&
        (selectedPriority() != m_origRestartPriority || m_ntol != m_origNtol);
    bool startupChanges =
        ui->spinBoxStartOrder->value() != m_origStartOrder ||
        ui->spinBoxStartDelay->value() != m_origStartDelay;

    if (haChanges && poolHasHAEnabled())
    {
        QMap<QString, QVariantMap> settings = buildVmStartupOptions(true);
        auto* action = new SetHaPrioritiesAction(m_connection, m_poolRef, settings, m_ntol, this);
        action->addApiMethodToRoleCheck("pool.set_ha_host_failures_to_tolerate");
        action->addApiMethodToRoleCheck("pool.sync_database");
        action->addApiMethodToRoleCheck("vm.set_ha_restart_priority");
        action->addApiMethodToRoleCheck("pool.ha_compute_hypothetical_max_host_failures_to_tolerate");
        return action;
    }

    if (startupChanges)
    {
        QMap<QString, QVariantMap> settings = buildVmStartupOptions(false);
        auto* action = new SetVMStartupOptionsAction(m_connection, m_poolRef, settings, this);
        action->addApiMethodToRoleCheck("VM.set_order");
        action->addApiMethodToRoleCheck("VM.set_start_delay");
        action->addApiMethodToRoleCheck("Pool.async_sync_database");
        return action;
    }

    return nullptr;
}

bool VMHAEditPage::isValidToSave() const
{
    if (!poolHasHAEnabled())
        return true;

    return !m_ntolUpdateInProgress && m_ntol >= 0;
}

void VMHAEditPage::showLocalValidationMessages()
{
    // No validation messages to show
}

void VMHAEditPage::hideLocalValidationMessages()
{
    // No validation messages to hide
}

void VMHAEditPage::cleanup()
{
    if (connection() && connection()->getCache())
    {
        disconnect(connection()->getCache(),
                   &XenCache::objectChanged,
                   this,
                   &VMHAEditPage::onCacheObjectChanged);
    }
}

bool VMHAEditPage::hasChanged() const
{
    bool haChanges = isHaEditable() &&
        (selectedPriority() != m_origRestartPriority || m_ntol != m_origNtol);
    bool startupChanges =
        ui->spinBoxStartOrder->value() != m_origStartOrder ||
        ui->spinBoxStartDelay->value() != m_origStartDelay;

    return haChanges || startupChanges;
}

QString VMHAEditPage::restartPriorityDisplay(const QString& priority) const
{
    if (priority == kPriorityRestart || priority == kPriorityAlwaysRestart)
        return tr("Restart");
    if (priority == kPriorityAlwaysRestartHigh)
        return tr("Restart first");
    if (priority == kPriorityBestEffort)
        return tr("Restart if possible");
    if (priority.isEmpty())
        return tr("Do not restart");

    return priority;
}

QString VMHAEditPage::restartPriorityDescription(const QString& priority) const
{
    if (priority == kPriorityAlwaysRestartHigh)
        return tr("Always try to restart VM first (highest priority)");
    if (priority == kPriorityRestart || priority == kPriorityAlwaysRestart)
        return tr("Always try to restart VM");
    if (priority == kPriorityBestEffort)
        return tr("Try to restart VM if resources are available");
    if (priority.isEmpty())
        return tr("VM will not be restarted");

    return priority;
}

QString VMHAEditPage::normalizePriority(const QString& priority) const
{
    if (priority == "0")
        return QString();
    if (priority == "1")
        return kPriorityRestart;
    if (priority == "best_effort")
        return kPriorityBestEffort;
    return priority;
}

QString VMHAEditPage::selectedPriority() const
{
    QVariant data = ui->comboBoxRestartPriority->currentData();
    if (data.isValid())
        return normalizePriority(data.toString());

    return m_origRestartPriority;
}

bool VMHAEditPage::isRestartPriority(const QString& priority) const
{
    return priority == kPriorityRestart ||
        priority == kPriorityAlwaysRestart ||
        priority == kPriorityAlwaysRestartHigh;
}

bool VMHAEditPage::vmHasVgpus() const
{
    QVariantList vgpus = m_objectDataBefore.value("VGPUs").toList();
    return !vgpus.isEmpty();
}

bool VMHAEditPage::poolHasHAEnabled() const
{
    QVariantMap poolData = getPoolData();
    return !poolData.isEmpty() && poolData.value("ha_enabled", false).toBool();
}

bool VMHAEditPage::isHaEditable() const
{
    return ui->comboBoxRestartPriority->isVisible() ||
        ui->linkLabel->isVisible() ||
        ui->labelNtol->isVisible();
}

QString VMHAEditPage::ellipsiseName(const QString& name, int maxChars) const
{
    if (name.length() <= maxChars)
        return name;

    return name.left(qMax(0, maxChars - 3)) + "...";
}

QVariantMap VMHAEditPage::getPoolData() const
{
    if (!connection() || !connection()->getCache())
        return QVariantMap();

    if (!m_poolRef.isEmpty())
        return connection()->getCache()->ResolveObjectData("pool", m_poolRef);

    QStringList poolRefs = connection()->getCache()->GetAllRefs("pool");
    if (poolRefs.isEmpty())
        return QVariantMap();

    return connection()->getCache()->ResolveObjectData("pool", poolRefs.first());
}

void VMHAEditPage::refillPrioritiesComboBox()
{
    ui->comboBoxRestartPriority->blockSignals(true);
    ui->comboBoxRestartPriority->clear();

    QStringList priorities;
    priorities << kPriorityRestart << kPriorityBestEffort << "";

    QString currentPriority = m_origRestartPriority;
    bool hasVgpus = vmHasVgpus();

    for (const QString& priority : priorities)
    {
        if (isRestartPriority(priority) && (!m_vmIsAgile || hasVgpus))
            continue;
        ui->comboBoxRestartPriority->addItem(restartPriorityDisplay(priority), priority);
    }

    int index = ui->comboBoxRestartPriority->findData(currentPriority);
    if (index < 0)
    {
        ui->comboBoxRestartPriority->insertItem(0, restartPriorityDisplay(currentPriority), currentPriority);
        index = 0;
    }
    ui->comboBoxRestartPriority->setCurrentIndex(index);

    ui->comboBoxRestartPriority->blockSignals(false);

    ui->labelPriorityDescription->setText(restartPriorityDescription(selectedPriority()));
}

void VMHAEditPage::updateEnablement()
{
    QVariantMap poolData = getPoolData();
    ui->labelHaStatus->clear();

    if (poolData.isEmpty())
    {
        ui->labelHaStatus->setText(tr("HA is not available on standalone servers."));
        ui->comboBoxRestartPriority->setVisible(false);
        ui->labelProtectionLevel->setVisible(false);
        ui->labelPriorityDescription->setVisible(false);
        ui->labelNtol->setVisible(false);
        ui->labelNtolMax->setVisible(false);
        ui->linkLabel->setVisible(false);
        return;
    }

    bool haEnabled = poolData.value("ha_enabled", false).toBool();
    QString poolName = ellipsiseName(poolData.value("name_label", "").toString(), 30);

    QString masterRef = poolData.value("master").toString();
    if (!masterRef.isEmpty() && connection() && connection()->getCache())
    {
        QVariantMap hostData = connection()->getCache()->ResolveObjectData("host", masterRef);
        QVariantMap licenseParams = hostData.value("license_params").toMap();
        bool enableHa = licenseParams.value("enable_xha", true).toBool();
        if (!enableHa)
        {
            ui->labelHaStatus->setText(tr("The server does not have a license permitting HA."));
            ui->comboBoxRestartPriority->setVisible(false);
            ui->labelProtectionLevel->setVisible(false);
            ui->labelPriorityDescription->setVisible(false);
            ui->labelNtol->setVisible(false);
            ui->labelNtolMax->setVisible(false);
            ui->linkLabel->setVisible(false);
            return;
        }
    }

    if (!haEnabled)
    {
        ui->labelHaStatus->setText(tr("HA is not currently configured on pool '%1'.").arg(poolName));
        ui->comboBoxRestartPriority->setVisible(false);
        ui->labelProtectionLevel->setVisible(false);
        ui->labelPriorityDescription->setVisible(false);
        ui->labelNtol->setVisible(false);
        ui->labelNtolMax->setVisible(false);
        ui->linkLabel->setVisible(true);
        ui->linkLabel->setText(tr("<a href=\"configure\">Configure HA now...</a>"));
        return;
    }

    QStringList deadHosts;
    if (connection() && connection()->getCache())
    {
        QStringList hostRefs = connection()->getCache()->GetAllRefs("host");
        for (const QString& hostRef : hostRefs)
        {
            QVariantMap hostData = connection()->getCache()->ResolveObjectData("host", hostRef);
            QString metricsRef = hostData.value("metrics").toString();
            QVariantMap metricsData = connection()->getCache()->ResolveObjectData("host_metrics", metricsRef);
            bool isLive = metricsData.value("live", true).toBool();
            if (!isLive)
                deadHosts << ellipsiseName(hostData.value("name_label").toString(), 30);
        }
    }

    if (!deadHosts.isEmpty())
    {
        ui->labelHaStatus->setText(tr("In order to edit the HA restart priorities of your virtual machine,\n"
                                     "all servers in the pool must be live. The following servers are\n"
                                     "not currently live:\n\n%1")
                                   .arg(deadHosts.join("\n")));
        ui->comboBoxRestartPriority->setVisible(false);
        ui->labelProtectionLevel->setVisible(false);
        ui->labelPriorityDescription->setVisible(false);
        ui->labelNtol->setVisible(false);
        ui->labelNtolMax->setVisible(false);
        ui->linkLabel->setVisible(false);
        return;
    }

    if (!m_ntolUpdateInProgress && m_ntol < 0)
    {
        ui->labelHaStatus->setText(tr("The number of server failures that can be tolerated could not be determined. Check the logs for more information."));
        ui->comboBoxRestartPriority->setVisible(false);
        ui->labelProtectionLevel->setVisible(false);
        ui->labelPriorityDescription->setVisible(false);
        ui->labelNtol->setVisible(false);
        ui->labelNtolMax->setVisible(false);
        ui->linkLabel->setVisible(false);
        return;
    }

    ui->labelHaStatus->setText(tr("HA is currently configured on pool '%1' with these settings:").arg(poolName));
    ui->comboBoxRestartPriority->setVisible(true);
    ui->labelProtectionLevel->setVisible(true);
    ui->labelPriorityDescription->setVisible(true);
    ui->labelNtol->setVisible(true);
    ui->labelNtolMax->setVisible(true);
    ui->linkLabel->setVisible(true);
    ui->linkLabel->setText(tr("<a href=\"configure\">Change these HA settings now...</a>"));

    if (m_ntolUpdateInProgress)
        updateNtolLabelsCalculating();
    else
        updateNtolLabelsSuccess();
}

void VMHAEditPage::updateNtolLabelsCalculating()
{
    ui->labelNtol->setText(tr("Calculating..."));
    ui->labelNtolMax->clear();
}

void VMHAEditPage::updateNtolLabelsSuccess()
{
    QString ntolText = tr("Server failure limit: %1").arg(m_ntol);
    if (m_ntolMax >= 0)
    {
        if (m_ntolMax < m_ntol)
            ntolText = tr("%1 - pool is overcommitted").arg(m_ntol);
        ui->labelNtolMax->setText(tr("Max failover capacity: %1").arg(m_ntolMax));
    }
    else
    {
        ui->labelNtolMax->clear();
    }

    ui->labelNtol->setText(ntolText);
}

void VMHAEditPage::updateNtolLabelsFailure()
{
    ui->labelNtol->setText(tr("Unable to calculate maximum pool failure capacity."));
    ui->labelNtolMax->clear();
}

void VMHAEditPage::startVmAgilityCheck()
{
    if (!connection() || !connection()->getSession())
    {
        m_vmIsAgile = false;
        m_agilityKnown = true;
        ui->scanningWidget->setVisible(false);
        ui->groupBoxHA->setVisible(true);
        refillPrioritiesComboBox();
        startNtolUpdate();
        updateEnablement();
        emit populated();
        return;
    }

    QPointer<VMHAEditPage> self(this);
    QThread* thread = QThread::create([self]() {
        if (!self)
            return;

        bool isAgile = false;
        XenSession* session = XenSession::duplicateSession(self->connection()->getSession(), nullptr);
        if (session)
        {
            try
            {
                XenAPI::VM::assert_agile(session, self->m_vmRef);
                isAgile = true;
            }
            catch (const std::exception&)
            {
                isAgile = false;
            }
            delete session;
        }

        QMetaObject::invokeMethod(self, [self, isAgile]() {
            if (!self)
                return;

            self->m_vmIsAgile = isAgile;
            self->m_agilityKnown = true;
            self->ui->scanningWidget->setVisible(false);
            self->ui->groupBoxHA->setVisible(true);
            self->refillPrioritiesComboBox();
            self->startNtolUpdate();
            self->updateEnablement();
            emit self->populated();
        }, Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void VMHAEditPage::startNtolUpdate()
{
    if (!connection() || !connection()->getCache() || m_poolRef.isEmpty())
        return;

    int requestId = ++m_ntolRequestId;
    m_ntolUpdateInProgress = true;
    updateNtolLabelsCalculating();

    QVariantMap ntolConfig = buildNtolConfig();
    QString poolRef = m_poolRef;

    QPointer<VMHAEditPage> self(this);
    QThread* thread = QThread::create([self, requestId, ntolConfig, poolRef]() {
        if (!self)
            return;

        qint64 ntolMax = -1;
        bool ok = false;
        XenSession* session = XenSession::duplicateSession(self->connection()->getSession(), nullptr);
        if (session)
        {
            try
            {
                ntolMax = XenAPI::Pool::ha_compute_hypothetical_max_host_failures_to_tolerate(session, ntolConfig);
                ok = true;
            }
            catch (const std::exception&)
            {
                ok = false;
            }
            delete session;
        }

        QMetaObject::invokeMethod(self, [self, requestId, ok, ntolMax, poolRef]() {
            if (!self || requestId != self->m_ntolRequestId)
                return;

            self->m_ntolUpdateInProgress = false;
            if (!ok)
            {
                self->m_ntolMax = -1;
                self->m_ntol = -1;
                self->updateNtolLabelsFailure();
                self->updateEnablement();
                emit self->populated();
                return;
            }

            self->m_ntolMax = ntolMax;
            QVariantMap poolData = self->connection()->getCache()->ResolveObjectData("pool", poolRef);
            if (poolData.value("ha_enabled", false).toBool())
                self->m_ntol = poolData.value("ha_host_failures_to_tolerate", 0).toLongLong();
            else
                self->m_ntol = self->m_ntolMax;

            self->updateNtolLabelsSuccess();
            self->updateEnablement();
            emit self->populated();
        }, Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QMap<QString, QVariantMap> VMHAEditPage::buildVmStartupOptions(bool includePriority) const
{
    QMap<QString, QVariantMap> settings;
    QVariantMap options;
    options.insert("order", static_cast<qint64>(ui->spinBoxStartOrder->value()));
    options.insert("start_delay", static_cast<qint64>(ui->spinBoxStartDelay->value()));

    if (includePriority)
        options.insert("ha_restart_priority", selectedPriority());

    settings.insert(m_vmRef, options);
    return settings;
}

QVariantMap VMHAEditPage::buildNtolConfig() const
{
    QVariantMap config;
    if (!connection() || !connection()->getCache())
        return config;

    QList<QVariantMap> vms = connection()->getCache()->GetAllData("vm");
    for (const QVariantMap& vmData : vms)
    {
        bool isTemplate = vmData.value("is_a_template", false).toBool();
        bool isSnapshot = vmData.value("is_a_snapshot", false).toBool();
        bool isControlDomain = vmData.value("is_control_domain", false).toBool();
        if (isTemplate || isSnapshot || isControlDomain)
            continue;

        QString vmRef = vmData.value("ref").toString();
        if (vmRef.isEmpty())
            vmRef = vmData.value("opaqueRef").toString();
        if (vmRef.isEmpty())
            vmRef = vmData.value("_ref").toString();
        if (vmRef.isEmpty())
            continue;

        QString priority = normalizePriority(vmData.value("ha_restart_priority", "").toString());
        if (vmRef == m_vmRef)
            priority = selectedPriority();

        if (!isRestartPriority(priority))
            continue;

        config.insert(vmRef, priority);
    }

    return config;
}

void VMHAEditPage::onPriorityChanged()
{
    ui->labelPriorityDescription->setText(restartPriorityDescription(selectedPriority()));
    startNtolUpdate();
    updateEnablement();
    emit populated();
}

void VMHAEditPage::onLinkActivated(const QString& link)
{
    Q_UNUSED(link);

    if (!connection() || !connection()->getCache())
        return;

    QVariantMap poolData = getPoolData();
    if (poolData.isEmpty())
        return;

    bool haEnabled = poolData.value("ha_enabled", false).toBool();
    MainWindow* mainWindow = qobject_cast<MainWindow*>(window());
    if (!mainWindow)
        return;

    if (haEnabled)
    {
        EditVmHaPrioritiesDialog dialog(mainWindow->xenLib(), m_poolRef, mainWindow);
        dialog.exec();
    }
    else
    {
        HAWizard wizard(mainWindow->xenLib(), m_poolRef, mainWindow);
        wizard.exec();
    }
}

void VMHAEditPage::onCacheObjectChanged(const QString& type, const QString& ref)
{
    Q_UNUSED(ref);
    if (type != "pool" && type != "host" && type != "host_metrics")
        return;

    updateEnablement();
}
