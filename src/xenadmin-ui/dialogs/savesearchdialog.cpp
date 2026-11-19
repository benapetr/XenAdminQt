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

#include "savesearchdialog.h"
#include "ui_savesearchdialog.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xencache.h"
#include <QPushButton>

SaveSearchDialog::SaveSearchDialog(const QString& name, QWidget* parent)
    : QDialog(parent), ui(new Ui::SaveSearchDialog)
{
    this->ui->setupUi(this);
    this->ui->nameEdit->setText(name);
    this->ui->nameEdit->selectAll();

    // C#: the connected servers and pools, by name
    for (XenConnection* connection : Xen::ConnectionsManager::instance()->GetAllConnections())
    {
        if (!connection || !connection->IsConnected() || !connection->GetCache())
            continue;
        QSharedPointer<Pool> pool = connection->GetCache()->GetPoolOfOne();
        if (!pool)
            continue;
        this->ui->locationCombo->addItem(connection->GetName(),
                                         QVariant::fromValue(reinterpret_cast<quintptr>(connection)));
    }

    connect(this->ui->nameEdit, &QLineEdit::textChanged, this, &SaveSearchDialog::updateSaveButton);
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    this->updateSaveButton();
}

SaveSearchDialog::~SaveSearchDialog()
{
    delete this->ui;
}

QString SaveSearchDialog::GetName() const
{
    return this->ui->nameEdit->text().trimmed();
}

XenConnection* SaveSearchDialog::GetConnection() const
{
    return reinterpret_cast<XenConnection*>(this->ui->locationCombo->currentData().value<quintptr>());
}

void SaveSearchDialog::updateSaveButton()
{
    // CA-40307: a search needs a connection to be saved on
    this->ui->buttonBox->button(QDialogButtonBox::Save)
        ->setEnabled(!this->GetName().isEmpty() && this->ui->locationCombo->count() > 0);
}
