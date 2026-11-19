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
#include "xenlib/customfields/customfielddefinition.h"
#include <QList>
#include <QMetaObject>
#include <QVariant>

class QCheckBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QPushButton;

namespace Ui
{
    class CustomFieldsDisplayPage;
}

/**
 * @brief Values of the pool's custom fields for one object
 *
 * Qt port of C# XenAdmin.SettingsPanels.CustomFieldsDisplayPage: one row per field defined in the
 * pool's gui_config (shared with XenCenter), a text box or an optional date and time for its
 * value, and buttons to add a field to the pool or delete one from it.
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

        void SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy) override;

        AsyncOperation* SaveSettings() override;
        bool IsValidToSave() const override;
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;
        void Cleanup() override;
        bool HasChanged() const override;

        //! Show these definitions (normally the pool's, as CustomFieldsManager reports them)
        void SetDefinitions(const QList<CustomFieldDefinition>& definitions, bool resetValues);

    private:
        //! C# CustomFieldRow
        struct FieldRow
        {
            explicit FieldRow(const CustomFieldDefinition& fieldDefinition) : definition(fieldDefinition) {}

            CustomFieldDefinition definition;
            QLabel* label = nullptr;
            QLineEdit* text = nullptr;
            QCheckBox* dateSet = nullptr;
            QDateTimeEdit* date = nullptr;
            QPushButton* deleteButton = nullptr;
        };

        void rebuild(bool resetValues);
        void addRow(const CustomFieldDefinition& definition);
        void removeRow(int index);
        QVariant currentValue(const CustomFieldDefinition& definition) const;
        static QVariant rowValue(const FieldRow& row);
        static void setRowValue(FieldRow& row, const QVariant& value);
        void onNewFieldClicked();
        void onDeleteFieldClicked(const CustomFieldDefinition& definition);

        Ui::CustomFieldsDisplayPage* ui;
        QVariantMap m_otherConfig;
        QList<FieldRow> m_rows;
        QMetaObject::Connection m_fieldsChangedConnection;
};

#endif // CUSTOMFIELDSDISPLAYPAGE_H
