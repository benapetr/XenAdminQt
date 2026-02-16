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

#ifndef NETWORKOPTIONSEDITPAGE_H
#define NETWORKOPTIONSEDITPAGE_H

#include "ieditpage.h"
#include <QVariantMap>

namespace Ui {
class NetworkOptionsEditPage;
}

class AsyncOperation;

/**
 * @brief Network options configuration edit page (IGMP Snooping)
 *
 * Qt equivalent of C# XenAdmin.SettingsPanels.NetworkOptionsEditPage
 * Allows enabling/disabling IGMP snooping for pools using vSwitch backend.
 *
 * C# Reference: xenadmin/XenAdmin/SettingsPanels/NetworkOptionsEditPage.cs
 */
class NetworkOptionsEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit NetworkOptionsEditPage(QWidget* parent = nullptr);
        ~NetworkOptionsEditPage() override;

        // IEditPage interface
        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;

        void SetXenObject(QSharedPointer<XenObject> object,
                          const QVariantMap& objectDataBefore,
                          const QVariantMap& objectDataCopy) override;

        AsyncOperation* SaveSettings() override;
        bool HasChanged() const override;
        bool IsValidToSave() const override;
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;
        void Cleanup() override;

    private:
        Ui::NetworkOptionsEditPage* ui;

        QString m_poolRef_;
        QVariantMap m_objectDataBefore_;
        QVariantMap m_objectDataCopy_;
};

#endif // NETWORKOPTIONSEDITPAGE_H
