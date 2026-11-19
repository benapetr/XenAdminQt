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
#include "xenlib/xen/actions/general/perfmonoptionsdefinitionaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/xenobject.h"
#include "xenlib/xencache.h"
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QToolTip>
#include <QVBoxLayout>

namespace
{
    // C# BrandManager.PerfAlertMailDefaultLanguage (XCP-ng branding: en-US)
    const char* const DEFAULT_MAIL_LANGUAGE = "en-US";
}

PerfmonAlertOptionsPage::PerfmonAlertOptionsPage(QWidget* parent) : IEditPage(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* blurb = new QLabel(tr("If you want to be notified via email when alerts are generated for your system's "
                                  "resources, enter an address where email notifications will be sent."), this);
    blurb->setWordWrap(true);
    layout->addWidget(blurb);

    this->m_enableCheckBox = new QCheckBox(tr("Send email alert &notifications"), this);
    layout->addWidget(this->m_enableCheckBox);

    QGroupBox* group = new QGroupBox(tr("Delivery address"), this);
    QGridLayout* grid = new QGridLayout(group);

    QLabel* emailLabel = new QLabel(tr("&Email address:"), group);
    this->m_emailAddress = new QLineEdit(group);
    emailLabel->setBuddy(this->m_emailAddress);
    grid->addWidget(emailLabel, 0, 0);
    grid->addWidget(this->m_emailAddress, 0, 1, 1, 3);

    QLabel* serverLabel = new QLabel(tr("&SMTP Server:"), group);
    this->m_smtpServer = new QLineEdit(group);
    serverLabel->setBuddy(this->m_smtpServer);
    QLabel* portLabel = new QLabel(tr("&Port:"), group);
    this->m_smtpPort = new QLineEdit(QStringLiteral("25"), group);
    this->m_smtpPort->setMaximumWidth(70);
    portLabel->setBuddy(this->m_smtpPort);
    grid->addWidget(serverLabel, 1, 0);
    grid->addWidget(this->m_smtpServer, 1, 1);
    grid->addWidget(portLabel, 1, 2);
    grid->addWidget(this->m_smtpPort, 1, 3);

    this->m_languageLabel = new QLabel(tr("&Mail language:"), group);
    this->m_language = new QComboBox(group);
    this->m_languageLabel->setBuddy(this->m_language);
    this->m_language->addItem(tr("English (United States)"), QStringLiteral("en-US"));
    this->m_language->addItem(tr("Chinese (Simplified)"), QStringLiteral("zh-CN"));
    this->m_language->addItem(tr("Japanese (Japan)"), QStringLiteral("ja-JP"));
    grid->addWidget(this->m_languageLabel, 2, 0);
    grid->addWidget(this->m_language, 2, 1);
    grid->setColumnStretch(1, 1);

    layout->addWidget(group);
    layout->addStretch();

    connect(this->m_enableCheckBox, &QCheckBox::toggled, this, &PerfmonAlertOptionsPage::updateEnablement);
    connect(this->m_enableCheckBox, &QCheckBox::toggled, this, &IEditPage::populated);
    connect(this->m_emailAddress, &QLineEdit::textChanged, this, &IEditPage::populated);
    this->updateEnablement();
}

QString PerfmonAlertOptionsPage::GetText() const
{
    return tr("Email Notifications");
}

QString PerfmonAlertOptionsPage::GetSubText() const
{
    return this->m_enableCheckBox->isChecked() ? this->m_emailAddress->text() : tr("None defined");
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
    this->m_origEnabled = this->m_enableCheckBox->isChecked();
    this->m_origEmailAddress = this->m_emailAddress->text();
    this->m_origSmtpServer = this->m_smtpServer->text();
    this->m_origSmtpPort = this->m_smtpPort->text();
    this->m_origLanguageIndex = this->m_language->currentIndex();
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

    this->m_enableCheckBox->setChecked(!mailDestination.trimmed().isEmpty() && !mailHub.trimmed().isEmpty());

    if (!mailDestination.trimmed().isEmpty())
        this->m_emailAddress->setText(mailDestination.trimmed());

    if (!mailHub.trimmed().isEmpty())
    {
        const QStringList words = mailHub.trimmed().split(QLatin1Char(':'));
        if (!words.isEmpty())
            this->m_smtpServer->setText(words.at(0));
        if (words.size() > 1)
            this->m_smtpPort->setText(words.at(1));
    }

    const bool languageSupported = Host::InvernessOrGreater(connection);
    this->m_languageLabel->setVisible(languageSupported);
    this->m_language->setVisible(languageSupported);

    int index = -1;
    if (languageSupported && !mailLanguage.trimmed().isEmpty())
        index = this->indexOfLanguage(mailLanguage.trimmed());
    if (index == -1)
        index = this->indexOfLanguage(QString::fromLatin1(DEFAULT_MAIL_LANGUAGE));
    if (index == -1)
        index = 0;
    this->m_language->setCurrentIndex(index);

    this->updateEnablement();
}

int PerfmonAlertOptionsPage::indexOfLanguage(const QString& code) const
{
    for (int i = 0; i < this->m_language->count(); ++i)
    {
        if (this->m_language->itemData(i).toString().compare(code, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

void PerfmonAlertOptionsPage::updateEnablement()
{
    // C# designer: the delivery fields follow the checkbox
    const bool enabled = this->m_enableCheckBox->isChecked();
    this->m_emailAddress->setEnabled(enabled);
    this->m_smtpServer->setEnabled(enabled);
    this->m_smtpPort->setEnabled(enabled);
    this->m_language->setEnabled(enabled);
}

bool PerfmonAlertOptionsPage::HasChanged() const
{
    return this->m_origEnabled != this->m_enableCheckBox->isChecked()
        || this->m_origEmailAddress != this->m_emailAddress->text()
        || this->m_origSmtpServer != this->m_smtpServer->text()
        || this->m_origSmtpPort != this->m_smtpPort->text()
        || this->m_origLanguageIndex != this->m_language->currentIndex();
}

bool PerfmonAlertOptionsPage::IsValidToSave() const
{
    return !this->m_enableCheckBox->isChecked()
        || (IsValidEmail(this->m_emailAddress->text())
            && IsValidPort(this->m_smtpPort->text())
            && IsValidSmtpAddress(this->m_smtpServer->text()));
}

void PerfmonAlertOptionsPage::ShowLocalValidationMessages()
{
    QLineEdit* invalid = nullptr;
    if (!IsValidEmail(this->m_emailAddress->text()))
        invalid = this->m_emailAddress;
    else if (!IsValidSmtpAddress(this->m_smtpServer->text()))
        invalid = this->m_smtpServer;
    else if (!IsValidPort(this->m_smtpPort->text()))
        invalid = this->m_smtpPort;

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
    if (this->m_enableCheckBox->isChecked())
    {
        mailDestination = this->m_emailAddress->text();
        mailHub = this->m_smtpServer->text() + QLatin1Char(':') + this->m_smtpPort->text();
        if (this->m_language->isVisible())
            mailLanguageCode = this->m_language->currentData().toString();
    }

    return new PerfmonOptionsDefinitionAction(connection, mailDestination, mailHub, mailLanguageCode, true);
}
