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

#ifndef HOSTAUTOSTARTEDITPAGE_H
#define HOSTAUTOSTARTEDITPAGE_H

#include "ieditpage.h"
#include <QWidget>

namespace Ui
{
    class HostAutostartEditPage;
}

class XenConnection;
class AsyncOperation;
class Host;

/**
 * @brief Host Autostart Settings Edit Page
 *
 * Allows enabling/disabling automatic VM startup when the host boots.
 * This modifies the pool's "auto_poweron" setting in other_config.
 *
 * C# equivalent: XenAdmin.SettingsPanels.HostAutostartEditPage
 */
class HostAutostartEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit HostAutostartEditPage(QWidget* parent = nullptr);
        ~HostAutostartEditPage() override;

        // IVerticalTab interface
        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;

        // IEditPage interface
        void SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy) override;
        AsyncOperation* SaveSettings() override;
        bool IsValidToSave() const override;
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;
        void Cleanup() override;
        bool HasChanged() const override;

    private:
        void repopulate();
        bool getAutostartEnabled() const;
        QSharedPointer<Host> m_host;

        Ui::HostAutostartEditPage* ui;
        bool m_originalAutostartEnabled;
};

#endif // HOSTAUTOSTARTEDITPAGE_H
