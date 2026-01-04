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

#ifndef REPAIRSRDIALOG_H
#define REPAIRSRDIALOG_H

#include <QDialog>
#include <QList>
#include <QSharedPointer>
#include <QTreeWidgetItem>

class SR;
class AsyncOperation;
class MultipleOperation;
class XenConnection;
class XenObject;

namespace Ui
{
    class RepairSRDialog;
}

/**
 * @class RepairSRDialog
 * @brief Dialog for repairing broken storage repositories
 * 
 * This dialog displays SRs that have connection issues (missing PBDs, unplugged PBDs)
 * and allows the user to repair them by recreating/plugging PBDs on all necessary hosts.
 * 
 * Ported from C# XenAdmin.Dialogs.RepairSRDialog
 */
class RepairSRDialog : public QDialog
{
    Q_OBJECT

    public:
        /**
         * @brief Construct dialog for a single SR
         * @param sr Storage repository to repair
         * @param runAction If true, automatically run repair action; if false, create action but don't run
         * @param parent Parent widget
         */
        explicit RepairSRDialog(QSharedPointer<SR> sr, bool runAction = true, QWidget* parent = nullptr);

        /**
         * @brief Construct dialog for multiple SRs
         * @param srs List of storage repositories to repair
         * @param runAction If true, automatically run repair action; if false, create action but don't run
         * @param parent Parent widget
         */
        explicit RepairSRDialog(QList<QSharedPointer<SR>> srs, bool runAction = true, QWidget* parent = nullptr);

        ~RepairSRDialog();

        /**
         * @brief Get the repair action (whether completed or not)
         * @return Pointer to the repair action
         */
        AsyncOperation* GetRepairAction() const;

        /**
         * @brief Check if operation succeeded with warnings (e.g., multipath issues)
         * @return True if completed with warnings
         */
        bool SucceededWithWarning() const;

        /**
         * @brief Get the warning description if operation succeeded with warnings
         * @return Warning message
         */
        QString SucceededWithWarningDescription() const;

    private slots:
        void onRepairButtonClicked();
        void onCloseButtonClicked();
        void onSrPropertyChanged();
        void onHostCollectionChanged(XenConnection* connection, const QString& type, const QString& ref);
        void onPbdCollectionChanged(XenConnection* connection, const QString& type, const QString& ref);
        void onActionChanged();
        void onActionCompleted();

    private:
        void initialize();
        void buildTree();
        void shrink();
        void grow();
        void updateProgressControls();
        void finalizeProgressControls();
        bool actionInProgress() const;

        struct RepairTreeNode
        {
            QSharedPointer<SR> sr;
            QSharedPointer<XenObject> host; // Can be null for SR-level nodes
            QVariantMap pbdData; // Can be empty if PBD missing
            QTreeWidgetItem* item;

            RepairTreeNode() : item(nullptr) {}
        };

        void createTreeNode(RepairTreeNode& node);
        void updateTreeNodeStatus(RepairTreeNode& node);

        Ui::RepairSRDialog* ui;
        QList<QSharedPointer<SR>> srList;
        AsyncOperation* repairAction;
        bool runAction;
        bool succeededWithWarning;
        QString succeededWithWarningDescription;
        int originalHeight;
        bool shrunk;

        QList<RepairTreeNode> treeNodes;
};

#endif // REPAIRSRDIALOG_H
