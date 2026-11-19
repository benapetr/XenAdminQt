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

#ifndef PERFMONALERTOPTIONSPAGE_H
#define PERFMONALERTOPTIONSPAGE_H

#include "ieditpage.h"

namespace Ui
{
    class PerfmonAlertOptionsPage;
}

/**
 * @brief Email notifications for performance alerts (pool or standalone server)
 *
 * Qt port of C# XenAdmin.SettingsPanels.PerfmonAlertOptionsPage: the delivery address, SMTP
 * server and port, and mail language, stored in the pool's other_config.
 */
class PerfmonAlertOptionsPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit PerfmonAlertOptionsPage(QWidget* parent = nullptr);
        ~PerfmonAlertOptionsPage() override;

        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;

        void SetXenObject(QSharedPointer<XenObject> object,
                          const QVariantMap& objectDataBefore,
                          const QVariantMap& objectDataCopy) override;
        AsyncOperation* SaveSettings() override;
        bool IsValidToSave() const override;
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;
        void Cleanup() override;
        bool HasChanged() const override;
        QVariantMap GetModifiedObjectData() const override;

        //! C# emailRegex: anything with an @ sign in the middle
        static bool IsValidEmail(const QString& text);
        //! C# IsValidSmtpAddress: not empty, ASCII only
        static bool IsValidSmtpAddress(const QString& text);
        //! C# Util.IsValidPort: 1-65535
        static bool IsValidPort(const QString& text);

    private:
        void populate();
        void updateEnablement();
        int indexOfLanguage(const QString& code) const;

        Ui::PerfmonAlertOptionsPage* ui;

        bool m_origEnabled = false;
        QString m_origEmailAddress;
        QString m_origSmtpServer;
        QString m_origSmtpPort;
        int m_origLanguageIndex = -1;
};

#endif // PERFMONALERTOPTIONSPAGE_H
