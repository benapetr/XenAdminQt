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
#include "../../xenlib/xen/asyncoperation.h"
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/api.h"
#include <QInputDialog>
#include <QPushButton>
#include <QLineEdit>

CustomFieldsDisplayPage::CustomFieldsDisplayPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::CustomFieldsDisplayPage)
{
    this->ui->setupUi(this);

    this->ui->tableWidgetFields->setColumnWidth(0, 150);
    this->ui->tableWidgetFields->setColumnWidth(1, 250);
    this->ui->tableWidgetFields->setColumnWidth(2, 80);

    this->connect(this->ui->buttonAdd, &QPushButton::clicked, this, &CustomFieldsDisplayPage::onAddFieldClicked);
}

CustomFieldsDisplayPage::~CustomFieldsDisplayPage()
{
    delete this->ui;
}

QString CustomFieldsDisplayPage::text() const
{
    return tr("Custom Fields");
}

QString CustomFieldsDisplayPage::subText() const
{
    QMap<QString, QString> fields = this->getCurrentFields();
    if (fields.isEmpty())
    {
        return tr("None");
    }

    QStringList fieldList;
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it)
    {
        if (!it.value().isEmpty())
        {
            fieldList << QString("%1: %2").arg(it.key(), it.value());
        }
    }

    if (fieldList.isEmpty())
    {
        return tr("None");
    }

    return fieldList.join(", ");
}

QIcon CustomFieldsDisplayPage::image() const
{
    return QIcon(":/icons/fields_16.png");
}

void CustomFieldsDisplayPage::setXenObjects(const QString& objectRef,
                                            const QString& objectType,
                                            const QVariantMap& objectDataBefore,
                                            const QVariantMap& objectDataCopy)
{
    this->m_objectRef = objectRef;
    this->m_objectType = objectType;
    this->m_objectDataBefore = objectDataBefore;
    this->m_objectDataCopy = objectDataCopy;

    // Extract custom fields from other_config
    // Custom fields are stored with "XenCenter.CustomFields." prefix
    QVariantMap otherConfig = objectDataBefore.value("other_config").toMap();
    this->m_origCustomFields.clear();

    for (auto it = otherConfig.constBegin(); it != otherConfig.constEnd(); ++it)
    {
        if (it.key().startsWith("XenCenter.CustomFields."))
        {
            QString fieldName = it.key().mid(23); // Remove "XenCenter.CustomFields." prefix
            this->m_origCustomFields[fieldName] = it.value().toString();
        }
    }

    this->populateFields();
}

AsyncOperation* CustomFieldsDisplayPage::saveSettings()
{
    if (!this->hasChanged())
    {
        return nullptr;
    }

    // Get current fields from table
    QMap<QString, QString> currentFields = this->getCurrentFields();

    // Update objectDataCopy with new custom fields
    QVariantMap otherConfig = this->m_objectDataCopy.value("other_config").toMap();

    // Remove old custom fields
    QStringList keysToRemove;
    for (auto it = otherConfig.constBegin(); it != otherConfig.constEnd(); ++it)
    {
        if (it.key().startsWith("XenCenter.CustomFields."))
        {
            keysToRemove << it.key();
        }
    }
    for (const QString& key : keysToRemove)
    {
        otherConfig.remove(key);
    }

    // Add new custom fields
    for (auto it = currentFields.constBegin(); it != currentFields.constEnd(); ++it)
    {
        QString key = "XenCenter.CustomFields." + it.key();
        otherConfig[key] = it.value();
    }

    this->m_objectDataCopy["other_config"] = otherConfig;

    // Return inline AsyncOperation
    class CustomFieldsOperation : public AsyncOperation
    {
    public:
        CustomFieldsOperation(XenConnection* conn,
                              const QString& objectRef,
                              const QString& objectType,
                              const QVariantMap& otherConfig,
                              QObject* parent)
            : AsyncOperation(conn, tr("Update Custom Fields"),
                             tr("Updating custom fields..."), parent),
              m_objectRef(objectRef), m_objectType(objectType), m_otherConfig(otherConfig)
        {}

    protected:
        void run() override
        {
            XenRpcAPI api(connection()->getSession());

            setPercentComplete(30);

            // Build method name based on object type
            QString methodName = m_objectType + ".set_other_config";

            QVariantList params;
            params << connection()->getSessionId() << m_objectRef << m_otherConfig;
            QByteArray request = api.buildJsonRpcCall(methodName, params);
            connection()->sendRequest(request);

            setPercentComplete(100);
        }

    private:
        QString m_objectRef;
        QString m_objectType;
        QVariantMap m_otherConfig;
    };

    QString objectType = this->m_objectDataBefore.value("class", "VM").toString();
    return new CustomFieldsOperation(this->m_connection, this->m_objectRef, objectType, otherConfig, this);
}

bool CustomFieldsDisplayPage::isValidToSave() const
{
    return true;
}

void CustomFieldsDisplayPage::showLocalValidationMessages()
{
    // No validation needed
}

void CustomFieldsDisplayPage::hideLocalValidationMessages()
{
    // No validation messages
}

void CustomFieldsDisplayPage::cleanup()
{
    // Nothing to clean up
}

bool CustomFieldsDisplayPage::hasChanged() const
{
    QMap<QString, QString> currentFields = this->getCurrentFields();

    if (currentFields.size() != this->m_origCustomFields.size())
    {
        return true;
    }

    for (auto it = currentFields.constBegin(); it != currentFields.constEnd(); ++it)
    {
        if (!this->m_origCustomFields.contains(it.key()) ||
            this->m_origCustomFields[it.key()] != it.value())
        {
            return true;
        }
    }

    return false;
}

void CustomFieldsDisplayPage::onAddFieldClicked()
{
    bool ok;
    QString fieldName = QInputDialog::getText(this, tr("Add Custom Field"),
                                              tr("Field name:"), QLineEdit::Normal,
                                              "", &ok);
    if (ok && !fieldName.isEmpty())
    {
        int row = this->ui->tableWidgetFields->rowCount();
        this->ui->tableWidgetFields->insertRow(row);

        // Field name
        QTableWidgetItem* nameItem = new QTableWidgetItem(fieldName);
        this->ui->tableWidgetFields->setItem(row, 0, nameItem);

        // Value
        QTableWidgetItem* valueItem = new QTableWidgetItem("");
        this->ui->tableWidgetFields->setItem(row, 1, valueItem);

        // Delete button
        QPushButton* deleteBtn = new QPushButton(tr("Delete"));
        this->connect(deleteBtn, &QPushButton::clicked, this, &CustomFieldsDisplayPage::onDeleteFieldClicked);
        this->ui->tableWidgetFields->setCellWidget(row, 2, deleteBtn);
    }
}

void CustomFieldsDisplayPage::onDeleteFieldClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (button)
    {
        for (int row = 0; row < this->ui->tableWidgetFields->rowCount(); ++row)
        {
            if (this->ui->tableWidgetFields->cellWidget(row, 2) == button)
            {
                this->ui->tableWidgetFields->removeRow(row);
                break;
            }
        }
    }
}

void CustomFieldsDisplayPage::populateFields()
{
    this->ui->tableWidgetFields->setRowCount(0);

    for (auto it = this->m_origCustomFields.constBegin(); it != this->m_origCustomFields.constEnd(); ++it)
    {
        int row = this->ui->tableWidgetFields->rowCount();
        this->ui->tableWidgetFields->insertRow(row);

        // Field name
        QTableWidgetItem* nameItem = new QTableWidgetItem(it.key());
        this->ui->tableWidgetFields->setItem(row, 0, nameItem);

        // Value
        QTableWidgetItem* valueItem = new QTableWidgetItem(it.value());
        this->ui->tableWidgetFields->setItem(row, 1, valueItem);

        // Delete button
        QPushButton* deleteBtn = new QPushButton(tr("Delete"));
        this->connect(deleteBtn, &QPushButton::clicked, this, &CustomFieldsDisplayPage::onDeleteFieldClicked);
        this->ui->tableWidgetFields->setCellWidget(row, 2, deleteBtn);
    }
}

QMap<QString, QString> CustomFieldsDisplayPage::getCurrentFields() const
{
    QMap<QString, QString> fields;

    for (int row = 0; row < this->ui->tableWidgetFields->rowCount(); ++row)
    {
        QTableWidgetItem* nameItem = this->ui->tableWidgetFields->item(row, 0);
        QTableWidgetItem* valueItem = this->ui->tableWidgetFields->item(row, 1);

        if (nameItem && valueItem)
        {
            QString name = nameItem->text().trimmed();
            QString value = valueItem->text().trimmed();
            if (!name.isEmpty())
            {
                fields[name] = value;
            }
        }
    }

    return fields;
}
