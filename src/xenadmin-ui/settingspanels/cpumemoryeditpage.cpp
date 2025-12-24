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

#include "cpumemoryeditpage.h"
#include "ui_cpumemoryeditpage.h"
#include "../../xenlib/xen/actions/vm/changevcpusettingsaction.h"
#include "../../xenlib/operations/multipleoperation.h"
#include "../../xenlib/xencache.h"
#include "../../xenlib/xen/host.h"
#include "../../xenlib/xen/vm.h"
#include <QComboBox>
#include <QSlider>
#include <QStyle>
#include <QtGlobal>
#include <QtMath>
#include <algorithm>
#include <cmath>

CpuMemoryEditPage::CpuMemoryEditPage(QWidget* parent)
    : IEditPage(parent),
      ui(new Ui::CpuMemoryEditPage),
      m_validToSave(true),
      m_origVcpus(1),
      m_origVCPUsMax(1),
      m_origVCPUsAtStartup(1),
      m_origCoresPerSocket(1),
      m_prevVcpusMax(1),
      m_origVcpuWeight(0.0),
      m_currentVcpuWeight(0.0),
      m_isVcpuHotplugSupported(false),
      m_minVcpus(1),
      m_topologyOrigVcpus(1),
      m_topologyOrigCoresPerSocket(1),
      m_maxCoresPerSocket(0)
{
    this->ui->setupUi(this);

    this->ui->comboBoxVCPUs->setEditable(false);
    this->ui->comboBoxInitialVCPUs->setEditable(false);
    this->ui->comboBoxTopology->setEditable(false);

    this->ui->cpuPrioritySlider->setMinimum(0);
    this->ui->cpuPrioritySlider->setMaximum(8);
    this->ui->cpuPrioritySlider->setTickInterval(1);

    QPixmap warningPixmap(":/icons/alert_16.png");
    if (!warningPixmap.isNull())
    {
        this->ui->cpuWarningIcon->setPixmap(warningPixmap);
        this->ui->topologyWarningIcon->setPixmap(warningPixmap);
    }

    QPixmap infoPixmap = this->style()->standardIcon(QStyle::SP_MessageBoxInformation).pixmap(16, 16);
    this->ui->infoIcon->setPixmap(infoPixmap);

    this->ui->infoPanel->setVisible(false);
    this->ui->cpuWarningIcon->setVisible(false);
    this->ui->cpuWarningLabel->setVisible(false);
    this->ui->topologyWarningIcon->setVisible(false);
    this->ui->topologyWarningLabel->setVisible(false);

    connect(this->ui->comboBoxVCPUs, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CpuMemoryEditPage::onVcpusSelectionChanged);
    connect(this->ui->comboBoxInitialVCPUs, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CpuMemoryEditPage::onVcpusAtStartupSelectionChanged);
    connect(this->ui->comboBoxTopology, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CpuMemoryEditPage::onTopologySelectionChanged);
    connect(this->ui->cpuPrioritySlider, &QSlider::valueChanged,
            this, &CpuMemoryEditPage::onPriorityChanged);
}

CpuMemoryEditPage::~CpuMemoryEditPage()
{
    delete this->ui;
}

QString CpuMemoryEditPage::text() const
{
    return tr("CPU");
}

QString CpuMemoryEditPage::subText() const
{
    return tr("%1 vCPU(s)").arg(this->selectedVcpusAtStartup());
}

QIcon CpuMemoryEditPage::image() const
{
    return QIcon(":/icons/cpu_16.png");
}

void CpuMemoryEditPage::setXenObjects(const QString& objectRef,
                                      const QString& objectType,
                                      const QVariantMap& objectDataBefore,
                                      const QVariantMap& objectDataCopy)
{
    this->m_vmRef = objectRef;
    this->m_objectDataBefore = objectDataBefore;
    this->m_objectDataCopy = objectDataCopy;

    if (this->connection() && objectType.toLower() == "vm")
    {
        this->m_vm = this->connection()->GetCache()->ResolveObject<VM>("vm", objectRef);
    }
    else
    {
        this->m_vm.reset();
    }

    this->repopulate();

    emit populated();
}

void CpuMemoryEditPage::repopulate()
{
    this->ui->comboBoxVCPUs->blockSignals(true);
    this->ui->comboBoxInitialVCPUs->blockSignals(true);
    this->ui->comboBoxTopology->blockSignals(true);
    this->ui->cpuPrioritySlider->blockSignals(true);

    if (!this->m_vm)
    {
        this->ui->comboBoxVCPUs->clear();
        this->ui->comboBoxInitialVCPUs->clear();
        this->ui->comboBoxTopology->clear();
        this->ui->comboBoxVCPUs->blockSignals(false);
        this->ui->comboBoxInitialVCPUs->blockSignals(false);
        this->ui->comboBoxTopology->blockSignals(false);
        this->ui->cpuPrioritySlider->blockSignals(false);
        return;
    }

    this->m_isVcpuHotplugSupported = this->m_vm->SupportsVCPUHotplug();
    this->m_minVcpus = this->m_vm->MinVCPUs();

    this->ui->labelRubric->setText(tr("Specify the number of vCPUs, their topology, and the priority to assign them over other vCPUs. "));
    if (this->m_isVcpuHotplugSupported)
        this->ui->labelRubric->setText(this->ui->labelRubric->text() +
                                       tr("If the initial number of vCPUs is set lower than the maximum number, more vCPUs can be added to the virtual machine while it is running. "));

    if (!this->m_vm->IsHalted())
    {
        if (this->m_isVcpuHotplugSupported)
        {
            QString infoText = tr("The maximum number of vCPUs, the topology and the vCPU priority can only be changed when the VM is shut down. ");
            if (this->m_vm->GetPowerState() != "Running")
                infoText += tr("The current number of vCPUs can only be changed when the VM is running or shut down. ");
            this->ui->labelInfo->setText(infoText);
        }
        else
        {
            this->ui->labelInfo->setText(tr("The vCPUs can only be changed when the VM is shut down."));
        }

        this->ui->infoPanel->setVisible(true);
    }
    else
    {
        this->ui->infoPanel->setVisible(false);
    }

    this->m_origVCPUsMax = this->m_vm->VCPUsMax() > 0 ? this->m_vm->VCPUsMax() : 1;
    this->m_origVCPUsAtStartup = this->m_vm->VCPUsAtStartup() > 0 ? this->m_vm->VCPUsAtStartup() : 1;
    this->m_currentVcpuWeight = static_cast<double>(this->m_vm->GetVCPUWeight());
    this->m_origVcpuWeight = this->m_currentVcpuWeight;
    this->m_origVcpus = this->m_isVcpuHotplugSupported ? this->m_origVCPUsMax : this->m_origVCPUsAtStartup;
    this->m_prevVcpusMax = this->m_origVCPUsMax;
    this->m_origCoresPerSocket = this->m_vm->GetCoresPerSocket();

    this->initializeVCpuControls();
    this->m_validToSave = true;

    this->ui->comboBoxVCPUs->blockSignals(false);
    this->ui->comboBoxInitialVCPUs->blockSignals(false);
    this->ui->comboBoxTopology->blockSignals(false);
    this->ui->cpuPrioritySlider->blockSignals(false);
}

void CpuMemoryEditPage::initializeVCpuControls()
{
    this->ui->lblVCPUs->setText(this->m_isVcpuHotplugSupported
        ? tr("Maximum number of v&CPUs:")
        : tr("&Number of vCPUs:"));

    this->ui->labelInitialVCPUs->setText(this->m_vm->GetPowerState() == "Halted"
        ? tr("Initial number of v&CPUs:")
        : tr("Current number of v&CPUs:"));

    this->ui->labelInitialVCPUs->setVisible(this->m_isVcpuHotplugSupported);
    this->ui->comboBoxInitialVCPUs->setVisible(this->m_isVcpuHotplugSupported);
    this->ui->comboBoxInitialVCPUs->setEnabled(this->m_isVcpuHotplugSupported &&
                                               (this->m_vm->GetPowerState() == "Halted" ||
                                                this->m_vm->GetPowerState() == "Running"));

    this->ui->comboBoxVCPUs->setEnabled(this->m_vm->IsHalted());
    this->ui->comboBoxTopology->setEnabled(this->m_vm->IsHalted());

    this->populateTopology(this->m_vm->VCPUsAtStartup(),
                           this->m_vm->VCPUsMax(),
                           this->m_vm->GetCoresPerSocket(),
                           this->m_vm->MaxCoresPerSocket());

    qint64 maxAllowed = this->m_vm->MaxVCPUsAllowed();
    qint64 maxVcpus = maxAllowed < this->m_origVcpus ? this->m_origVcpus : maxAllowed;
    this->populateVcpus(maxVcpus, this->m_origVcpus);

    if (this->m_isVcpuHotplugSupported)
        this->populateVcpusAtStartup(this->m_origVCPUsMax, this->m_origVCPUsAtStartup);

    double weight = this->m_vm->GetVCPUWeight();
    int sliderValue = qRound(std::log(weight) / std::log(4.0));
    sliderValue = qBound(this->ui->cpuPrioritySlider->minimum(),
                         sliderValue,
                         this->ui->cpuPrioritySlider->maximum());
    this->ui->cpuPrioritySlider->setValue(sliderValue);
    this->ui->priorityPanel->setEnabled(this->m_vm->IsHalted());
}

void CpuMemoryEditPage::populateVCpuComboBox(QComboBox* comboBox,
                                             qint64 min,
                                             qint64 max,
                                             qint64 currentValue,
                                             const std::function<bool(qint64)>& isValid) const
{
    comboBox->clear();
    for (qint64 i = min; i <= max; ++i)
    {
        if (i == currentValue || isValid(i))
            comboBox->addItem(QString::number(i), QVariant::fromValue<qlonglong>(i));
    }

    if (currentValue > max)
        comboBox->addItem(QString::number(currentValue), QVariant::fromValue<qlonglong>(currentValue));

    int currentIndex = comboBox->findData(QVariant::fromValue<qlonglong>(currentValue));
    comboBox->setCurrentIndex(currentIndex >= 0 ? currentIndex : 0);
}

void CpuMemoryEditPage::populateVcpus(qint64 maxVcpus, qint64 currentVcpus)
{
    this->populateVCpuComboBox(this->ui->comboBoxVCPUs, 1, maxVcpus, currentVcpus,
                               [this](qint64 vcpus) { return this->isValidVcpu(vcpus); });
}

void CpuMemoryEditPage::populateVcpusAtStartup(qint64 maxVcpus, qint64 currentValue)
{
    qint64 min = this->m_vm->IsHalted() ? 1 : this->m_origVCPUsAtStartup;
    this->populateVCpuComboBox(this->ui->comboBoxInitialVCPUs, min, maxVcpus, currentValue,
                               [](qint64) { return true; });
}

void CpuMemoryEditPage::populateTopology(qint64 vcpusAtStartup,
                                         qint64 vcpusMax,
                                         qint64 coresPerSocket,
                                         qint64 maxCoresPerSocket)
{
    this->m_topologyOrigVcpus = vcpusAtStartup;
    this->m_topologyOrigCoresPerSocket = coresPerSocket;
    this->m_maxCoresPerSocket = maxCoresPerSocket;
    this->updateTopologyOptions(vcpusMax);
}

void CpuMemoryEditPage::updateTopologyOptions(qint64 noOfVcpus)
{
    qint64 currentCores = this->selectedCoresPerSocket();
    this->ui->comboBoxTopology->clear();

    QList<QPair<qint64, qint64>> topologies;
    qint64 maxCores = this->m_maxCoresPerSocket > 0
        ? qMin(noOfVcpus, this->m_maxCoresPerSocket)
        : noOfVcpus;

    for (qint64 cores = 1; cores <= maxCores; ++cores)
    {
        if (noOfVcpus % cores == 0 && (noOfVcpus / cores) <= VM::MAX_SOCKETS)
            topologies.append(qMakePair(noOfVcpus / cores, cores));
    }

    for (const auto& topology : topologies)
    {
        this->ui->comboBoxTopology->addItem(VM::GetTopology(topology.first, topology.second),
                                            QVariant::fromValue<qlonglong>(topology.second));
    }

    bool hasOrigCores = std::any_of(topologies.begin(), topologies.end(),
                                    [this](const QPair<qint64, qint64>& topology)
                                    {
                                        return topology.second == this->m_topologyOrigCoresPerSocket;
                                    });

    if (this->m_topologyOrigVcpus == noOfVcpus && !hasOrigCores)
    {
        this->ui->comboBoxTopology->addItem(VM::GetTopology(0, this->m_topologyOrigCoresPerSocket),
                                            QVariant::fromValue<qlonglong>(this->m_topologyOrigCoresPerSocket));
    }

    int currentIndex = this->ui->comboBoxTopology->findData(
        QVariant::fromValue<qlonglong>(currentCores));
    if (currentIndex < 0 && this->ui->comboBoxTopology->count() > 0)
        currentIndex = 0;
    this->ui->comboBoxTopology->setCurrentIndex(currentIndex);
}

bool CpuMemoryEditPage::isValidVcpu(qint64 noOfVcpus) const
{
    qint64 maxCores = this->m_maxCoresPerSocket > 0
        ? qMin(noOfVcpus, this->m_maxCoresPerSocket)
        : noOfVcpus;

    for (qint64 cores = 1; cores <= maxCores; ++cores)
    {
        if (noOfVcpus % cores == 0 && (noOfVcpus / cores) <= VM::MAX_SOCKETS)
            return true;
    }

    return false;
}

void CpuMemoryEditPage::validateVcpuSettings()
{
    if (!this->m_vm || !this->ui->comboBoxVCPUs->isEnabled())
        return;

    XenCache* cache = this->connection() ? this->connection()->GetCache() : nullptr;
    if (!cache)
        return;

    QList<QSharedPointer<Host>> hosts = cache->GetAll<Host>("host");
    int maxPhysicalCpus = 0;
    for (const QSharedPointer<Host>& host : hosts)
        maxPhysicalCpus = qMax(maxPhysicalCpus, host ? host->hostCpuCount() : 0);

    QSharedPointer<Host> homeHost = cache->ResolveObject<Host>("host", this->m_vm->HomeRef());
    int homeHostPhysicalCpus = homeHost ? homeHost->hostCpuCount() : 0;

    QStringList warnings;

    if (this->ui->comboBoxVCPUs->currentIndex() >= 0 && maxPhysicalCpus < this->selectedVcpusMax())
    {
        if (homeHost && homeHostPhysicalCpus < this->selectedVcpusMax() &&
            maxPhysicalCpus >= this->selectedVcpusMax())
        {
            warnings.append(tr("The VM's home server does not have enough physical CPUs to start the VM. The VM will start on another server."));
        }
        else if (maxPhysicalCpus < this->selectedVcpusMax())
        {
            warnings.append(tr("There are no servers with enough physical CPUs to start the VM."));
        }
    }

    if (this->ui->comboBoxVCPUs->currentIndex() >= 0 && this->selectedVcpusMax() < this->m_minVcpus)
    {
        warnings.append(tr("It is recommended to allocate at least %1 vCPUs for this VM.").arg(this->m_minVcpus));
    }

    if (this->ui->comboBoxVCPUs->currentIndex() >= 0 &&
        this->selectedVcpusMax() > VM::MAX_VCPUS_FOR_NON_TRUSTED_VMS)
    {
        warnings.append(tr("You have selected more than %1 vCPUs for the new VM. Where a VM may be running actively hostile privileged code %2 recommends that the vCPU limit is set to %1 to prevent impact on system availability.")
                        .arg(VM::MAX_VCPUS_FOR_NON_TRUSTED_VMS)
                        .arg(this->productBrand()));
    }

    if (this->ui->comboBoxInitialVCPUs->currentIndex() >= 0 &&
        this->selectedVcpusAtStartup() < this->m_minVcpus)
    {
        warnings.append(tr("It is recommended to allocate at least %1 vCPUs for this VM.").arg(this->m_minVcpus));
    }

    this->showCpuWarnings(warnings);
}

void CpuMemoryEditPage::validateTopologySettings()
{
    QStringList warnings;
    if (this->ui->comboBoxVCPUs->currentIndex() >= 0)
    {
        QString topologyWarning = VM::ValidVCPUConfiguration(this->selectedVcpusMax(),
                                                             this->selectedCoresPerSocket());
        if (!topologyWarning.isEmpty())
            warnings.append(QString("%1.").arg(topologyWarning));
    }
    this->showTopologyWarnings(warnings);
}

void CpuMemoryEditPage::refreshCurrentVcpus()
{
    if (this->ui->comboBoxInitialVCPUs->isVisible() && this->ui->comboBoxInitialVCPUs->count() > 0)
    {
        qint64 newValue = this->selectedVcpusAtStartup();

        if (this->selectedVcpusMax() < this->selectedVcpusAtStartup())
            newValue = this->selectedVcpusMax();
        else if (this->selectedVcpusAtStartup() == this->m_prevVcpusMax &&
                 this->selectedVcpusMax() != this->m_prevVcpusMax)
            newValue = this->selectedVcpusMax();

        this->populateVcpusAtStartup(this->selectedVcpusMax(), newValue);
        this->m_prevVcpusMax = this->selectedVcpusMax();
    }
}

void CpuMemoryEditPage::showCpuWarnings(const QStringList& warnings)
{
    bool show = !warnings.isEmpty();
    this->ui->cpuWarningLabel->setText(show ? warnings.join("\n\n") : QString());
    this->ui->cpuWarningIcon->setVisible(show);
    this->ui->cpuWarningLabel->setVisible(show);
}

void CpuMemoryEditPage::showTopologyWarnings(const QStringList& warnings)
{
    bool show = !warnings.isEmpty();
    this->ui->topologyWarningLabel->setText(show ? warnings.join("\n\n") : QString());
    this->ui->topologyWarningIcon->setVisible(show);
    this->ui->topologyWarningLabel->setVisible(show);
}

void CpuMemoryEditPage::updateSubText()
{
    emit populated();
}

qint64 CpuMemoryEditPage::selectedVcpusMax() const
{
    QVariant data = this->ui->comboBoxVCPUs->currentData();
    if (data.isValid())
        return data.toLongLong();

    bool ok = false;
    qint64 value = this->ui->comboBoxVCPUs->currentText().toLongLong(&ok);
    return ok ? value : this->m_origVcpus;
}

qint64 CpuMemoryEditPage::selectedVcpusAtStartup() const
{
    if (this->m_isVcpuHotplugSupported)
    {
        QVariant data = this->ui->comboBoxInitialVCPUs->currentData();
        if (data.isValid())
            return data.toLongLong();

        return this->m_origVCPUsAtStartup;
    }

    return this->selectedVcpusMax();
}

qint64 CpuMemoryEditPage::selectedCoresPerSocket() const
{
    QVariant data = this->ui->comboBoxTopology->currentData();
    if (data.isValid())
        return data.toLongLong();

    return this->m_origCoresPerSocket;
}

QString CpuMemoryEditPage::productBrand() const
{
    if (!this->connection())
        return tr("XenServer");

    XenCache* cache = this->connection()->GetCache();
    QList<QVariantMap> pools = cache->GetAllData("pool");
    if (!pools.isEmpty())
    {
        QVariantMap swVersion = pools.first().value("software_version").toMap();
        QString brand = swVersion.value("product_brand").toString();
        if (!brand.isEmpty())
            return brand;
    }

    QList<QVariantMap> hosts = cache->GetAllData("host");
    if (!hosts.isEmpty())
    {
        QVariantMap swVersion = hosts.first().value("software_version").toMap();
        QString brand = swVersion.value("product_brand").toString();
        if (!brand.isEmpty())
            return brand;
    }

    return tr("XenServer");
}

AsyncOperation* CpuMemoryEditPage::saveSettings()
{
    QList<AsyncOperation*> actions;

    if (this->m_origVcpuWeight != this->m_currentVcpuWeight)
    {
        QVariantMap vcpusParams = this->m_objectDataCopy.value("VCPUs_params").toMap();
        int weight = qRound(this->m_currentVcpuWeight);
        vcpusParams["weight"] = QString::number(weight);
        this->m_objectDataCopy["VCPUs_params"] = vcpusParams;
    }

    if (this->m_origVcpus != this->selectedVcpusMax() ||
        (this->m_isVcpuHotplugSupported && this->m_origVCPUsAtStartup != this->selectedVcpusAtStartup()))
    {
        actions.append(new ChangeVCPUSettingsAction(this->connection(),
                                                    this->m_vmRef,
                                                    this->selectedVcpusMax(),
                                                    this->selectedVcpusAtStartup(),
                                                    nullptr));
    }

    if (this->m_origCoresPerSocket != this->selectedCoresPerSocket())
    {
        QVariantMap platform = this->m_objectDataCopy.value("platform").toMap();
        platform["cores-per-socket"] = QString::number(this->selectedCoresPerSocket());
        this->m_objectDataCopy["platform"] = platform;
    }

    if (actions.isEmpty())
        return nullptr;

    if (actions.size() == 1)
        return actions.first();

    return new MultipleOperation(this->connection(),
                                 QString(),
                                 QString(),
                                 QString(),
                                 actions,
                                 true);
}

bool CpuMemoryEditPage::isValidToSave() const
{
    return this->m_validToSave;
}

void CpuMemoryEditPage::showLocalValidationMessages()
{
}

void CpuMemoryEditPage::hideLocalValidationMessages()
{
}

void CpuMemoryEditPage::cleanup()
{
}

bool CpuMemoryEditPage::hasChanged() const
{
    bool vcpuChanged = this->m_origVcpus != this->selectedVcpusMax();
    bool vcpuAtStartupChanged = this->m_isVcpuHotplugSupported &&
                                this->m_origVCPUsAtStartup != this->selectedVcpusAtStartup();
    bool topologyChanged = this->m_origCoresPerSocket != this->selectedCoresPerSocket();
    bool weightChanged = this->m_origVcpuWeight != this->m_currentVcpuWeight;
    return vcpuChanged || vcpuAtStartupChanged || topologyChanged || weightChanged;
}

QVariantMap CpuMemoryEditPage::getModifiedObjectData() const
{
    return this->m_objectDataCopy;
}

void CpuMemoryEditPage::onVcpusSelectionChanged()
{
    this->validateVcpuSettings();
    this->updateTopologyOptions(this->selectedVcpusMax());
    this->validateTopologySettings();
    this->refreshCurrentVcpus();
    this->updateSubText();
}

void CpuMemoryEditPage::onVcpusAtStartupSelectionChanged()
{
    this->validateVcpuSettings();
    this->updateSubText();
}

void CpuMemoryEditPage::onTopologySelectionChanged()
{
    this->validateTopologySettings();
}

void CpuMemoryEditPage::onPriorityChanged(int value)
{
    this->m_currentVcpuWeight = std::pow(4.0, static_cast<double>(value));
    if (value == this->ui->cpuPrioritySlider->maximum())
        this->m_currentVcpuWeight -= 1.0;
}
