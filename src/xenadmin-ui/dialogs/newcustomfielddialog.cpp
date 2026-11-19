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


#include "newcustomfielddialog.h"
#include "ui_newcustomfielddialog.h"
#include "xenlib/customfields/customfieldsmanager.h"
#include <QPushButton>

NewCustomFieldDialog::NewCustomFieldDialog(XenConnection* connection, QWidget* parent)
    : QDialog(parent), ui(new Ui::NewCustomFieldDialog)
{
    this->ui->setupUi(this);
    this->m_existingFields = CustomFieldsManager::instance()->getCustomFields(connection);

    connect(this->ui->nameLineEdit, &QLineEdit::textChanged, this, &NewCustomFieldDialog::updateControls);
    this->updateControls();
}

NewCustomFieldDialog::~NewCustomFieldDialog()
{
    delete this->ui;
}

CustomFieldDefinition NewCustomFieldDialog::GetDefinition() const
{
    // The combo box order matches C# CustomFieldDefinition.Types: String, Date
    return CustomFieldDefinition(this->ui->nameLineEdit->text().trimmed(),
                                 this->ui->typeComboBox->currentIndex() == 1 ? CustomFieldDefinition::Type::Date
                                                                            : CustomFieldDefinition::Type::String);
}

void NewCustomFieldDialog::SetExistingFields(const QList<CustomFieldDefinition>& fields)
{
    this->m_existingFields = fields;
    this->updateControls();
}

// C# UpdateControls
void NewCustomFieldDialog::updateControls()
{
    QPushButton* ok = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    const QString name = this->ui->nameLineEdit->text().trimmed();
    if (name.isEmpty())
    {
        ok->setEnabled(false);
        this->ui->duplicateLabel->setVisible(false);
        return;
    }

    bool duplicate = false;
    for (const CustomFieldDefinition& field : std::as_const(this->m_existingFields))
    {
        if (field.GetName().trimmed() == name)
        {
            duplicate = true;
            break;
        }
    }

    ok->setEnabled(!duplicate);
    this->ui->duplicateLabel->setVisible(duplicate);
}
