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
#include "xenlib/xen/asyncoperation.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/api.h"
#include <QDomDocument>
#include <QSet>
#include <QXmlStreamWriter>
#include <QtMath>

namespace
{
    static const QString PERFMON_KEY = QStringLiteral("perfmon");
    static const QString PERFMON_CPU = QStringLiteral("cpu_usage");
    static const QString PERFMON_NETWORK = QStringLiteral("network_usage");
    static const QString PERFMON_DISK = QStringLiteral("disk_usage");
    static const QString PERFMON_MEMORY_FREE = QStringLiteral("memory_free_kib");
    static const QString PERFMON_DOM0_MEMORY = QStringLiteral("mem_usage");
    static const QString PERFMON_SR = QStringLiteral("sr_io_throughput_total_per_host");

    bool almostEqual(double a, double b)
    {
        return qAbs(a - b) < 0.0001;
    }

    int toMinutes(int seconds)
    {
        return seconds > 0 ? qMax(1, seconds / 60) : 5;
    }

    int toSeconds(int minutes)
    {
        return qMax(1, minutes) * 60;
    }
}

PerfmonAlertEditPage::PerfmonAlertEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::PerfmonAlertEditPage)
{
    this->ui->setupUi(this);
}

PerfmonAlertEditPage::~PerfmonAlertEditPage()
{
    delete this->ui;
}

QString PerfmonAlertEditPage::GetText() const
{
    return tr("Performance Alerts");
}

QString PerfmonAlertEditPage::GetSubText() const
{
    QStringList subs;

    if (this->ui->groupBoxCPU->isVisible() && this->ui->groupBoxCPU->isChecked())
        subs << tr("CPU");
    if (this->ui->groupBoxNetwork->isVisible() && this->ui->groupBoxNetwork->isChecked())
        subs << tr("Network");
    if (this->ui->groupBoxMemory->isVisible() && this->ui->groupBoxMemory->isChecked())
        subs << tr("Memory");
    if (this->ui->groupBoxDom0Memory->isVisible() && this->ui->groupBoxDom0Memory->isChecked())
        subs << tr("Dom0 Memory");
    if (this->ui->groupBoxDisk->isVisible() && this->ui->groupBoxDisk->isChecked())
        subs << tr("Disk");
    if (this->ui->groupBoxSr->isVisible() && this->ui->groupBoxSr->isChecked())
        subs << tr("Storage Throughput");

    return subs.isEmpty() ? tr("None") : subs.join(", ");
}

QIcon PerfmonAlertEditPage::GetImage() const
{
    return QIcon(":/icons/alert_16.png");
}

void PerfmonAlertEditPage::SetXenObjects(const QString& objectRef,
                                         const QString& objectType,
                                         const QVariantMap& objectDataBefore,
                                         const QVariantMap& objectDataCopy)
{
    this->m_objectRef = objectRef;
    this->m_objectType = objectType.toLower();
    this->m_objectDataBefore = objectDataBefore;
    this->m_objectDataCopy = objectDataCopy;

    this->configureVisibilityByObjectType();

    const QVariantMap otherConfig = objectDataBefore.value("other_config").toMap();
    const QString perfmonXml = otherConfig.value(PERFMON_KEY).toString();
    const QMap<QString, AlertConfig> defs = this->parsePerfmonDefinitions(perfmonXml);

    this->m_origCPUAlert = this->getAlert(defs, PERFMON_CPU);
    this->m_origNetworkAlert = this->getAlert(defs, PERFMON_NETWORK);
    this->m_origDiskAlert = this->getAlert(defs, PERFMON_DISK);
    this->m_origSrAlert = this->getAlert(defs, PERFMON_SR);
    this->m_origMemoryAlert = this->getAlert(defs, PERFMON_MEMORY_FREE);
    this->m_origDom0Alert = this->getAlert(defs, PERFMON_DOM0_MEMORY);

    this->setCpuAlertToUi(this->m_origCPUAlert);
    this->setNetworkAlertToUi(this->m_origNetworkAlert);
    this->setDiskAlertToUi(this->m_origDiskAlert);
    this->setSrAlertToUi(this->m_origSrAlert);
    this->setMemoryAlertToUi(this->m_origMemoryAlert);
    this->setDom0AlertToUi(this->m_origDom0Alert);
}

AsyncOperation* PerfmonAlertEditPage::SaveSettings()
{
    if (!this->HasChanged())
        return nullptr;

    QVariantMap otherConfig = this->m_objectDataCopy.value("other_config").toMap();
    QMap<QString, AlertConfig> defs = this->parsePerfmonDefinitions(otherConfig.value(PERFMON_KEY).toString());

    const QSet<QString> managed = {PERFMON_CPU, PERFMON_NETWORK, PERFMON_DISK, PERFMON_SR, PERFMON_MEMORY_FREE, PERFMON_DOM0_MEMORY};
    for (const QString& key : managed)
        defs.remove(key);

    if (this->ui->groupBoxCPU->isVisible())
        this->setAlert(defs, PERFMON_CPU, this->readCpuAlertFromUi());
    if (this->ui->groupBoxNetwork->isVisible())
        this->setAlert(defs, PERFMON_NETWORK, this->readNetworkAlertFromUi());
    if (this->ui->groupBoxDisk->isVisible())
        this->setAlert(defs, PERFMON_DISK, this->readDiskAlertFromUi());
    if (this->ui->groupBoxSr->isVisible())
        this->setAlert(defs, PERFMON_SR, this->readSrAlertFromUi());
    if (this->ui->groupBoxMemory->isVisible())
        this->setAlert(defs, PERFMON_MEMORY_FREE, this->readMemoryAlertFromUi());
    if (this->ui->groupBoxDom0Memory->isVisible())
        this->setAlert(defs, PERFMON_DOM0_MEMORY, this->readDom0AlertFromUi());

    const QString perfmonXml = this->buildPerfmonXml(defs);
    if (perfmonXml.isEmpty())
        otherConfig.remove(PERFMON_KEY);
    else
        otherConfig[PERFMON_KEY] = perfmonXml;

    this->m_objectDataCopy["other_config"] = otherConfig;

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
            {
            }

        protected:
            void run() override
            {
                XenRpcAPI api(GetConnection()->GetSession());

                SetPercentComplete(30);

                QString methodName = m_objectType + ".set_other_config";
                QVariantList params;
                params << GetConnection()->GetSessionId() << this->m_objectRef << this->m_otherConfig;
                QByteArray request = api.BuildJsonRpcCall(methodName, params);
                QByteArray response = GetConnection()->SendRequest(request);
                api.ParseJsonRpcResponse(response);

                SetPercentComplete(100);
            }

        private:
            QString m_objectRef;
            QString m_objectType;
            QVariantMap m_otherConfig;
    };

    return new PerfmonAlertOperation(this->m_connection, this->m_objectRef, this->m_objectType, otherConfig, nullptr);
}

bool PerfmonAlertEditPage::IsValidToSave() const
{
    return true;
}

void PerfmonAlertEditPage::ShowLocalValidationMessages()
{
}

void PerfmonAlertEditPage::HideLocalValidationMessages()
{
}

void PerfmonAlertEditPage::Cleanup()
{
}

bool PerfmonAlertEditPage::HasChanged() const
{
    if (this->ui->groupBoxCPU->isVisible())
    {
        const AlertConfig cur = this->readCpuAlertFromUi();
        if (cur.enabled != this->m_origCPUAlert.enabled ||
            !almostEqual(cur.threshold, this->m_origCPUAlert.threshold) ||
            cur.durationSeconds != this->m_origCPUAlert.durationSeconds ||
            cur.intervalSeconds != this->m_origCPUAlert.intervalSeconds)
            return true;
    }

    if (this->ui->groupBoxNetwork->isVisible())
    {
        const AlertConfig cur = this->readNetworkAlertFromUi();
        if (cur.enabled != this->m_origNetworkAlert.enabled ||
            !almostEqual(cur.threshold, this->m_origNetworkAlert.threshold) ||
            cur.durationSeconds != this->m_origNetworkAlert.durationSeconds ||
            cur.intervalSeconds != this->m_origNetworkAlert.intervalSeconds)
            return true;
    }

    if (this->ui->groupBoxDisk->isVisible())
    {
        const AlertConfig cur = this->readDiskAlertFromUi();
        if (cur.enabled != this->m_origDiskAlert.enabled ||
            !almostEqual(cur.threshold, this->m_origDiskAlert.threshold) ||
            cur.durationSeconds != this->m_origDiskAlert.durationSeconds ||
            cur.intervalSeconds != this->m_origDiskAlert.intervalSeconds)
            return true;
    }

    if (this->ui->groupBoxSr->isVisible())
    {
        const AlertConfig cur = this->readSrAlertFromUi();
        if (cur.enabled != this->m_origSrAlert.enabled ||
            !almostEqual(cur.threshold, this->m_origSrAlert.threshold) ||
            cur.durationSeconds != this->m_origSrAlert.durationSeconds ||
            cur.intervalSeconds != this->m_origSrAlert.intervalSeconds)
            return true;
    }

    if (this->ui->groupBoxMemory->isVisible())
    {
        const AlertConfig cur = this->readMemoryAlertFromUi();
        if (cur.enabled != this->m_origMemoryAlert.enabled ||
            !almostEqual(cur.threshold, this->m_origMemoryAlert.threshold) ||
            cur.durationSeconds != this->m_origMemoryAlert.durationSeconds ||
            cur.intervalSeconds != this->m_origMemoryAlert.intervalSeconds)
            return true;
    }

    if (this->ui->groupBoxDom0Memory->isVisible())
    {
        const AlertConfig cur = this->readDom0AlertFromUi();
        if (cur.enabled != this->m_origDom0Alert.enabled ||
            !almostEqual(cur.threshold, this->m_origDom0Alert.threshold) ||
            cur.durationSeconds != this->m_origDom0Alert.durationSeconds ||
            cur.intervalSeconds != this->m_origDom0Alert.intervalSeconds)
            return true;
    }

    return false;
}

QMap<QString, PerfmonAlertEditPage::AlertConfig> PerfmonAlertEditPage::parsePerfmonDefinitions(const QString& perfmonXml) const
{
    QMap<QString, AlertConfig> defs;
    if (perfmonXml.trimmed().isEmpty())
        return defs;

    QDomDocument doc;
    if (!doc.setContent(perfmonXml))
        return defs;

    const QDomElement root = doc.documentElement();
    if (root.tagName() != QStringLiteral("config"))
        return defs;

    for (QDomElement var = root.firstChildElement("variable"); !var.isNull(); var = var.nextSiblingElement("variable"))
    {
        const QDomElement nameEl = var.firstChildElement("name");
        if (nameEl.isNull())
            continue;

        const QString name = nameEl.attribute("value");
        if (name.isEmpty())
            continue;

        AlertConfig cfg;
        cfg.enabled = true;

        const QDomElement levelEl = var.firstChildElement("alarm_trigger_level");
        const QDomElement periodEl = var.firstChildElement("alarm_trigger_period");
        const QDomElement inhibitEl = var.firstChildElement("alarm_auto_inhibit_period");

        cfg.threshold = levelEl.attribute("value").toDouble();
        cfg.durationSeconds = periodEl.attribute("value").toInt();
        cfg.intervalSeconds = inhibitEl.attribute("value").toInt();

        if (cfg.durationSeconds <= 0)
            cfg.durationSeconds = 300;
        if (cfg.intervalSeconds <= 0)
            cfg.intervalSeconds = cfg.durationSeconds;

        defs.insert(name, cfg);
    }

    return defs;
}

QString PerfmonAlertEditPage::buildPerfmonXml(const QMap<QString, AlertConfig>& definitions) const
{
    if (definitions.isEmpty())
        return QString();

    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("config"));

    for (auto it = definitions.cbegin(); it != definitions.cend(); ++it)
    {
        if (!it.value().enabled)
            continue;

        writer.writeStartElement(QStringLiteral("variable"));

        writer.writeStartElement(QStringLiteral("name"));
        writer.writeAttribute(QStringLiteral("value"), it.key());
        writer.writeEndElement();

        writer.writeStartElement(QStringLiteral("alarm_trigger_level"));
        writer.writeAttribute(QStringLiteral("value"), QString::number(it.value().threshold, 'f', 10));
        writer.writeEndElement();

        writer.writeStartElement(QStringLiteral("alarm_trigger_period"));
        writer.writeAttribute(QStringLiteral("value"), QString::number(it.value().durationSeconds));
        writer.writeEndElement();

        writer.writeStartElement(QStringLiteral("alarm_auto_inhibit_period"));
        writer.writeAttribute(QStringLiteral("value"), QString::number(it.value().intervalSeconds));
        writer.writeEndElement();

        writer.writeEndElement();
    }

    writer.writeEndElement();
    writer.writeEndDocument();
    return xml;
}

PerfmonAlertEditPage::AlertConfig PerfmonAlertEditPage::getAlert(const QMap<QString, AlertConfig>& definitions,
                                                                 const QString& perfmonName) const
{
    return definitions.value(perfmonName, AlertConfig());
}

void PerfmonAlertEditPage::setAlert(QMap<QString, AlertConfig>& definitions,
                                    const QString& perfmonName,
                                    const AlertConfig& config) const
{
    if (config.enabled)
        definitions.insert(perfmonName, config);
    else
        definitions.remove(perfmonName);
}

PerfmonAlertEditPage::AlertConfig PerfmonAlertEditPage::readCpuAlertFromUi() const
{
    AlertConfig cfg;
    cfg.enabled = this->ui->groupBoxCPU->isChecked();
    cfg.threshold = this->ui->spinBoxCPUThreshold->value() / 100.0;
    cfg.durationSeconds = toSeconds(this->ui->spinBoxCPUDuration->value());
    cfg.intervalSeconds = toSeconds(this->ui->spinBoxCPUInterval->value());
    return cfg;
}

PerfmonAlertEditPage::AlertConfig PerfmonAlertEditPage::readNetworkAlertFromUi() const
{
    AlertConfig cfg;
    cfg.enabled = this->ui->groupBoxNetwork->isChecked();
    cfg.threshold = this->ui->spinBoxNetworkThreshold->value() * 1024.0;
    cfg.durationSeconds = toSeconds(this->ui->spinBoxNetworkDuration->value());
    cfg.intervalSeconds = toSeconds(this->ui->spinBoxNetworkInterval->value());
    return cfg;
}

PerfmonAlertEditPage::AlertConfig PerfmonAlertEditPage::readMemoryAlertFromUi() const
{
    AlertConfig cfg;
    cfg.enabled = this->ui->groupBoxMemory->isChecked();
    cfg.threshold = this->ui->spinBoxMemoryThreshold->value() * 1024.0;
    cfg.durationSeconds = toSeconds(this->ui->spinBoxMemoryDuration->value());
    cfg.intervalSeconds = toSeconds(this->ui->spinBoxMemoryInterval->value());
    return cfg;
}

PerfmonAlertEditPage::AlertConfig PerfmonAlertEditPage::readDiskAlertFromUi() const
{
    AlertConfig cfg;
    cfg.enabled = this->ui->groupBoxDisk->isChecked();
    cfg.threshold = this->ui->spinBoxDiskThreshold->value() * 1024.0;
    cfg.durationSeconds = toSeconds(this->ui->spinBoxDiskDuration->value());
    cfg.intervalSeconds = toSeconds(this->ui->spinBoxDiskInterval->value());
    return cfg;
}

PerfmonAlertEditPage::AlertConfig PerfmonAlertEditPage::readSrAlertFromUi() const
{
    AlertConfig cfg;
    cfg.enabled = this->ui->groupBoxSr->isChecked();
    cfg.threshold = this->ui->spinBoxSrThreshold->value() / 1024.0;
    cfg.durationSeconds = toSeconds(this->ui->spinBoxSrDuration->value());
    cfg.intervalSeconds = toSeconds(this->ui->spinBoxSrInterval->value());
    return cfg;
}

PerfmonAlertEditPage::AlertConfig PerfmonAlertEditPage::readDom0AlertFromUi() const
{
    AlertConfig cfg;
    cfg.enabled = this->ui->groupBoxDom0Memory->isChecked();
    cfg.threshold = this->ui->spinBoxDom0Threshold->value() / 100.0;
    cfg.durationSeconds = toSeconds(this->ui->spinBoxDom0Duration->value());
    cfg.intervalSeconds = toSeconds(this->ui->spinBoxDom0Interval->value());
    return cfg;
}

void PerfmonAlertEditPage::setCpuAlertToUi(const AlertConfig& config)
{
    this->ui->groupBoxCPU->setChecked(config.enabled);
    this->ui->spinBoxCPUThreshold->setValue(qBound(1, static_cast<int>(qRound(config.threshold * 100.0)), 100));
    this->ui->spinBoxCPUDuration->setValue(toMinutes(config.durationSeconds));
    this->ui->spinBoxCPUInterval->setValue(toMinutes(config.intervalSeconds));
}

void PerfmonAlertEditPage::setNetworkAlertToUi(const AlertConfig& config)
{
    this->ui->groupBoxNetwork->setChecked(config.enabled);
    this->ui->spinBoxNetworkThreshold->setValue(qMax(1, static_cast<int>(qRound(config.threshold / 1024.0))));
    this->ui->spinBoxNetworkDuration->setValue(toMinutes(config.durationSeconds));
    this->ui->spinBoxNetworkInterval->setValue(toMinutes(config.intervalSeconds));
}

void PerfmonAlertEditPage::setMemoryAlertToUi(const AlertConfig& config)
{
    this->ui->groupBoxMemory->setChecked(config.enabled);
    this->ui->spinBoxMemoryThreshold->setValue(qMax(1, static_cast<int>(qRound(config.threshold / 1024.0))));
    this->ui->spinBoxMemoryDuration->setValue(toMinutes(config.durationSeconds));
    this->ui->spinBoxMemoryInterval->setValue(toMinutes(config.intervalSeconds));
}

void PerfmonAlertEditPage::setDiskAlertToUi(const AlertConfig& config)
{
    this->ui->groupBoxDisk->setChecked(config.enabled);
    this->ui->spinBoxDiskThreshold->setValue(qMax(1, static_cast<int>(qRound(config.threshold / 1024.0))));
    this->ui->spinBoxDiskDuration->setValue(toMinutes(config.durationSeconds));
    this->ui->spinBoxDiskInterval->setValue(toMinutes(config.intervalSeconds));
}

void PerfmonAlertEditPage::setSrAlertToUi(const AlertConfig& config)
{
    this->ui->groupBoxSr->setChecked(config.enabled);
    this->ui->spinBoxSrThreshold->setValue(qMax(1, static_cast<int>(qRound(config.threshold * 1024.0))));
    this->ui->spinBoxSrDuration->setValue(toMinutes(config.durationSeconds));
    this->ui->spinBoxSrInterval->setValue(toMinutes(config.intervalSeconds));
}

void PerfmonAlertEditPage::setDom0AlertToUi(const AlertConfig& config)
{
    this->ui->groupBoxDom0Memory->setChecked(config.enabled);
    this->ui->spinBoxDom0Threshold->setValue(qBound(1, static_cast<int>(qRound(config.threshold * 100.0)), 100));
    this->ui->spinBoxDom0Duration->setValue(toMinutes(config.durationSeconds));
    this->ui->spinBoxDom0Interval->setValue(toMinutes(config.intervalSeconds));
}

void PerfmonAlertEditPage::configureVisibilityByObjectType()
{
    const bool isVm = (this->m_objectType == QStringLiteral("vm"));
    const bool isHost = (this->m_objectType == QStringLiteral("host"));
    const bool isSr = (this->m_objectType == QStringLiteral("sr"));

    this->ui->groupBoxCPU->setVisible(isVm || isHost);
    this->ui->groupBoxNetwork->setVisible(isVm || isHost);
    this->ui->groupBoxDisk->setVisible(isVm);
    this->ui->groupBoxSr->setVisible(isSr);
    this->ui->groupBoxMemory->setVisible(isHost);
    this->ui->groupBoxDom0Memory->setVisible(isHost);
}
