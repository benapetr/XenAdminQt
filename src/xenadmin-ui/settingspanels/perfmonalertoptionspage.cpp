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

#include "perfmonalertoptionspage.h"
#include "ui_perfmonalertoptionspage.h"
#include "xenlib/xen/actions/general/perfmonoptionsdefinitionaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xencache.h"
#include <QRegularExpression>
#include <QToolTip>

namespace
{
    // C# BrandManager.PerfAlertMailDefaultLanguage (XCP-ng branding: en-US)
    const char* const DEFAULT_MAIL_LANGUAGE = "en-US";
}

PerfmonAlertOptionsPage::PerfmonAlertOptionsPage(QWidget* parent) : IEditPage(parent), ui(new Ui::PerfmonAlertOptionsPage)
{
    this->ui->setupUi(this);

    // C# languages; the item data is the code stored in other_config
    this->ui->languageComboBox->addItem(tr("English (United States)"), QStringLiteral("en-US"));
    this->ui->languageComboBox->addItem(tr("Chinese (Simplified)"), QStringLiteral("zh-CN"));
    this->ui->languageComboBox->addItem(tr("Japanese (Japan)"), QStringLiteral("ja-JP"));

    connect(this->ui->enableCheckBox, &QCheckBox::toggled, this, &PerfmonAlertOptionsPage::updateEnablement);
    connect(this->ui->enableCheckBox, &QCheckBox::toggled, this, &IEditPage::populated);
    connect(this->ui->emailAddressLineEdit, &QLineEdit::textChanged, this, &IEditPage::populated);
    this->updateEnablement();
}

PerfmonAlertOptionsPage::~PerfmonAlertOptionsPage()
{
    delete this->ui;
}

QString PerfmonAlertOptionsPage::GetText() const
{
    return tr("Email Notifications");
}

QString PerfmonAlertOptionsPage::GetSubText() const
{
    return this->ui->enableCheckBox->isChecked() ? this->ui->emailAddressLineEdit->text() : tr("None defined");
}

QIcon PerfmonAlertOptionsPage::GetImage() const
{
    return QIcon(":/icons/email_16.png");
}

bool PerfmonAlertOptionsPage::IsValidEmail(const QString& text)
{
    static const QRegularExpression emailRegex(QStringLiteral("\\w+([-+.]\\w+)*@\\w+([-.]\\w+)*\\.\\w+([-.]\\w+)*"),
                                               QRegularExpression::CaseInsensitiveOption);
    return emailRegex.match(text).hasMatch();
}

bool PerfmonAlertOptionsPage::IsValidSmtpAddress(const QString& text)
{
    for (const QChar c : text)
    {
        if (c.unicode() >= 128)
            return false;
    }
    return !text.trimmed().isEmpty();
}

bool PerfmonAlertOptionsPage::IsValidPort(const QString& text)
{
    bool ok = false;
    const int port = text.toInt(&ok);
    return ok && port > 0 && port <= 65535;
}

void PerfmonAlertOptionsPage::SetXenObject(QSharedPointer<XenObject> object,
                                           const QVariantMap& objectDataBefore,
                                           const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);
    this->m_object = object;

    this->populate();

    // Originals for change detection
    this->m_origEnabled = this->ui->enableCheckBox->isChecked();
    this->m_origEmailAddress = this->ui->emailAddressLineEdit->text();
    this->m_origSmtpServer = this->ui->smtpServerLineEdit->text();
    this->m_origSmtpPort = this->ui->smtpPortLineEdit->text();
    this->m_origLanguageIndex = this->ui->languageComboBox->currentIndex();
}

// C# Populate: settings live in the pool's other_config, also for a standalone server
void PerfmonAlertOptionsPage::populate()
{
    XenConnection* connection = this->m_object ? this->m_object->GetConnection() : nullptr;
    QSharedPointer<Pool> pool = connection && connection->GetCache() ? connection->GetCache()->GetPoolOfOne() : QSharedPointer<Pool>();
    if (!pool)
        return;

    const QVariantMap otherConfig = pool->GetOtherConfig();
    const QString mailDestination = otherConfig.value(Pool::MAIL_DESTINATION_KEY_NAME).toString();
    const QString mailHub = otherConfig.value(Pool::SMTP_MAILHUB_KEY_NAME).toString();
    const QString mailLanguage = otherConfig.value(Pool::MAIL_LANGUAGE_KEY_NAME).toString();

    this->ui->enableCheckBox->setChecked(!mailDestination.trimmed().isEmpty() && !mailHub.trimmed().isEmpty());

    if (!mailDestination.trimmed().isEmpty())
        this->ui->emailAddressLineEdit->setText(mailDestination.trimmed());

    if (!mailHub.trimmed().isEmpty())
    {
        const QStringList words = mailHub.trimmed().split(QLatin1Char(':'));
        if (!words.isEmpty())
            this->ui->smtpServerLineEdit->setText(words.at(0));
        if (words.size() > 1)
            this->ui->smtpPortLineEdit->setText(words.at(1));
    }

    const bool languageSupported = Host::InvernessOrGreater(connection);
    this->ui->languageLabel->setVisible(languageSupported);
    this->ui->languageComboBox->setVisible(languageSupported);

    int index = -1;
    if (languageSupported && !mailLanguage.trimmed().isEmpty())
        index = this->indexOfLanguage(mailLanguage.trimmed());
    if (index == -1)
        index = this->indexOfLanguage(QString::fromLatin1(DEFAULT_MAIL_LANGUAGE));
    if (index == -1)
        index = 0;
    this->ui->languageComboBox->setCurrentIndex(index);

    this->updateEnablement();
}

int PerfmonAlertOptionsPage::indexOfLanguage(const QString& code) const
{
    for (int i = 0; i < this->ui->languageComboBox->count(); ++i)
    {
        if (this->ui->languageComboBox->itemData(i).toString().compare(code, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

void PerfmonAlertOptionsPage::updateEnablement()
{
    // C# designer: the delivery fields follow the checkbox
    const bool enabled = this->ui->enableCheckBox->isChecked();
    this->ui->emailAddressLineEdit->setEnabled(enabled);
    this->ui->smtpServerLineEdit->setEnabled(enabled);
    this->ui->smtpPortLineEdit->setEnabled(enabled);
    this->ui->languageComboBox->setEnabled(enabled);
}

bool PerfmonAlertOptionsPage::HasChanged() const
{
    return this->m_origEnabled != this->ui->enableCheckBox->isChecked()
        || this->m_origEmailAddress != this->ui->emailAddressLineEdit->text()
        || this->m_origSmtpServer != this->ui->smtpServerLineEdit->text()
        || this->m_origSmtpPort != this->ui->smtpPortLineEdit->text()
        || this->m_origLanguageIndex != this->ui->languageComboBox->currentIndex();
}

bool PerfmonAlertOptionsPage::IsValidToSave() const
{
    return !this->ui->enableCheckBox->isChecked()
        || (IsValidEmail(this->ui->emailAddressLineEdit->text())
            && IsValidPort(this->ui->smtpPortLineEdit->text())
            && IsValidSmtpAddress(this->ui->smtpServerLineEdit->text()));
}

void PerfmonAlertOptionsPage::ShowLocalValidationMessages()
{
    QLineEdit* invalid = nullptr;
    if (!IsValidEmail(this->ui->emailAddressLineEdit->text()))
        invalid = this->ui->emailAddressLineEdit;
    else if (!IsValidSmtpAddress(this->ui->smtpServerLineEdit->text()))
        invalid = this->ui->smtpServerLineEdit;
    else if (!IsValidPort(this->ui->smtpPortLineEdit->text()))
        invalid = this->ui->smtpPortLineEdit;

    if (invalid)
        QToolTip::showText(invalid->mapToGlobal(QPoint(0, invalid->height())), tr("Invalid parameter"), invalid);
}

void PerfmonAlertOptionsPage::HideLocalValidationMessages()
{
    QToolTip::hideText();
}

void PerfmonAlertOptionsPage::Cleanup()
{
}

QVariantMap PerfmonAlertOptionsPage::GetModifiedObjectData() const
{
    // The settings are saved on the pool by the action, not through the edited object
    return QVariantMap();
}

AsyncOperation* PerfmonAlertOptionsPage::SaveSettings()
{
    XenConnection* connection = this->m_object ? this->m_object->GetConnection() : nullptr;
    if (!connection)
        return nullptr;

    // Empty values clear the settings
    QString mailDestination;
    QString mailHub;
    QString mailLanguageCode;
    if (this->ui->enableCheckBox->isChecked())
    {
        mailDestination = this->ui->emailAddressLineEdit->text();
        mailHub = this->ui->smtpServerLineEdit->text() + QLatin1Char(':') + this->ui->smtpPortLineEdit->text();
        if (this->ui->languageComboBox->isVisible())
            mailLanguageCode = this->ui->languageComboBox->currentData().toString();
    }

    return new PerfmonOptionsDefinitionAction(connection, mailDestination, mailHub, mailLanguageCode, true);
}
