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

#ifndef COMMANDERRORDIALOG_H
#define COMMANDERRORDIALOG_H

#include <QDialog>
#include <QHash>
#include <QMap>
#include <QString>
#include <QSharedPointer>
#include <QIcon>

namespace Ui {
class CommandErrorDialog;
}

class QTableWidgetItem;
class XenObject;

/*!
 * \brief Dialog for displaying command errors
 *
 * A confirmation dialog for Commands. Primarily used for displaying the subset
 * of items from the multiple-selection which are not going to be actioned by the Command.
 *
 * Matches xenadmin/XenAdmin/Dialogs/CommandErrorDialog.cs structure.
 */
class CommandErrorDialog : public QDialog
{
    Q_OBJECT

    public:
        /*!
         * \brief Dialog mode - determines which buttons are shown
         */
        enum class DialogMode
        {
            Close,      ///< Show only Close button (default)
            OKCancel    ///< Show OK and Cancel buttons
        };

        /*!
         * \brief Construct CommandErrorDialog
         * \param title The title for the confirmation dialog
         * \param text The text for the confirmation dialog
         * \param cantRunReasons A map of object names to reasons why they can't be actioned
         * \param mode Whether the dialog should show a Close button, or OK and Cancel buttons
         * \param parent Parent widget
         */
        explicit CommandErrorDialog(const QString& title, const QString& text, const QMap<QString, QString>& cantRunReasons, DialogMode mode = DialogMode::Close, QWidget* parent = nullptr);

        /*!
         * \brief Construct CommandErrorDialog with icon data
         * \param title The title for the confirmation dialog
         * \param text The text for the confirmation dialog
         * \param cantRunReasons A map of object names to reasons (with optional icon paths)
         * \param mode Whether the dialog should show a Close button, or OK and Cancel buttons
         * \param parent Parent widget
         */
        explicit CommandErrorDialog(const QString& title, const QString& text, const QMap<QString, QPair<QString, QString>>& cantRunReasons, DialogMode mode = DialogMode::Close, QWidget* parent = nullptr);

        /*!
         * \brief Construct CommandErrorDialog with XenObject pointers (matches C# version)
         * \param title The title for the confirmation dialog
         * \param text The text for the confirmation dialog
         * \param cantRunReasons A map of XenObject pointers to reasons why they can't be actioned
         * \param mode Whether the dialog should show a Close button, or OK and Cancel buttons
         * \param parent Parent widget
         */
        explicit CommandErrorDialog(const QString& title, const QString& text, const QHash<QSharedPointer<XenObject>, QString>& cantRunReasons, DialogMode mode = DialogMode::Close, QWidget* parent = nullptr);

        ~CommandErrorDialog();

        DialogMode mode() const { return this->m_mode; }

    private slots:
        void onTableHeaderClicked(int logicalIndex);
        void scheduleRowResize();

    private:
        Ui::CommandErrorDialog* ui;
        DialogMode m_mode;
        int m_currentSortColumn;
        Qt::SortOrder m_currentSortOrder;
        bool m_rowResizePending = false;

        void setupDialog(const QString& title, const QString& text, DialogMode mode);
        void addRow(const QString& iconPath, const QString& name, const QString& reason);
        void addRow(const QIcon& icon, const QString& name, const QString& reason);

    protected:
        void showEvent(QShowEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
};

#endif // COMMANDERRORDIALOG_H
