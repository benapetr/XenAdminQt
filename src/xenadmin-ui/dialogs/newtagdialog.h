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

#ifndef NEWTAGDIALOG_H
#define NEWTAGDIALOG_H

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QPushButton;
class QTableWidget;

class NewTagDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit NewTagDialog(QWidget* parent = nullptr);

        void SetTags(const QStringList& allTags,
                     const QStringList& selectedTags,
                     const QStringList& indeterminateTags = QStringList());

        QStringList GetSelectedTags() const;
        QStringList GetIndeterminateTags() const;

    private slots:
        void onAddTextChanged(const QString& text);
        void onAddClicked();
        void onCellClicked(int row, int column);
        void onToggleSelection();

    private:
        enum class TagPriority
        {
            Checked = 0,
            Indeterminate = 1,
            Unchecked = 2
        };

        void addOrUpdateTag(const QString& tag, Qt::CheckState checkState);
        void resortRows();
        int priorityForState(Qt::CheckState state) const;

        QLineEdit* m_addLineEdit = nullptr;
        QPushButton* m_addButton = nullptr;
        QTableWidget* m_table = nullptr;
};

#endif // NEWTAGDIALOG_H
