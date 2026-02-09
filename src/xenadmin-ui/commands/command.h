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

#ifndef COMMAND_H
#define COMMAND_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTreeWidgetItem>
#include <QIcon>
#include <QSharedPointer>
#include "xenlib/xen/xenobjecttype.h"

class MainWindow;
class XenObject;
class AsyncOperation;

/**
 * @brief Base class for all XenAdmin commands
 *
 * This class follows the same pattern as the original C# XenAdmin Command class.
 * Each operation (VM start/stop, host operations, SR operations) should inherit
 * from this class and implement the specific logic.
 */
class Command : public QObject
{
    Q_OBJECT

    public:
        /**
         * @brief Construct a command with a main window reference
         */
        explicit Command(MainWindow* mainWindow, QObject* parent = nullptr);

        /**
         * @brief Construct a command with selection context
         */
        Command(MainWindow* mainWindow, const QStringList& selection, QObject* parent = nullptr);

        virtual ~Command() = default;

        /**
         * @brief Check if this command can run with current selection
         */
        virtual bool CanRun() const = 0;

        /**
         * @brief Execute the command
         */
        virtual void Run() = 0;

        /**
         * @brief Get the menu text for this command
         */
        virtual QString MenuText() const = 0;
        virtual QIcon GetIcon() const;

        /**
         * @brief Get the current selection
         */
        QStringList GetSelection() const
        {
            return this->m_selection;
        }

        /**
         * @brief Override selection for this command (used for explicit selections)
         */
        void SetSelectionOverride(const QList<QSharedPointer<XenObject>>& objects);

        /**
         * @brief Set the selection context
         */
        void SetSelection(const QStringList& selection)
        {
            this->m_selection = selection;
        }

        QSharedPointer<XenObject> GetObject() const;
        class SelectionManager* GetSelectionManager() const;

    protected:
        /**
         * @brief Get the main window interface
         */
        MainWindow* mainWindow() const
        {
            return this->m_mainWindow;
        }

        /**
         * @brief Get the currently selected tree item
         */
        QTreeWidgetItem* getSelectedItem() const;

        /**
         * @brief Get the object reference from selection
         */
        QString getSelectedObjectRef() const;

        /**
         * @brief Get the object name from selection
         */
        QString getSelectedObjectName() const;

        /**
         * @brief Get the object type from selection
         */
        XenObjectType getSelectedObjectType() const;

        /**
         * @brief Get the selected XenObject
         // remove - GetObject() does same
         */
        QSharedPointer<XenObject> getSelectedObject() const;

        /**
         * @brief Get selected objects (override or SelectionManager)
         */
        QList<QSharedPointer<XenObject>> getSelectedObjects() const;

        /**
         * @brief Run multiple actions similar to C# RunMultipleActions
         */
        void RunMultipleActions(const QList<AsyncOperation*>& actions, const QString& title, const QString& startDescription, const QString& endDescription, bool runActionsInParallel);

    private:
        MainWindow* m_mainWindow;
        QStringList m_selection;
        QList<QSharedPointer<XenObject>> m_selectionOverride;
};

#endif // COMMAND_H
