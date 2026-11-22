/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#include "roleselectiondialog.h"
#include "ui_roleselectiondialog.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/role.h"
#include "xenlib/xen/subject.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include <QHeaderView>
#include <QKeyEvent>
#include <algorithm>

RoleSelectionDialog::RoleSelectionDialog(XenConnection* connection, const QList<QSharedPointer<Subject>>& subjects, QWidget* parent)
    : QDialog(parent), ui(new Ui::RoleSelectionDialog), m_connection(connection), m_subjectCount(subjects.size())
{
    this->ui->setupUi(this);
    this->ui->tableRoles->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    const bool allGroups = !subjects.isEmpty() && std::all_of(subjects.begin(), subjects.end(), [](const QSharedPointer<Subject>& s) { return s->IsGroup(); });
    const bool allUsers = !subjects.isEmpty() && std::none_of(subjects.begin(), subjects.end(), [](const QSharedPointer<Subject>& s) { return s->IsGroup(); });
    this->ui->labelSubjectType->setPixmap(QIcon(allUsers ? ":/icons/user_32.png" : ":/icons/user_group_32.png").pixmap(32, 32));

    if (subjects.size() == 1)
    {
        const QSharedPointer<Subject> subject = subjects.first();
        QString name = !subject->DisplayName().isEmpty() ? subject->DisplayName()
                     : !subject->SubjectName().isEmpty() ? subject->SubjectName() : tr("<unknown AD user>");
        if (name.length() > 30)
            name = name.left(29) + QChar(0x2026);
        this->ui->labelBlurb->setText(allGroups
            ? tr("Select the role you want to assign to group '%1'. Each role inherits all the privileges of the subordinate role below it in the list.").arg(name)
            : tr("Select the role you want to assign to user '%1'. Each role inherits all the privileges of the subordinate role below it in the list.").arg(name));
    } else if (allGroups)
    {
        this->ui->labelBlurb->setText(tr("Select the role you want to assign to these groups. Each role inherits all the privileges of the subordinate role below it in the list."));
    } else if (allUsers)
    {
        this->ui->labelBlurb->setText(tr("Select the role you want to assign to these users. Each role inherits all the privileges of the subordinate role below it in the list."));
    } else
    {
        this->ui->labelBlurb->setText(tr("Select the role you want to assign to these users and groups. Each role inherits all the privileges of the subordinate role below it in the list."));
    }

    // The server's roles, highest rank first (C# OrderBy(r => r).Reverse())
    for (const QSharedPointer<Role>& role : connection->GetCache()->GetAll<Role>(XenObjectType::Role))
    {
        if (role && role->IsAssignable())
            this->m_roles.append(role);
    }
    std::stable_sort(this->m_roles.begin(), this->m_roles.end(), [](const QSharedPointer<Role>& a, const QSharedPointer<Role>& b)
    {
        return a->Rank() > b->Rank();
    });

    this->ui->tableRoles->setRowCount(this->m_roles.size());
    for (int row = 0; row < this->m_roles.size(); ++row)
    {
        const QSharedPointer<Role> role = this->m_roles.at(row);
        int holders = 0;
        for (const QSharedPointer<Subject>& subject : subjects)
        {
            if (subject->RoleRefs().contains(role->OpaqueRef()))
                ++holders;
        }
        this->m_subjectsPerRole.insert(role->OpaqueRef(), holders);

        auto* check = new QTableWidgetItem();
        check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        check->setCheckState(holders == this->m_subjectCount && holders > 0 ? Qt::Checked
                             : holders == 0 ? Qt::Unchecked : Qt::PartiallyChecked);
        this->ui->tableRoles->setItem(row, 0, check);
        this->ui->tableRoles->setItem(row, 1, new QTableWidgetItem(role->FriendlyName()));
    }
    if (!this->m_roles.isEmpty())
        this->ui->tableRoles->selectRow(0);

    connect(this->ui->tableRoles, &QTableWidget::cellClicked, this, &RoleSelectionDialog::onCellClicked);
    connect(this->ui->tableRoles, &QTableWidget::itemSelectionChanged, this, &RoleSelectionDialog::updateDescription);
    connect(this->ui->buttonSave, &QPushButton::clicked, this, &QDialog::accept);
    connect(this->ui->buttonCancel, &QPushButton::clicked, this, &QDialog::reject);

    this->updateDescription();
    this->ui->labelPoolAdminWarning->setVisible(false);
    this->ui->buttonSave->setEnabled(false);
}

RoleSelectionDialog::~RoleSelectionDialog()
{
    delete this->ui;
}

QStringList RoleSelectionDialog::SelectedRoleRefs() const
{
    QStringList refs;
    for (int row = 0; row < this->m_roles.size(); ++row)
    {
        if (this->ui->tableRoles->item(row, 0)->checkState() == Qt::Checked)
            refs.append(this->m_roles.at(row)->OpaqueRef());
    }
    return refs;
}

void RoleSelectionDialog::updateDescription()
{
    const QList<QTableWidgetItem*> selected = this->ui->tableRoles->selectedItems();
    if (selected.isEmpty())
        return;
    this->ui->labelDescription->setText(this->m_roles.at(selected.first()->row())->FriendlyDescription());
}

void RoleSelectionDialog::onCellClicked(int row, int column)
{
    // C#: only the check box column checks a row, so rows can be browsed for their descriptions
    if (column == 0)
        this->checkRow(row);
}

void RoleSelectionDialog::keyPressEvent(QKeyEvent* event)
{
    // C# gridRoles_KeyPress: space checks the selected row
    const QList<QTableWidgetItem*> selected = this->ui->tableRoles->selectedItems();
    if (event->key() == Qt::Key_Space && !selected.isEmpty())
    {
        this->checkRow(selected.first()->row());
        return;
    }
    QDialog::keyPressEvent(event);
}

void RoleSelectionDialog::checkRow(int checkedRow)
{
    for (int row = 0; row < this->m_roles.size(); ++row)
        this->ui->tableRoles->item(row, 0)->setCheckState(row == checkedRow ? Qt::Checked : Qt::Unchecked);
    this->updateButtonsAndInfo();
}

void RoleSelectionDialog::updateButtonsAndInfo()
{
    // C# UpdateButtonsAndInfo: Save once the choice changes something; warn when Pool Admin is taken away
    for (int row = 0; row < this->m_roles.size(); ++row)
    {
        const QSharedPointer<Role> role = this->m_roles.at(row);
        const bool checked = this->ui->tableRoles->item(row, 0)->checkState() == Qt::Checked;
        const int holders = this->m_subjectsPerRole.value(role->OpaqueRef());
        if (checked && holders < this->m_subjectCount)
        {
            this->ui->buttonSave->setEnabled(true);
            this->ui->labelPoolAdminWarning->setVisible(false);
            return;
        }
        if (!checked && holders > 0)
        {
            this->ui->buttonSave->setEnabled(true);
            // No licence check here: XCP-ng can always rotate the pool secret
            this->ui->labelPoolAdminWarning->setVisible(role->GetName() == "pool-admin" && Host::StockholmOrGreater(this->m_connection));
            return;
        }
    }
    this->ui->labelPoolAdminWarning->setVisible(false);
    this->ui->buttonSave->setEnabled(false);
}
