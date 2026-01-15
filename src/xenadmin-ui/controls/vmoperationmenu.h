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

#ifndef VMOPERATIONMENU_H
#define VMOPERATIONMENU_H

#include <QMenu>
#include <QSharedPointer>
#include <QMutex>
#include <QList>
#include <QHash>

class MainWindow;
class VM;
class Host;
class XenConnection;
class ProducerConsumerQueue;
class WlbRecommendations;
class QAction;

/**
 * @brief Base class for VM operation menus (Start On, Resume On, Migrate)
 *
 * Qt port of C# XenAdmin.Commands.VMOperationToolStripMenuItem.
 * Builds a submenu with host list and per-host eligibility checks.
 *
 * Supports three operations:
 * - start_on: Start VM on a specific host
 * - resume_on: Resume suspended VM on a specific host
 * - pool_migrate: Migrate running VM to a different host
 *
 * C# Reference: xenadmin/XenAdmin/Commands/Controls/VMOperationToolStripMenuItem.cs
 */
class VMOperationMenu : public QMenu
{
    Q_OBJECT

    public:
        enum class Operation
        {
            StartOn,      // vm_operations.start_on
            ResumeOn,     // vm_operations.resume_on
            Migrate       // vm_operations.pool_migrate
        };

        explicit VMOperationMenu(MainWindow* main_window, const QList<QSharedPointer<VM>>& vms, Operation operation, QWidget* parent = nullptr);
        ~VMOperationMenu() override;

    protected:
        virtual void AddAdditionalMenuItems();
        void aboutToShowMenu();

    private:
        struct HostMenuItem
        {
            QAction* action;
            QSharedPointer<Host> host;
            QString reason;
            bool isHomeServer;
            bool isOptimalServer;
            double starRating;  // WLB star rating (if WLB enabled)
            QHash<QSharedPointer<VM>, QString> cantRunReasons;
            bool canRunAny;
        };

        MainWindow* m_mainWindow;
        QList<QSharedPointer<VM>> m_vms;
        Operation m_operation;
        QString m_operationName;  // "start_on", "resume_on", or "pool_migrate"
        bool m_stopped = false;
        mutable QMutex m_stopMutex;
        ProducerConsumerQueue* m_workerQueue = nullptr;
        QList<HostMenuItem*> m_hostMenuItems;
        QList<QAction*> m_additionalActions;
        QSharedPointer<WlbRecommendations> m_wlbRecommendations;

        void populate();
        void stop();
        void addDisabledReason(const QString& reason);
        void updateHostList();
        void enableAppropriateHostsNoWlb();
        void enableAppropriateHostsWlb();
        void enqueueHostMenuItem(const QSharedPointer<Host>& host, HostMenuItem* menuItem, bool isHomeServer);
        void runOperationOnHost(const QSharedPointer<Host>& host);
        void runOperationOnHostForVms(const QSharedPointer<Host>& host, const QList<QSharedPointer<VM>>& vms);
        void runHomeServerOperation();
        void runOptimalServerOperation();
        
        QString getOperationName() const;
        QString getMenuText() const;
        bool isStopped() const;
        void setStopped(bool stopped);
        void clearWorkerQueue();
        XenConnection* getConnection() const;
        QStringList getSelectionRefs() const;
        QString getErrorDialogTitle() const;
        QString getErrorDialogText() const;
        bool showCantRunDialog(const QHash<QSharedPointer<VM>, QString>& cantRunReasons, bool allowProceed);
};

#endif // VMOPERATIONMENU_H
