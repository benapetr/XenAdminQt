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

#include "perfmonalerteditpage.h"
#include "ui_perfmonalerteditpage.h"
#include "../../xenlib/xen/asyncoperation.h"
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/api.h"

PerfmonAlertEditPage::PerfmonAlertEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::PerfmonAlertEditPage)
{
    ui->setupUi(this);
}

PerfmonAlertEditPage::~PerfmonAlertEditPage()
{
    delete ui;
}

QString PerfmonAlertEditPage::text() const
{
    return tr("Alerts");
}

QString PerfmonAlertEditPage::subText() const
{
    QStringList alerts;

    if (ui->groupBoxCPU->isChecked())
    {
        alerts << tr("CPU: %1%%").arg(ui->spinBoxCPUThreshold->value());
    }

    if (ui->groupBoxMemory->isChecked())
    {
        alerts << tr("Memory: %1 MB").arg(ui->spinBoxMemoryThreshold->value());
    }

    if (alerts.isEmpty())
    {
        return tr("No alerts configured");
    }

    return alerts.join(", ");
}

QIcon PerfmonAlertEditPage::image() const
{
    return QIcon::fromTheme("dialog-warning");
}

void PerfmonAlertEditPage::setXenObjects(const QString& objectRef,
                                         const QString& objectType,
                                         const QVariantMap& objectDataBefore,
                                         const QVariantMap& objectDataCopy)
{
    m_objectRef = objectRef;
    m_objectType = objectType;
    m_objectDataBefore = objectDataBefore;
    m_objectDataCopy = objectDataCopy;

    QVariantMap otherConfig = objectDataBefore.value("other_config").toMap();

    // Load CPU alert configuration
    m_origCPUAlert = getAlertConfig(otherConfig, "perfmon_cpu");
    ui->groupBoxCPU->setChecked(m_origCPUAlert.enabled);
    ui->spinBoxCPUThreshold->setValue(m_origCPUAlert.threshold * 100); // Convert fraction to percentage
    ui->spinBoxCPUDuration->setValue(m_origCPUAlert.duration / 60);    // Convert seconds to minutes

    // Load memory alert configuration
    m_origMemoryAlert = getAlertConfig(otherConfig, "perfmon_memory_free");
    ui->groupBoxMemory->setChecked(m_origMemoryAlert.enabled);
    ui->spinBoxMemoryThreshold->setValue(m_origMemoryAlert.threshold / 1024); // Convert KiB to MB
    ui->spinBoxMemoryDuration->setValue(m_origMemoryAlert.duration / 60);     // Convert seconds to minutes
}

AsyncOperation* PerfmonAlertEditPage::saveSettings()
{
    if (!hasChanged())
    {
        return nullptr;
    }

    // Update objectDataCopy with new alert configurations
    QVariantMap otherConfig = m_objectDataCopy.value("other_config").toMap();

    // CPU alert
    AlertConfig cpuAlert;
    cpuAlert.enabled = ui->groupBoxCPU->isChecked();
    cpuAlert.threshold = ui->spinBoxCPUThreshold->value() / 100.0; // Convert percentage to fraction
    cpuAlert.duration = ui->spinBoxCPUDuration->value() * 60;      // Convert minutes to seconds
    setAlertConfig(otherConfig, "perfmon_cpu", cpuAlert);

    // Memory alert
    AlertConfig memoryAlert;
    memoryAlert.enabled = ui->groupBoxMemory->isChecked();
    memoryAlert.threshold = ui->spinBoxMemoryThreshold->value() * 1024; // Convert MB to KiB
    memoryAlert.duration = ui->spinBoxMemoryDuration->value() * 60;     // Convert minutes to seconds
    setAlertConfig(otherConfig, "perfmon_memory_free", memoryAlert);

    m_objectDataCopy["other_config"] = otherConfig;

    // Return inline AsyncOperation
    class PerfmonAlertOperation : public AsyncOperation
    {
    public:
        PerfmonAlertOperation(XenConnection* conn,
                              const QString& objectRef,
                              const QString& objectType,
                              const QVariantMap& otherConfig,
                              QObject* parent)
            : AsyncOperation(conn, tr("Update Performance Alerts"),
                             tr("Updating performance alert configuration..."), parent),
              m_objectRef(objectRef), m_objectType(objectType), m_otherConfig(otherConfig)
        {}

    protected:
        void run() override
        {
            XenRpcAPI api(connection()->getSession());

            setPercentComplete(30);

            // Build method name based on object type
            QString methodName = m_objectType + ".set_other_config";

            QVariantList params;
            params << connection()->getSessionId() << m_objectRef << m_otherConfig;
            QByteArray request = api.buildJsonRpcCall(methodName, params);
            connection()->sendRequest(request);

            setPercentComplete(100);
        }

    private:
        QString m_objectRef;
        QString m_objectType;
        QVariantMap m_otherConfig;
    };

    return new PerfmonAlertOperation(m_connection, m_objectRef, m_objectType, otherConfig, this);
}

bool PerfmonAlertEditPage::isValidToSave() const
{
    return true;
}

void PerfmonAlertEditPage::showLocalValidationMessages()
{
    // No validation needed
}

void PerfmonAlertEditPage::hideLocalValidationMessages()
{
    // No validation messages
}

void PerfmonAlertEditPage::cleanup()
{
    // Nothing to clean up
}

bool PerfmonAlertEditPage::hasChanged() const
{
    // Check CPU alert
    AlertConfig currentCPU;
    currentCPU.enabled = ui->groupBoxCPU->isChecked();
    currentCPU.threshold = ui->spinBoxCPUThreshold->value() / 100.0;
    currentCPU.duration = ui->spinBoxCPUDuration->value() * 60;

    if (currentCPU.enabled != m_origCPUAlert.enabled ||
        qAbs(currentCPU.threshold - m_origCPUAlert.threshold) > 0.01 ||
        currentCPU.duration != m_origCPUAlert.duration)
    {
        return true;
    }

    // Check memory alert
    AlertConfig currentMemory;
    currentMemory.enabled = ui->groupBoxMemory->isChecked();
    currentMemory.threshold = ui->spinBoxMemoryThreshold->value() * 1024;
    currentMemory.duration = ui->spinBoxMemoryDuration->value() * 60;

    if (currentMemory.enabled != m_origMemoryAlert.enabled ||
        qAbs(currentMemory.threshold - m_origMemoryAlert.threshold) > 0.01 ||
        currentMemory.duration != m_origMemoryAlert.duration)
    {
        return true;
    }

    return false;
}

PerfmonAlertEditPage::AlertConfig PerfmonAlertEditPage::getAlertConfig(
    const QVariantMap& otherConfig, const QString& prefix) const
{
    AlertConfig config;
    config.enabled = false;
    config.threshold = 0.0;
    config.duration = 0;

    // Check if alert is configured (has trigger_level key)
    QString triggerKey = "perfmon_" + prefix + "_trigger_level";
    if (otherConfig.contains(triggerKey))
    {
        config.enabled = true;
        config.threshold = otherConfig.value(triggerKey, 0.0).toDouble();

        QString periodKey = "perfmon_" + prefix + "_trigger_period";
        config.duration = otherConfig.value(periodKey, 0).toInt();
    }

    return config;
}

void PerfmonAlertEditPage::setAlertConfig(QVariantMap& otherConfig,
                                          const QString& prefix,
                                          const AlertConfig& config)
{
    QString triggerKey = "perfmon_" + prefix + "_trigger_level";
    QString periodKey = "perfmon_" + prefix + "_trigger_period";
    QString intervalKey = "perfmon_" + prefix + "_alert_interval";

    if (config.enabled)
    {
        otherConfig[triggerKey] = QString::number(config.threshold, 'f', 4);
        otherConfig[periodKey] = QString::number(config.duration);
        otherConfig[intervalKey] = QString::number(config.duration); // Same as period by default
    } else
    {
        // Remove alert configuration keys
        otherConfig.remove(triggerKey);
        otherConfig.remove(periodKey);
        otherConfig.remove(intervalKey);
    }
}
