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

#ifndef LOGDESTINATIONEDITPAGE_H
#define LOGDESTINATIONEDITPAGE_H

#include "ieditpage.h"
#include <QWidget>
#include <QEvent>
#include <QRegularExpression>
#include <QToolTip>

namespace Ui
{
    class LogDestinationEditPage;
}

class XenConnection;
class AsyncOperation;

/**
 * @brief Log Destination Settings Edit Page
 *
 * Allows configuring remote syslog server for host logging.
 * Modifies host.logging["syslog_destination"] and calls Host.syslog_reconfigure.
 *
 * C# equivalent: XenAdmin.SettingsPanels.LogDestinationEditPage
 */
class LogDestinationEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit LogDestinationEditPage(QWidget* parent = nullptr);
        ~LogDestinationEditPage() override;

        // IVerticalTab interface
        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;

        // IEditPage interface
        void SetXenObjects(const QString& objectRef,
                           const QString& objectType,
                           const QVariantMap& objectDataBefore,
                           const QVariantMap& objectDataCopy) override;
        AsyncOperation* SaveSettings() override;
        bool IsValidToSave() const override;
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;
        void Cleanup() override;
        bool HasChanged() const override;
        QVariantMap GetModifiedObjectData() const override;

    protected:
        bool eventFilter(QObject* obj, QEvent* event) override;

    private slots:
        void onCheckBoxRemoteToggled(bool checked);
        void onServerTextChanged(const QString& text);
        void onServerEditFocused();

    private:
        void repopulate();
        void revalidate();
        QString remoteServer() const;

        Ui::LogDestinationEditPage* ui;
        QString m_hostRef;
        QVariantMap m_objectDataCopy; // Need this for modification
        QString m_originalLocation;
        bool m_validToSave;
        QRegularExpression m_hostnameRegex;
};

#endif // LOGDESTINATIONEDITPAGE_H
