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
#include "../../xenlib/xen/network/connection.h"
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
    this->ui->setupUi(this);

    this->ui->spinBoxStartOrder->setMaximum(std::numeric_limits<int>::max());
    this->ui->spinBoxStartDelay->setMaximum(std::numeric_limits<int>::max());

    this->ui->labelPriorityDescription->setWordWrap(true);
    this->ui->labelHaStatus->setWordWrap(true);
    this->ui->labelNtol->setWordWrap(true);
    this->ui->labelNtolMax->setWordWrap(true);

    this->ui->linkLabel->setTextFormat(Qt::RichText);
    this->ui->linkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    this->ui->linkLabel->setOpenExternalLinks(false);

    connect(this->ui->comboBoxRestartPriority,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &VMHAEditPage::onPriorityChanged);
    connect(this->ui->linkLabel, &QLabel::linkActivated, this, &VMHAEditPage::onLinkActivated);
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

    this->m_vmRef = objectRef;
    this->m_objectDataBefore = objectDataBefore;
    this->m_objectDataCopy = objectDataCopy;

    this->m_origRestartPriority = this->normalizePriority(objectDataBefore.value("ha_restart_priority", "").toString());
    this->m_origStartOrder = objectDataBefore.value("order", 0).toLongLong();
    this->m_origStartDelay = objectDataBefore.value("start_delay", 0).toLongLong();

    this->ui->spinBoxStartOrder->setValue(static_cast<int>(this->m_origStartOrder));
    this->ui->spinBoxStartDelay->setValue(static_cast<int>(this->m_origStartDelay));

    this->m_poolRef.clear();
    this->m_origNtol = 0;
    if (this->connection() && this->connection()->GetCache())
    {
        QStringList poolRefs = this->connection()->GetCache()->GetAllRefs("pool");
        if (!poolRefs.isEmpty())
        {
            this->m_poolRef = poolRefs.first();
            QVariantMap poolData = this->connection()->GetCache()->ResolveObjectData("pool", this->m_poolRef);
            this->m_origNtol = poolData.value("ha_host_failures_to_tolerate", 0).toLongLong();
        }

        connect(this->connection()->GetCache(),
                &XenCache::objectChanged,
                this,
                &VMHAEditPage::onCacheObjectChanged,
                Qt::UniqueConnection);
    }

    this->ui->scanningWidget->setVisible(true);
    this->ui->groupBoxHA->setVisible(false);
    this->m_agilityKnown = false;

    this->updateEnablement();
    this->startVmAgilityCheck();
    emit populated();
}

AsyncOperation* VMHAEditPage::saveSettings()
{
    if (!this->hasChanged())
        return nullptr;

    bool haChanges = this->isHaEditable() &&
        (this->selectedPriority() != this->m_origRestartPriority || this->m_ntol != this->m_origNtol);
    bool startupChanges =
        this->ui->spinBoxStartOrder->value() != this->m_origStartOrder ||
        this->ui->spinBoxStartDelay->value() != this->m_origStartDelay;

    if (haChanges && this->poolHasHAEnabled())
    {
        QMap<QString, QVariantMap> settings = this->buildVmStartupOptions(true);
        auto* action = new SetHaPrioritiesAction(this->m_connection, this->m_poolRef, settings, this->m_ntol, this);
        action->addApiMethodToRoleCheck("pool.set_ha_host_failures_to_tolerate");
        action->addApiMethodToRoleCheck("pool.sync_database");
        action->addApiMethodToRoleCheck("vm.set_ha_restart_priority");
        action->addApiMethodToRoleCheck("pool.ha_compute_hypothetical_max_host_failures_to_tolerate");
        return action;
    }

    if (startupChanges)
    {
        QMap<QString, QVariantMap> settings = this->buildVmStartupOptions(false);
        auto* action = new SetVMStartupOptionsAction(this->m_connection, this->m_poolRef, settings, this);
        action->addApiMethodToRoleCheck("VM.set_order");
        action->addApiMethodToRoleCheck("VM.set_start_delay");
        action->addApiMethodToRoleCheck("Pool.async_sync_database");
        return action;
    }

    return nullptr;
}

bool VMHAEditPage::isValidToSave() const
{
    if (!this->poolHasHAEnabled())
        return true;

    return !this->m_ntolUpdateInProgress && this->m_ntol >= 0;
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
    if (this->connection() && this->connection()->GetCache())
    {
        disconnect(this->connection()->GetCache(),
                   &XenCache::objectChanged,
                   this,
                   &VMHAEditPage::onCacheObjectChanged);
    }
}

bool VMHAEditPage::hasChanged() const
{
    bool haChanges = this->isHaEditable() &&
        (this->selectedPriority() != this->m_origRestartPriority || this->m_ntol != this->m_origNtol);
    bool startupChanges =
        this->ui->spinBoxStartOrder->value() != this->m_origStartOrder ||
        this->ui->spinBoxStartDelay->value() != this->m_origStartDelay;

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
    QVariant data = this->ui->comboBoxRestartPriority->currentData();
    if (data.isValid())
        return this->normalizePriority(data.toString());

    return this->m_origRestartPriority;
}

bool VMHAEditPage::isRestartPriority(const QString& priority) const
{
    return priority == kPriorityRestart ||
        priority == kPriorityAlwaysRestart ||
        priority == kPriorityAlwaysRestartHigh;
}

bool VMHAEditPage::vmHasVgpus() const
{
    QVariantList vgpus = this->m_objectDataBefore.value("VGPUs").toList();
    return !vgpus.isEmpty();
}

bool VMHAEditPage::poolHasHAEnabled() const
{
    QVariantMap poolData = this->getPoolData();
    return !poolData.isEmpty() && poolData.value("ha_enabled", false).toBool();
}

bool VMHAEditPage::isHaEditable() const
{
    return this->ui->comboBoxRestartPriority->isVisible() ||
        this->ui->linkLabel->isVisible() ||
        this->ui->labelNtol->isVisible();
}

QString VMHAEditPage::ellipsiseName(const QString& name, int maxChars) const
{
    if (name.length() <= maxChars)
        return name;

    return name.left(qMax(0, maxChars - 3)) + "...";
}

QVariantMap VMHAEditPage::getPoolData() const
{
    if (!this->connection() || !this->connection()->GetCache())
        return QVariantMap();

    if (!this->m_poolRef.isEmpty())
        return this->connection()->GetCache()->ResolveObjectData("pool", this->m_poolRef);

    QStringList poolRefs = this->connection()->GetCache()->GetAllRefs("pool");
    if (poolRefs.isEmpty())
        return QVariantMap();

    return this->connection()->GetCache()->ResolveObjectData("pool", poolRefs.first());
}

void VMHAEditPage::refillPrioritiesComboBox()
{
    this->ui->comboBoxRestartPriority->blockSignals(true);
    this->ui->comboBoxRestartPriority->clear();

    QStringList priorities;
    priorities << kPriorityRestart << kPriorityBestEffort << "";

    QString currentPriority = this->m_origRestartPriority;
    bool hasVgpus = this->vmHasVgpus();

    for (const QString& priority : priorities)
    {
        if (this->isRestartPriority(priority) && (!this->m_vmIsAgile || hasVgpus))
            continue;
        this->ui->comboBoxRestartPriority->addItem(this->restartPriorityDisplay(priority), priority);
    }

    int index = this->ui->comboBoxRestartPriority->findData(currentPriority);
    if (index < 0)
    {
        this->ui->comboBoxRestartPriority->insertItem(0, this->restartPriorityDisplay(currentPriority), currentPriority);
        index = 0;
    }
    this->ui->comboBoxRestartPriority->setCurrentIndex(index);

    this->ui->comboBoxRestartPriority->blockSignals(false);

    this->ui->labelPriorityDescription->setText(this->restartPriorityDescription(this->selectedPriority()));
}

void VMHAEditPage::updateEnablement()
{
    QVariantMap poolData = this->getPoolData();
    this->ui->labelHaStatus->clear();

    if (poolData.isEmpty())
    {
        this->ui->labelHaStatus->setText(tr("HA is not available on standalone servers."));
        this->ui->comboBoxRestartPriority->setVisible(false);
        this->ui->labelProtectionLevel->setVisible(false);
        this->ui->labelPriorityDescription->setVisible(false);
        this->ui->labelNtol->setVisible(false);
        this->ui->labelNtolMax->setVisible(false);
        this->ui->linkLabel->setVisible(false);
        return;
    }

    bool haEnabled = poolData.value("ha_enabled", false).toBool();
    QString poolName = this->ellipsiseName(poolData.value("name_label", "").toString(), 30);

    QString masterRef = poolData.value("master").toString();
    if (!masterRef.isEmpty() && this->connection() && this->connection()->GetCache())
    {
        QVariantMap hostData = this->connection()->GetCache()->ResolveObjectData("host", masterRef);
        QVariantMap licenseParams = hostData.value("license_params").toMap();
        bool enableHa = licenseParams.value("enable_xha", true).toBool();
        if (!enableHa)
        {
            this->ui->labelHaStatus->setText(tr("The server does not have a license permitting HA."));
            this->ui->comboBoxRestartPriority->setVisible(false);
            this->ui->labelProtectionLevel->setVisible(false);
            this->ui->labelPriorityDescription->setVisible(false);
            this->ui->labelNtol->setVisible(false);
            this->ui->labelNtolMax->setVisible(false);
            this->ui->linkLabel->setVisible(false);
            return;
        }
    }

    if (!haEnabled)
    {
        this->ui->labelHaStatus->setText(tr("HA is not currently configured on pool '%1'.").arg(poolName));
        this->ui->comboBoxRestartPriority->setVisible(false);
        this->ui->labelProtectionLevel->setVisible(false);
        this->ui->labelPriorityDescription->setVisible(false);
        this->ui->labelNtol->setVisible(false);
        this->ui->labelNtolMax->setVisible(false);
        this->ui->linkLabel->setVisible(true);
        this->ui->linkLabel->setText(tr("<a href=\"configure\">Configure HA now...</a>"));
        return;
    }

    QStringList deadHosts;
    if (this->connection() && this->connection()->GetCache())
    {
        QStringList hostRefs = this->connection()->GetCache()->GetAllRefs("host");
        for (const QString& hostRef : hostRefs)
        {
            QVariantMap hostData = this->connection()->GetCache()->ResolveObjectData("host", hostRef);
            QString metricsRef = hostData.value("metrics").toString();
            QVariantMap metricsData = this->connection()->GetCache()->ResolveObjectData("host_metrics", metricsRef);
            bool isLive = metricsData.value("live", true).toBool();
            if (!isLive)
                deadHosts << this->ellipsiseName(hostData.value("name_label").toString(), 30);
        }
    }

    if (!deadHosts.isEmpty())
    {
        this->ui->labelHaStatus->setText(tr("In order to edit the HA restart priorities of your virtual machine,\n"
                                     "all servers in the pool must be live. The following servers are\n"
                                     "not currently live:\n\n%1")
                                   .arg(deadHosts.join("\n")));
        this->ui->comboBoxRestartPriority->setVisible(false);
        this->ui->labelProtectionLevel->setVisible(false);
        this->ui->labelPriorityDescription->setVisible(false);
        this->ui->labelNtol->setVisible(false);
        this->ui->labelNtolMax->setVisible(false);
        this->ui->linkLabel->setVisible(false);
        return;
    }

    if (!this->m_ntolUpdateInProgress && this->m_ntol < 0)
    {
        this->ui->labelHaStatus->setText(tr("The number of server failures that can be tolerated could not be determined. Check the logs for more information."));
        this->ui->comboBoxRestartPriority->setVisible(false);
        this->ui->labelProtectionLevel->setVisible(false);
        this->ui->labelPriorityDescription->setVisible(false);
        this->ui->labelNtol->setVisible(false);
        this->ui->labelNtolMax->setVisible(false);
        this->ui->linkLabel->setVisible(false);
        return;
    }

    this->ui->labelHaStatus->setText(tr("HA is currently configured on pool '%1' with these settings:").arg(poolName));
    this->ui->comboBoxRestartPriority->setVisible(true);
    this->ui->labelProtectionLevel->setVisible(true);
    this->ui->labelPriorityDescription->setVisible(true);
    this->ui->labelNtol->setVisible(true);
    this->ui->labelNtolMax->setVisible(true);
    this->ui->linkLabel->setVisible(true);
    this->ui->linkLabel->setText(tr("<a href=\"configure\">Change these HA settings now...</a>"));

    if (this->m_ntolUpdateInProgress)
        this->updateNtolLabelsCalculating();
    else
        this->updateNtolLabelsSuccess();
}

void VMHAEditPage::updateNtolLabelsCalculating()
{
    ui->labelNtol->setText(tr("Calculating..."));
    ui->labelNtolMax->clear();
}

void VMHAEditPage::updateNtolLabelsSuccess()
{
    QString ntolText = tr("Server failure limit: %1").arg(this->m_ntol);
    if (this->m_ntolMax >= 0)
    {
        if (this->m_ntolMax < this->m_ntol)
            ntolText = tr("%1 - pool is overcommitted").arg(this->m_ntol);
        this->ui->labelNtolMax->setText(tr("Max failover capacity: %1").arg(this->m_ntolMax));
    }
    else
    {
        this->ui->labelNtolMax->clear();
    }

    this->ui->labelNtol->setText(ntolText);
}

void VMHAEditPage::updateNtolLabelsFailure()
{
    ui->labelNtol->setText(tr("Unable to calculate maximum pool failure capacity."));
    ui->labelNtolMax->clear();
}

void VMHAEditPage::startVmAgilityCheck()
{
    if (!this->connection() || !this->connection()->GetSession())
    {
        this->m_vmIsAgile = false;
        this->m_agilityKnown = true;
        this->ui->scanningWidget->setVisible(false);
        this->ui->groupBoxHA->setVisible(true);
        this->refillPrioritiesComboBox();
        this->startNtolUpdate();
        this->updateEnablement();
        emit populated();
        return;
    }

    QPointer<VMHAEditPage> self(this);
    QThread* thread = QThread::create([self]() {
        if (!self)
            return;

        bool isAgile = false;
        XenAPI::Session* session = XenAPI::Session::DuplicateSession(self->connection()->GetSession(), nullptr);
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
    if (!this->connection() || !this->connection()->GetCache() || this->m_poolRef.isEmpty())
        return;

    int requestId = ++this->m_ntolRequestId;
    this->m_ntolUpdateInProgress = true;
    this->updateNtolLabelsCalculating();

    QVariantMap ntolConfig = this->buildNtolConfig();
    QString poolRef = this->m_poolRef;

    QPointer<VMHAEditPage> self(this);
    QThread* thread = QThread::create([self, requestId, ntolConfig, poolRef]() {
        if (!self)
            return;

        qint64 ntolMax = -1;
        bool ok = false;
        XenAPI::Session* session = XenAPI::Session::DuplicateSession(self->connection()->GetSession(), nullptr);
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
            QVariantMap poolData = self->connection()->GetCache()->ResolveObjectData("pool", poolRef);
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
    options.insert("order", static_cast<qint64>(this->ui->spinBoxStartOrder->value()));
    options.insert("start_delay", static_cast<qint64>(this->ui->spinBoxStartDelay->value()));

    if (includePriority)
        options.insert("ha_restart_priority", this->selectedPriority());

    settings.insert(this->m_vmRef, options);
    return settings;
}

QVariantMap VMHAEditPage::buildNtolConfig() const
{
    QVariantMap config;
    if (!this->connection() || !this->connection()->GetCache())
        return config;

    QList<QVariantMap> vms = this->connection()->GetCache()->GetAllData("vm");
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

        QString priority = this->normalizePriority(vmData.value("ha_restart_priority", "").toString());
        if (vmRef == this->m_vmRef)
            priority = this->selectedPriority();

        if (!this->isRestartPriority(priority))
            continue;

        config.insert(vmRef, priority);
    }

    return config;
}

void VMHAEditPage::onPriorityChanged()
{
    this->ui->labelPriorityDescription->setText(this->restartPriorityDescription(this->selectedPriority()));
    this->startNtolUpdate();
    this->updateEnablement();
    emit populated();
}

void VMHAEditPage::onLinkActivated(const QString& link)
{
    Q_UNUSED(link);

    if (!this->connection() || !this->connection()->GetCache())
        return;

    QVariantMap poolData = this->getPoolData();
    if (poolData.isEmpty())
        return;

    bool haEnabled = poolData.value("ha_enabled", false).toBool();
    MainWindow* mainWindow = qobject_cast<MainWindow*>(this->window());
    if (!mainWindow)
        return;

    if (haEnabled)
    {
        EditVmHaPrioritiesDialog dialog(this->connection(), this->m_poolRef, mainWindow);
        dialog.exec();
    }
    else
    {
        HAWizard wizard(this->connection(), this->m_poolRef, mainWindow);
        wizard.exec();
    }
}

void VMHAEditPage::onCacheObjectChanged(const QString& type, const QString& ref)
{
    Q_UNUSED(ref);
    if (type != "pool" && type != "host" && type != "host_metrics")
        return;

    this->updateEnablement();
}
