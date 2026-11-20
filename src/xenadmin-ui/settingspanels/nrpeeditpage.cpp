/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "nrpeeditpage.h"
#include "ui_nrpeeditpage.h"
#include "xenlib/xen/actions/nrpe/nrpeactions.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/role.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/xenobject.h"
#include <QHeaderView>
#include <QMovie>
#include <QPointer>
#include <QRegularExpression>
#include <QToolTip>

namespace
{
    const double THRESHOLD_MINIMUM = 0.01;
    const double THRESHOLD_MAXIMUM = 100;

    bool parseNumber(const QString& text, double* value)
    {
        bool ok = false;
        *value = text.trimmed().toDouble(&ok);
        return ok;
    }

    //! C# CheckGroup.CheckEachValue (and the Dom0 load override)
    QString checkEachValue(NRPEEditPage::CheckKind kind, const QString& text)
    {
        if (kind == NRPEEditPage::CheckKind::Dom0Load)
        {
            const QStringList loads = text.split(',');
            double unused;
            if (loads.size() != 3 || !parseNumber(loads[0], &unused) || !parseNumber(loads[1], &unused) || !parseNumber(loads[2], &unused))
                return QObject::tr("Threshold value should consist of 3 comma separated numbers.");
            return QString();
        }

        const QString threshold = text.trimmed();
        if (threshold.isEmpty())
            return QObject::tr("Threshold value should not be empty.");
        double value;
        if (!parseNumber(threshold, &value))
            return QObject::tr("Threshold value should be a number.");
        if (value < THRESHOLD_MINIMUM || value > THRESHOLD_MAXIMUM)
            return QObject::tr("Threshold value should range from %1 to %2.").arg(THRESHOLD_MINIMUM).arg(THRESHOLD_MAXIMUM);
        return QString();
    }
}

NRPEEditPage::NRPEEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::NRPEEditPage)
{
    this->ui->setupUi(this);

    this->addCheck("check_host_load", tr("Host CPU Load"), CheckKind::Usage);
    this->addCheck("check_host_cpu", tr("Host CPU Usage (%)"), CheckKind::Usage);
    this->addCheck("check_host_memory", tr("Host Memory Usage (%)"), CheckKind::Usage);
    this->addCheck("check_vgpu", tr("Host vGPU Usage (%)"), CheckKind::Usage);
    this->addCheck("check_vgpu_memory", tr("Host vGPU Memory Usage (%)"), CheckKind::Usage);
    this->addCheck("check_load", tr("Dom0 CPU Load"), CheckKind::Dom0Load);
    this->addCheck("check_cpu", tr("Dom0 CPU Usage (%)"), CheckKind::Usage);
    this->addCheck("check_memory", tr("Dom0 Memory Usage (%)"), CheckKind::Usage);
    this->addCheck("check_swap", tr("Dom0 Free Swap (%)"), CheckKind::Free);
    this->addCheck("check_disk_root", tr("Dom0 Root Partition Free Space (%)"), CheckKind::Free);
    this->addCheck("check_disk_log", tr("Dom0 Log Partition Free Space (%)"), CheckKind::Free);

    QHeaderView* header = this->ui->CheckDataGridView->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    connect(this->ui->EnableNRPECheckBox, &QCheckBox::toggled, this, &NRPEEditPage::updateEnabledState);
    connect(this->ui->CheckDataGridView, &QTableWidget::itemChanged, this, &NRPEEditPage::validateChecks);
    connect(this->ui->AllowHostsTextBox, &QLineEdit::textEdited, this, [this]() { QToolTip::hideText(); });

    this->ui->EnableNRPECheckBox->setEnabled(false);
    this->ui->GeneralConfigureGroupBox->setEnabled(false);
    this->ui->CheckDataGridView->setEnabled(false);
}

NRPEEditPage::~NRPEEditPage()
{
    delete this->ui;
}

void NRPEEditPage::addCheck(const QString& name, const QString& label, CheckKind kind)
{
    QTableWidget* table = this->ui->CheckDataGridView;
    const int row = table->rowCount();
    table->insertRow(row);

    auto* metric = new QTableWidgetItem(label);
    metric->setFlags(Qt::ItemIsEnabled);
    metric->setToolTip(tr("NRPE check name: %1").arg(name));
    auto* warning = new QTableWidgetItem();
    auto* critical = new QTableWidgetItem();
    table->setItem(row, 0, metric);
    table->setItem(row, 1, warning);
    table->setItem(row, 2, critical);

    this->m_checks.append(CheckRow{name, kind, warning, critical});
}

QString NRPEEditPage::GetText() const
{
    return tr("NRPE");
}

QString NRPEEditPage::GetSubText() const
{
    return tr("NRPE Configuration");
}

QIcon NRPEEditPage::GetImage() const
{
    return QIcon(":/icons/module_16.png");
}

void NRPEEditPage::SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);
    this->m_object = object;
    if (!object)
        return;

    const bool isHost = object->GetObjectType() == XenObjectType::Host;
    this->ui->DescLabelHost->setVisible(isHost);
    this->ui->DescLabelPool->setVisible(!isHost);
    this->setRetrieveStatus(RetrieveStatus::Retrieving);

    auto* action = new NRPERetrieveAction(object, true);
    QPointer<NRPEEditPage> self(this);
    connect(action, &AsyncOperation::completed, this, [self, action]()
    {
        action->deleteLater();
        if (!self)
            return;

        if (action->Succeeded())
        {
            self->m_original = action->GetConfiguration();
            self->m_retrieved = true;
            self->ui->EnableNRPECheckBox->setEnabled(true);
            self->applyConfiguration();
            self->updateEnabledState();
            self->setRetrieveStatus(RetrieveStatus::Successful);
        } else
        {
            self->setRetrieveStatus(RetrieveStatus::Failed);
        }
    });
    action->RunAsync();
}

void NRPEEditPage::setRetrieveStatus(RetrieveStatus status)
{
    QLabel* picture = this->ui->RetrieveNRPEPicture;
    if (QMovie* movie = picture->movie())
    {
        picture->setMovie(nullptr);
        delete movie;
    }

    switch (status)
    {
        case RetrieveStatus::Retrieving:
        {
            auto* movie = new QMovie(":/icons/ajax_loader.gif", QByteArray(), picture);
            picture->setMovie(movie);
            movie->start();
            picture->setVisible(true);
            this->ui->RetrieveNRPELabel->setText(tr("Retrieving NRPE configuration..."));
            break;
        }
        case RetrieveStatus::Successful:
            picture->setVisible(false);
            this->ui->RetrieveNRPELabel->clear();
            break;
        case RetrieveStatus::Failed:
            picture->setPixmap(QPixmap(":/icons/error_16.png"));
            picture->setVisible(true);
            this->ui->RetrieveNRPELabel->setText(tr("Failed to retrieve NRPE configuration, please check the application logs."));
            break;
    }
}

void NRPEEditPage::applyConfiguration()
{
    this->ui->EnableNRPECheckBox->setChecked(this->m_original.enableNRPE);
    this->ui->AllowHostsTextBox->setText(this->m_original.allowHosts);
    this->ui->DebugLogCheckBox->setChecked(this->m_original.debug);
    this->ui->SslDebugLogCheckBox->setChecked(this->m_original.sslLogging);

    QSignalBlocker blocker(this->ui->CheckDataGridView);
    for (const CheckRow& row : this->m_checks)
    {
        auto it = this->m_original.checks.constFind(row.name);
        if (it == this->m_original.checks.constEnd())
            continue;
        row.warning->setText(it->warningThreshold);
        row.critical->setText(it->criticalThreshold);
    }
}

void NRPEEditPage::updateEnabledState()
{
    // C#: the settings are editable only while NRPE is enabled, and only then are errors shown
    const bool enabled = this->m_retrieved && this->ui->EnableNRPECheckBox->isChecked();
    this->ui->GeneralConfigureGroupBox->setEnabled(enabled);
    this->ui->CheckDataGridView->setEnabled(enabled);
    this->validateChecks();
}

bool NRPEEditPage::CheckThresholds(CheckKind kind, const QString& warning, const QString& critical,
                                   QString* warningError, QString* criticalError)
{
    warningError->clear();
    criticalError->clear();

    *warningError = checkEachValue(kind, warning);
    if (!warningError->isEmpty())
        return false;
    *criticalError = checkEachValue(kind, critical);
    if (!criticalError->isEmpty())
        return false;

    if (kind == CheckKind::Dom0Load)
    {
        const QStringList warnings = warning.split(',');
        const QStringList criticals = critical.split(',');
        for (int i = 0; i < 3; ++i)
        {
            if (warnings[i].trimmed().toDouble() > criticals[i].trimmed().toDouble())
            {
                *warningError = tr("Warning threshold should be less than critical threshold.");
                return false;
            }
        }
        return true;
    }

    const double w = warning.trimmed().toDouble();
    const double c = critical.trimmed().toDouble();
    if (kind == CheckKind::Free && !(w > c))
    {
        *warningError = tr("Warning threshold should be greater than critical threshold.");
        return false;
    }
    if (kind == CheckKind::Usage && !(w < c))
    {
        *warningError = tr("Warning threshold should be less than critical threshold.");
        return false;
    }
    return true;
}

void NRPEEditPage::validateChecks()
{
    // C# shows cell errors (an icon with the message as its tooltip) only while NRPE is enabled
    const bool showErrors = this->m_retrieved && this->ui->EnableNRPECheckBox->isChecked();
    QSignalBlocker blocker(this->ui->CheckDataGridView);
    for (const CheckRow& row : this->m_checks)
    {
        QString warningError, criticalError;
        CheckThresholds(row.kind, row.warning->text(), row.critical->text(), &warningError, &criticalError);
        if (!showErrors)
            warningError.clear(), criticalError.clear();

        for (auto [item, error] : {std::make_pair(row.warning, warningError), std::make_pair(row.critical, criticalError)})
        {
            item->setIcon(error.isEmpty() ? QIcon() : QIcon(":/icons/error_16.png"));
            item->setToolTip(error);
        }
    }
}

bool NRPEEditPage::checksValid() const
{
    for (const CheckRow& row : this->m_checks)
    {
        QString warningError, criticalError;
        if (!CheckThresholds(row.kind, row.warning->text(), row.critical->text(), &warningError, &criticalError))
            return false;
    }
    return true;
}

QString NRPEEditPage::AllowHostsError(const QString& allowHosts)
{
    static const QRegularExpression ipv4("^((25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\.){3}(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)$");
    static const QRegularExpression ipv4Cidr("^([0-9]{1,3}\\.){3}[0-9]{1,3}(\\/([0-9]|[1-2][0-9]|3[0-2]))?$");
    static const QRegularExpression domain("^(((?!-))(xn--|_)?[a-z0-9-]{0,61}[a-z0-9]{1,1}\\.)*(xn--)?([a-z0-9][a-z0-9\\-]{0,60}|[a-z0-9-]{1,30}\\.[a-z]{2,})$");

    if (allowHosts.trimmed().isEmpty())
        return tr("Monitoring servers should not be empty.");

    const QStringList hosts = allowHosts.split(',');
    for (int i = 0; i < hosts.size(); ++i)
    {
        const QString host = hosts[i].trimmed();
        if (!ipv4.match(host).hasMatch() && !ipv4Cidr.match(host).hasMatch() && !domain.match(host).hasMatch())
            return tr("Monitoring servers should be comma separated IP address or domain list, e.g. 192.168.1.1, test.domain.com");
        for (int j = 0; j < i; ++j)
        {
            if (hosts[j].trimmed() == host)
                return tr("Please remove duplicate addresses");
        }
    }
    return QString();
}

bool NRPEEditPage::IsAvailable(XenConnection* connection)
{
    if (!connection || !Host::XapiEqualOrGreater(connection, "23.27.0"))
        return false;

    XenAPI::Session* session = connection->GetSession();
    if (!session)
        return false;
    if (session->IsLocalSuperuser())
        return true;

    for (const QSharedPointer<Role>& role : Role::SessionRoles(connection))
    {
        if (role && role->GetName() == "pool-admin")
            return true;
    }
    return false;
}

bool NRPEEditPage::IsValidToSave() const
{
    this->m_validationMessage.clear();
    if (!this->m_retrieved || !this->ui->EnableNRPECheckBox->isChecked())
        return true;

    this->m_validationMessage = AllowHostsError(this->ui->AllowHostsTextBox->text());
    return this->m_validationMessage.isEmpty() && this->checksValid();
}

void NRPEEditPage::ShowLocalValidationMessages()
{
    QToolTip::hideText();
    if (!this->m_validationMessage.isEmpty())
    {
        QLineEdit* field = this->ui->AllowHostsTextBox;
        QToolTip::showText(field->mapToGlobal(QPoint(0, field->height())),
                           tr("Monitoring servers format is not correct") + "\n" + this->m_validationMessage, field);
        return;
    }

    // C# focuses the first metric with an error
    for (const CheckRow& row : this->m_checks)
    {
        QString warningError, criticalError;
        if (!CheckThresholds(row.kind, row.warning->text(), row.critical->text(), &warningError, &criticalError))
        {
            QTableWidgetItem* item = warningError.isEmpty() ? row.critical : row.warning;
            this->ui->CheckDataGridView->setCurrentItem(item);
            this->ui->CheckDataGridView->scrollToItem(item);
            break;
        }
    }
}

void NRPEEditPage::HideLocalValidationMessages()
{
    QToolTip::hideText();
}

void NRPEEditPage::Cleanup()
{
}

NRPEHostConfiguration NRPEEditPage::currentConfiguration() const
{
    NRPEHostConfiguration config;
    config.enableNRPE = this->ui->EnableNRPECheckBox->isChecked();
    config.allowHosts = this->ui->AllowHostsTextBox->text().remove(' ');
    config.debug = this->ui->DebugLogCheckBox->isChecked();
    config.sslLogging = this->ui->SslDebugLogCheckBox->isChecked();
    for (const CheckRow& row : this->m_checks)
        config.checks.insert(row.name, NRPEHostConfiguration::Check{row.warning->text().trimmed(), row.critical->text().trimmed()});
    return config;
}

bool NRPEEditPage::HasChanged() const
{
    return this->m_retrieved && this->currentConfiguration() != this->m_original;
}

AsyncOperation* NRPEEditPage::SaveSettings()
{
    if (!this->m_object || !this->HasChanged())
        return nullptr;
    return new NRPEUpdateAction(this->m_object, this->currentConfiguration(), this->m_original, true);
}
