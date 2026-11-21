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
// (AssignGroupToolStripMenuItem<VM_appliance>) and VMGroupCommand<VM_appliance>

#ifndef ASSIGNTOVAPPMENU_H
#define ASSIGNTOVAPPMENU_H

#include "../command.h"
#include <QMenu>

class VM;
class VMAppliance;

/**
 * The "Assign to vApp" command: VMs of one pool, at least one of them a real, unlocked VM.
 * Its items live in AssignToVAppMenu.
 */
class AssignToVAppCommand : public Command
{
    Q_OBJECT

    public:
        explicit AssignToVAppCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        bool CanRun() const override;
        void Run() override {}
        QString MenuText() const override;

        //! The selected VMs
        QList<QSharedPointer<VM>> SelectedVMs() const;
};

/**
 * Submenu listing "New vApp..." and the pool's vApps; choosing a vApp adds the selected VMs
 * to it, or takes a single VM out of the vApp it's already in.
 */
class AssignToVAppMenu : public QMenu
{
    Q_OBJECT

    public:
        explicit AssignToVAppMenu(MainWindow* mainWindow, QWidget* parent = nullptr);

        bool CanRun() const;

        /**
         * C# AssignGroupToVMCommand.ChangesOK: if any of \a vms is already in a vApp other than
         * \a appliance (null for a new one, named \a applianceName), asks whether to move them.
         * Returns whether to go on.
         */
        static bool ChangesOK(const QList<QSharedPointer<VM>>& vms, QSharedPointer<VMAppliance> appliance,
                              const QString& applianceName, QWidget* parent);

    private slots:
        void onAboutToShow();

    private:
        void assign(QSharedPointer<VMAppliance> appliance, const QList<QSharedPointer<VM>>& vms);

        MainWindow* m_mainWindow;
};

/**
 * Opens the Manage vApps dialog for the selection's pool.
 * C# VMGroupCommand<VM_appliance>
 */
class ManageVAppsCommand : public Command
{
    Q_OBJECT

    public:
        explicit ManageVAppsCommand(MainWindow* mainWindow, QObject* parent = nullptr);

        bool CanRun() const override;
        void Run() override;
        QString MenuText() const override;
};

#endif // ASSIGNTOVAPPMENU_H
