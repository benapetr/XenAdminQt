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

#ifndef CUSTOMFIELDSDISPLAYPAGE_H
#define CUSTOMFIELDSDISPLAYPAGE_H

#include "ieditpage.h"
#include <QVariantMap>
#include <QMap>

class AsyncOperation;

namespace Ui
{
    class CustomFieldsDisplayPage;
}

/**
 * @brief Custom fields display and editing page
 *
 * Matches C# XenAdmin.SettingsPanels.CustomFieldsDisplayPage
 * Allows viewing and editing custom fields stored in other_config
 */
class CustomFieldsDisplayPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit CustomFieldsDisplayPage(QWidget* parent = nullptr);
        ~CustomFieldsDisplayPage() override;

        // IEditPage interface
        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;

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

    private slots:
        void onAddFieldClicked();
        void onDeleteFieldClicked();

    private:
        void populateFields();
        QMap<QString, QString> getCurrentFields() const;

        Ui::CustomFieldsDisplayPage* ui;
        QString m_objectRef;
        QString m_objectType;
        QVariantMap m_objectDataBefore;
        QVariantMap m_objectDataCopy;

        // Original custom fields from other_config
        QMap<QString, QString> m_origCustomFields;
};

#endif // CUSTOMFIELDSDISPLAYPAGE_H
