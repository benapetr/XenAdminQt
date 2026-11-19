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

#include "customfieldsdisplaypage.h"
#include "ui_customfieldsdisplaypage.h"
#include "../dialogs/newcustomfielddialog.h"
#include "../dialogs/warningdialogs/warningdialog.h"
#include "xenlib/customfields/customfieldsmanager.h"
#include "xenlib/xen/actions/general/customfieldactions.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/xenobject.h"
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

namespace
{
    QString ellipsise(const QString& text, int maxChars)
    {
        return text.length() > maxChars ? text.left(maxChars - 1) + QChar(0x2026) : text;
    }
}

CustomFieldsDisplayPage::CustomFieldsDisplayPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::CustomFieldsDisplayPage)
{
    this->ui->setupUi(this);
    this->ui->fieldsLayout->setRowStretch(0, 1);
    connect(this->ui->buttonAdd, &QPushButton::clicked, this, &CustomFieldsDisplayPage::onNewFieldClicked);
}

CustomFieldsDisplayPage::~CustomFieldsDisplayPage()
{
    QObject::disconnect(this->m_fieldsChangedConnection);
    delete this->ui;
}

QString CustomFieldsDisplayPage::GetText() const
{
    return tr("Custom Fields");
}

// C# SubText: "name: value" for every field with a value
QString CustomFieldsDisplayPage::GetSubText() const
{
    QStringList fields;
    for (const FieldRow& row : this->m_rows)
    {
        const QVariant value = rowValue(row);
        if (value.isNull())
            continue;
        fields << QString("%1: %2").arg(row.definition.GetName(),
                                        CustomFieldsManager::ValueToDisplayString(row.definition.GetType(), value));
    }

    return fields.isEmpty() ? tr("None") : fields.join(", ");
}

QIcon CustomFieldsDisplayPage::GetImage() const
{
    return QIcon(":/icons/fields_16.png");
}

void CustomFieldsDisplayPage::SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataCopy);
    this->m_object = object;
    this->m_otherConfig = objectDataBefore.value("other_config").toMap();
    if (this->m_otherConfig.isEmpty() && object)
        this->m_otherConfig = object->GetOtherConfig();

    // C#: rebuild when the pool's definitions change, keeping what was typed
    QObject::disconnect(this->m_fieldsChangedConnection);
    this->m_fieldsChangedConnection = connect(CustomFieldsManager::instance(), &CustomFieldsManager::customFieldsChanged,
                                              this, [this]() { this->rebuild(false); });
    this->rebuild(true);
}

void CustomFieldsDisplayPage::rebuild(bool resetValues)
{
    XenConnection* connection = this->m_object ? this->m_object->GetConnection() : nullptr;
    this->SetDefinitions(CustomFieldsManager::instance()->getCustomFields(connection), resetValues);
}

// C# Rebuild: add rows for new fields, drop rows for deleted ones
void CustomFieldsDisplayPage::SetDefinitions(const QList<CustomFieldDefinition>& definitions, bool resetValues)
{
    for (const CustomFieldDefinition& definition : definitions)
    {
        int index = -1;
        for (int i = 0; i < this->m_rows.size(); ++i)
        {
            if (this->m_rows.at(i).definition == definition)
            {
                index = i;
                break;
            }
        }

        if (index == -1)
            this->addRow(definition);
        else if (resetValues)
            setRowValue(this->m_rows[index], this->currentValue(definition));
    }

    for (int i = this->m_rows.size() - 1; i >= 0; --i)
    {
        if (!definitions.contains(this->m_rows.at(i).definition))
            this->removeRow(i);
    }

    // Keep the rows at the top
    QGridLayout* layout = this->ui->fieldsLayout;
    for (int row = 0; row < layout->rowCount(); ++row)
        layout->setRowStretch(row, 0);
    layout->setRowStretch(layout->rowCount(), 1);

    emit this->populated();
}

void CustomFieldsDisplayPage::addRow(const CustomFieldDefinition& definition)
{
    QGridLayout* layout = this->ui->fieldsLayout;
    const int gridRow = layout->rowCount();
    QWidget* parent = this->ui->fieldsWidget;

    FieldRow row(definition);
    row.label = new QLabel(ellipsise(definition.GetName(), 45), parent);
    row.label->setTextFormat(Qt::PlainText);
    row.label->setToolTip(definition.GetName());
    layout->addWidget(row.label, gridRow, 0);

    if (definition.GetType() == CustomFieldDefinition::Type::Date)
    {
        // C# DateTimePicker with ShowCheckBox: unchecked means no value
        QWidget* editor = new QWidget(parent);
        QHBoxLayout* editorLayout = new QHBoxLayout(editor);
        editorLayout->setContentsMargins(0, 0, 0, 0);
        row.dateSet = new QCheckBox(editor);
        row.dateSet->setToolTip(tr("Set a value"));
        row.date = new QDateTimeEdit(QDateTime::currentDateTime(), editor);
        row.date->setCalendarPopup(true);
        row.date->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat) + QStringLiteral(" HH:mm:ss"));
        row.date->setEnabled(false);
        editorLayout->addWidget(row.dateSet);
        editorLayout->addWidget(row.date, 1);
        layout->addWidget(editor, gridRow, 1);

        QDateTimeEdit* date = row.date;
        connect(row.dateSet, &QCheckBox::toggled, date, &QWidget::setEnabled);
        connect(row.dateSet, &QCheckBox::toggled, this, &IEditPage::populated);
        connect(row.date, &QDateTimeEdit::dateTimeChanged, this, &IEditPage::populated);
    } else
    {
        row.text = new QLineEdit(parent);
        layout->addWidget(row.text, gridRow, 1);
        connect(row.text, &QLineEdit::textChanged, this, &IEditPage::populated);
    }

    row.deleteButton = new QPushButton(tr("Delete"), parent);
    row.deleteButton->setToolTip(tr("Delete this custom field from the pool"));
    layout->addWidget(row.deleteButton, gridRow, 2);
    connect(row.deleteButton, &QPushButton::clicked, this, [this, definition]() { this->onDeleteFieldClicked(definition); });

    setRowValue(row, this->currentValue(definition));
    this->m_rows.append(row);
}

void CustomFieldsDisplayPage::removeRow(int index)
{
    FieldRow row = this->m_rows.takeAt(index);
    delete row.label;
    if (row.date)
        delete row.date->parentWidget(); // the checkbox and editor container
    delete row.text;
    delete row.deleteButton;
}

QVariant CustomFieldsDisplayPage::currentValue(const CustomFieldDefinition& definition) const
{
    return CustomFieldsManager::GetCustomFieldValue(this->m_otherConfig, definition);
}

// C# CustomFieldRow.GetValue: the text, or the date and time if set; null otherwise
QVariant CustomFieldsDisplayPage::rowValue(const FieldRow& row)
{
    if (row.text)
        return row.text->text().isEmpty() ? QVariant() : QVariant(row.text->text());
    if (row.date && row.dateSet && row.dateSet->isChecked())
        return row.date->dateTime().toUTC();
    return QVariant();
}

void CustomFieldsDisplayPage::setRowValue(FieldRow& row, const QVariant& value)
{
    if (row.text)
    {
        row.text->setText(value.toString());
        return;
    }

    if (row.date && row.dateSet)
    {
        row.dateSet->setChecked(!value.isNull());
        if (!value.isNull())
            row.date->setDateTime(value.toDateTime().toLocalTime());
    }
}

// C# SaveSettings: every field whose value is or was set
AsyncOperation* CustomFieldsDisplayPage::SaveSettings()
{
    if (!this->m_object || !this->HasChanged())
        return nullptr;

    QList<SaveCustomFieldsAction::FieldValue> values;
    for (const FieldRow& row : std::as_const(this->m_rows))
    {
        const QVariant current = this->currentValue(row.definition);
        const QVariant value = rowValue(row);
        if (current.isNull() && value.isNull())
            continue;
        values.append(qMakePair(row.definition, value));
    }

    return new SaveCustomFieldsAction(this->m_object, values, this);
}

bool CustomFieldsDisplayPage::IsValidToSave() const
{
    return true;
}

void CustomFieldsDisplayPage::ShowLocalValidationMessages()
{
}

void CustomFieldsDisplayPage::HideLocalValidationMessages()
{
}

void CustomFieldsDisplayPage::Cleanup()
{
    QObject::disconnect(this->m_fieldsChangedConnection);
}

// Compared as stored, so a date only differs if it does to the second
bool CustomFieldsDisplayPage::HasChanged() const
{
    for (const FieldRow& row : this->m_rows)
    {
        const CustomFieldDefinition::Type type = row.definition.GetType();
        if (CustomFieldsManager::ValueToInvariantString(type, this->currentValue(row.definition))
            != CustomFieldsManager::ValueToInvariantString(type, rowValue(row)))
            return true;
    }
    return false;
}

void CustomFieldsDisplayPage::onNewFieldClicked()
{
    XenConnection* connection = this->m_object ? this->m_object->GetConnection() : nullptr;
    if (!connection)
        return;

    NewCustomFieldDialog dialog(connection, this);
    if (dialog.exec() == QDialog::Accepted)
        (new AddCustomFieldAction(connection, dialog.GetDefinition()))->RunAsync(true);
}

void CustomFieldsDisplayPage::onDeleteFieldClicked(const CustomFieldDefinition& definition)
{
    const QString name = ellipsise(definition.GetName(), 50);
    if (WarningDialog::ShowYesNo(tr("This will delete '%1' permanently, making it unavailable to all resources in the pool "
                                    "and destroying all data associated with it. Do you want to continue?").arg(name),
                                 tr("Delete Custom Field"), this) != WarningDialog::Result::Yes)
        return;

    XenConnection* connection = this->m_object ? this->m_object->GetConnection() : nullptr;
    if (connection && connection->IsConnected())
        (new RemoveCustomFieldAction(connection, definition))->RunAsync(true);
    else
        QMessageBox::critical(this, tr("Delete Custom Field"),
                              tr("The server was disconnected before the action could be started."));
}
