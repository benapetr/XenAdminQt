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

// C# equivalent: xenadmin/XenAdmin/Commands/Controls/AssignVMGroupToolStripMenuItem.cs
// (AssignGroupToolStripMenuItem<T>) and xenadmin/XenAdmin/Commands/VMGroupCommand.cs

#ifndef ASSIGNTOGROUPMENU_H
#define ASSIGNTOGROUPMENU_H

#include "../command.h"
#include "vmgroup.h"
#include <QMenu>

class VM;

/**
 * "Assign to vApp" / "Assign to Snapshot Schedule": VMs of one pool, at least one of them a
 * real, unlocked VM. Its items live in AssignToGroupMenu.
 */
class AssignToGroupCommand : public Command
{
    Q_OBJECT

    public:
        AssignToGroupCommand(VMGroup::Kind kind, MainWindow* mainWindow, QObject* parent = nullptr);

        bool CanRun() const override;
        void Run() override {}
        QString MenuText() const override;

        //! The selected VMs
        QList<QSharedPointer<VM>> SelectedVMs() const;

    private:
        VMGroup::Kind m_kind;
};

/**
 * Submenu listing "New vApp..." / "New Snapshot Schedule..." and the pool's groups; choosing a
 * group adds the selected VMs to it, or takes a single VM out of the group it's already in.
 */
class AssignToGroupMenu : public QMenu
{
    Q_OBJECT

    public:
        AssignToGroupMenu(VMGroup::Kind kind, MainWindow* mainWindow, QWidget* parent = nullptr);

        bool CanRun() const;

    private slots:
        void onAboutToShow();

    private:
        void assign(QSharedPointer<XenObject> group, const QList<QSharedPointer<VM>>& vms);

        VMGroup::Kind m_kind;
        MainWindow* m_mainWindow;
};

/**
 * Opens the Manage vApps or VM Snapshot Schedules dialog for the selection's pool.
 * C# VMGroupCommand<T>
 */
class ManageVMGroupsCommand : public Command
{
    Q_OBJECT

    public:
        ManageVMGroupsCommand(VMGroup::Kind kind, MainWindow* mainWindow, QObject* parent = nullptr);

        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;

    private:
        VMGroup::Kind m_kind;
};

#endif // ASSIGNTOGROUPMENU_H
