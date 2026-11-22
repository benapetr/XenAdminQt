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

// C# equivalent: xenadmin/XenAdmin/Dialogs/ResolvingSubjectsDialog.cs

#ifndef RESOLVINGSUBJECTSDIALOG_H
#define RESOLVINGSUBJECTSDIALOG_H

#include <QDialog>
#include <QPointer>

namespace Ui
{
    class ResolvingSubjectsDialog;
}

class AddRemoveSubjectsAction;
class XenConnection;
class QTreeWidgetItem;

/**
 * Gives AD users or groups access: the names entered are looked up in the domain one by one
 * and the ones found are added to the pool's access list, with each name's progress shown.
 */
class ResolvingSubjectsDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit ResolvingSubjectsDialog(XenConnection* connection, QWidget* parent = nullptr);
        ~ResolvingSubjectsDialog() override;

        //! C# BeginResolve's splitting: names separated by commas or semicolons, trimmed, no blanks or repeats
        static QStringList SplitNames(const QString& text);

    protected:
        void closeEvent(QCloseEvent* event) override;
        void reject() override;

    private slots:
        void beginResolve();
        void onNameResolved(const QString& enteredName, const QString& resolvedName, const QString& sid, const QString& error);
        void onSubjectAdded(const QString& sid, const QString& name, const QString& error);
        void onCompleted();

    private:
        struct Entry
        {
            QTreeWidgetItem* item = nullptr;
            QString enteredName;
            QString resolvedName;
            QString sid;
            bool resolveFailed = false;
            bool added = false;
            bool addFailed = false;
        };

        void updateEntry(Entry& entry);
        void cancelAction();

        Ui::ResolvingSubjectsDialog* ui;
        XenConnection* m_connection;
        QPointer<AddRemoveSubjectsAction> m_action;
        QList<Entry> m_entries;
        bool m_finished = false;
};

#endif // RESOLVINGSUBJECTSDIALOG_H
