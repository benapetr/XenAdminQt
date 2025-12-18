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
#include "../../xenlib/xen/asyncoperation.h"
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/api.h"
#include <QMessageBox>
#include <QToolTip>

CpuMemoryEditPage::CpuMemoryEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::CpuMemoryEditPage), m_origVCPUsMax(1), m_origVCPUsAtStartup(1), m_origCoresPerSocket(1)
{
    ui->setupUi(this);

    connect(ui->spinBoxVCPUsMax, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CpuMemoryEditPage::onVCPUsMaxChanged);
    connect(ui->spinBoxVCPUsAtStartup, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CpuMemoryEditPage::onVCPUsAtStartupChanged);
}

CpuMemoryEditPage::~CpuMemoryEditPage()
{
    delete ui;
}

QString CpuMemoryEditPage::text() const
{
    return tr("CPU and Memory");
}

QString CpuMemoryEditPage::subText() const
{
    return tr("%1 vCPUs").arg(ui->spinBoxVCPUsAtStartup->value());
}

QIcon CpuMemoryEditPage::image() const
{
    // TODO: Use proper CPU icon from resources
    return QIcon::fromTheme("cpu");
}

void CpuMemoryEditPage::setXenObjects(const QString& objectRef,
                                      const QString& objectType,
                                      const QVariantMap& objectDataBefore,
                                      const QVariantMap& objectDataCopy)
{
    m_vmRef = objectRef;
    m_objectDataBefore = objectDataBefore;
    m_objectDataCopy = objectDataCopy;

    // Get current values from VM data
    m_origVCPUsMax = objectDataBefore.value("VCPUs_max", 1).toLongLong();
    m_origVCPUsAtStartup = objectDataBefore.value("VCPUs_at_startup", 1).toLongLong();

    // Get CPU topology from platform data
    QVariantMap platform = objectDataBefore.value("platform").toMap();
    m_origCoresPerSocket = platform.value("cores-per-socket", 1).toInt();

    // Populate UI
    ui->spinBoxVCPUsMax->setValue(m_origVCPUsMax);
    ui->spinBoxVCPUsAtStartup->setValue(m_origVCPUsAtStartup);

    // Ensure VCPUsAtStartup max matches VCPUsMax
    ui->spinBoxVCPUsAtStartup->setMaximum(m_origVCPUsMax);

    populateTopology();

    // Check power state for enabling/disabling controls
    QString powerState = objectDataBefore.value("power_state", "").toString();
    bool isHalted = (powerState == "Halted");

    // VCPUs max can only be changed when halted
    ui->spinBoxVCPUsMax->setEnabled(isHalted);
    ui->comboBoxTopology->setEnabled(isHalted);

    // VCPUs at startup can be changed when halted or running (hotplug)
    // For simplicity, allow when halted or running
    bool canChangeAtStartup = (isHalted || powerState == "Running");
    ui->spinBoxVCPUsAtStartup->setEnabled(canChangeAtStartup);

    if (!isHalted && powerState == "Running")
    {
        ui->labelRubric->setText(tr("Configure the number of virtual CPUs. Maximum vCPUs can only be changed when the VM is halted. Current vCPUs can be adjusted while running (hot-plug)."));
    }
}

AsyncOperation* CpuMemoryEditPage::saveSettings()
{
    if (!hasChanged())
    {
        return nullptr;
    }

    long newVCPUsMax = ui->spinBoxVCPUsMax->value();
    long newVCPUsAtStartup = ui->spinBoxVCPUsAtStartup->value();
    int newCoresPerSocket = 1; // TODO: Get from topology combobox when implemented

    // Update objectDataCopy with new values - cast to qlonglong for QVariant
    m_objectDataCopy["VCPUs_max"] = (qlonglong) newVCPUsMax;
    m_objectDataCopy["VCPUs_at_startup"] = (qlonglong) newVCPUsAtStartup;

    if (newCoresPerSocket != m_origCoresPerSocket)
    {
        QVariantMap platform = m_objectDataCopy.value("platform").toMap();
        platform["cores-per-socket"] = newCoresPerSocket;
        m_objectDataCopy["platform"] = platform;
    }

    // Return inline AsyncOperation (DelegatedAsyncAction pattern)
    // This matches C# ChangeVCPUSettingsAction
    class ChangeVCPUSettingsOperation : public AsyncOperation
    {
    public:
        ChangeVCPUSettingsOperation(XenConnection* conn,
                                    const QString& vmRef,
                                    long vcpusMax,
                                    long vcpusAtStartup,
                                    const QString& powerState,
                                    QObject* parent)
            : AsyncOperation(conn, tr("Change vCPU Settings"),
                             tr("Changing virtual CPU configuration..."), parent),
              m_vmRef(vmRef), m_vcpusMax(vcpusMax), m_vcpusAtStartup(vcpusAtStartup), m_powerState(powerState)
        {}

    protected:
        void run() override
        {
            XenRpcAPI api(connection()->getSession());

            setPercentComplete(10);

            // If VM is running, we can only change VCPUs_at_startup (hot-plug)
            if (m_powerState == "Running")
            {
                // VM.set_VCPUs_number_live(session, vm, vcpus_at_startup)
                QVariantList params;
                params << connection()->getSessionId() << m_vmRef << (qlonglong) m_vcpusAtStartup;

                QByteArray request = api.buildJsonRpcCall("VM.set_VCPUs_number_live", params);
                connection()->sendRequest(request);

                setPercentComplete(100);
                return;
            }

            // VM is halted - we can change both VCPUs_max and VCPUs_at_startup
            // Order matters: if reducing, change at_startup first; if increasing, change max first

            if (m_vcpusAtStartup < m_vcpusMax)
            {
                // Reducing: set VCPUs_at_startup first
                QVariantList params1;
                params1 << connection()->getSessionId() << m_vmRef << (qlonglong) m_vcpusAtStartup;
                QByteArray request1 = api.buildJsonRpcCall("VM.set_VCPUs_at_startup", params1);
                connection()->sendRequest(request1);

                setPercentComplete(50);

                QVariantList params2;
                params2 << connection()->getSessionId() << m_vmRef << (qlonglong) m_vcpusMax;
                QByteArray request2 = api.buildJsonRpcCall("VM.set_VCPUs_max", params2);
                connection()->sendRequest(request2);
            } else
            {
                // Increasing: set VCPUs_max first
                QVariantList params1;
                params1 << connection()->getSessionId() << m_vmRef << (qlonglong) m_vcpusMax;
                QByteArray request1 = api.buildJsonRpcCall("VM.set_VCPUs_max", params1);
                connection()->sendRequest(request1);

                setPercentComplete(50);

                QVariantList params2;
                params2 << connection()->getSessionId() << m_vmRef << (qlonglong) m_vcpusAtStartup;
                QByteArray request2 = api.buildJsonRpcCall("VM.set_VCPUs_at_startup", params2);
                connection()->sendRequest(request2);
            }

            setPercentComplete(100);
        }

    private:
        QString m_vmRef;
        long m_vcpusMax;
        long m_vcpusAtStartup;
        QString m_powerState;
    };

    QString powerState = m_objectDataBefore.value("power_state", "").toString();
    return new ChangeVCPUSettingsOperation(this->m_connection, this->m_vmRef,
                                           newVCPUsMax, newVCPUsAtStartup,
                                           powerState, nullptr);
}

bool CpuMemoryEditPage::isValidToSave() const
{
    long vcpusMax = ui->spinBoxVCPUsMax->value();
    long vcpusAtStartup = ui->spinBoxVCPUsAtStartup->value();

    // VCPUs at startup cannot exceed VCPUs max
    if (vcpusAtStartup > vcpusMax)
    {
        return false;
    }

    // Both must be at least 1
    if (vcpusMax < 1 || vcpusAtStartup < 1)
    {
        return false;
    }

    return true;
}

void CpuMemoryEditPage::showLocalValidationMessages()
{
    if (!isValidToSave())
    {
        long vcpusMax = ui->spinBoxVCPUsMax->value();
        long vcpusAtStartup = ui->spinBoxVCPUsAtStartup->value();

        if (vcpusAtStartup > vcpusMax)
        {
            QMessageBox::warning(this, tr("Invalid vCPU Configuration"),
                                 tr("Initial vCPUs (%1) cannot exceed maximum vCPUs (%2).")
                                     .arg(vcpusAtStartup)
                                     .arg(vcpusMax));
        }
    }
}

void CpuMemoryEditPage::hideLocalValidationMessages()
{
    // No persistent messages to hide
}

void CpuMemoryEditPage::cleanup()
{
    // Nothing to clean up
}

bool CpuMemoryEditPage::hasChanged() const
{
    long currentVCPUsMax = ui->spinBoxVCPUsMax->value();
    long currentVCPUsAtStartup = ui->spinBoxVCPUsAtStartup->value();

    return (currentVCPUsMax != m_origVCPUsMax ||
            currentVCPUsAtStartup != m_origVCPUsAtStartup);
    // TODO: Also check topology when implemented
}

void CpuMemoryEditPage::onVCPUsMaxChanged(int value)
{
    // Ensure VCPUs at startup doesn't exceed max
    if (ui->spinBoxVCPUsAtStartup->value() > value)
    {
        ui->spinBoxVCPUsAtStartup->setValue(value);
    }
    ui->spinBoxVCPUsAtStartup->setMaximum(value);

    updateSubText();
}

void CpuMemoryEditPage::onVCPUsAtStartupChanged(int value)
{
    updateSubText();
}

void CpuMemoryEditPage::populateTopology()
{
    // TODO: Implement topology dropdown
    // For now, add a simple "1 core per socket" option
    ui->comboBoxTopology->clear();
    ui->comboBoxTopology->addItem(tr("1 core per socket (default)"));
    ui->comboBoxTopology->setCurrentIndex(0);
}

void CpuMemoryEditPage::updateSubText()
{
    // Trigger subText update if needed
    // The parent dialog will query subText() when needed
}
