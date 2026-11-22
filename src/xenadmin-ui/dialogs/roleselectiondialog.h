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

// C# equivalent: xenadmin/XenAdmin/Dialogs/RoleSelectionDialog.cs

#ifndef ROLESELECTIONDIALOG_H
#define ROLESELECTIONDIALOG_H

#include <QDialog>
#include <QMap>
#include <QSharedPointer>
#include <QStringList>

namespace Ui
{
    class RoleSelectionDialog;
}

class Role;
class Subject;
class XenConnection;

/**
 * Chooses the role for one or more AD users or groups. One role at a time, like C#: the list
 * starts with the role they all share checked, or nothing checked when they differ.
 */
class RoleSelectionDialog : public QDialog
{
    Q_OBJECT

    public:
        RoleSelectionDialog(XenConnection* connection, const QList<QSharedPointer<Subject>>& subjects, QWidget* parent = nullptr);
        ~RoleSelectionDialog() override;

        //! The chosen roles' refs (one role, as C# allows)
        QStringList SelectedRoleRefs() const;

    private slots:
        void onCellClicked(int row, int column);
        void updateDescription();

    protected:
        void keyPressEvent(QKeyEvent* event) override;

    private:
        void checkRow(int row);
        void updateButtonsAndInfo();

        Ui::RoleSelectionDialog* ui;
        XenConnection* m_connection;
        int m_subjectCount;
        QList<QSharedPointer<Role>> m_roles;      //!< in table order, highest rank first
        QMap<QString, int> m_subjectsPerRole;     //!< role ref -> how many of the subjects hold it
};

#endif // ROLESELECTIONDIALOG_H
