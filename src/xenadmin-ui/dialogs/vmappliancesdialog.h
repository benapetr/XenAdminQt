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

// C# equivalent: xenadmin/XenAdmin/Dialogs/VMAppliances/VMAppliancesDialog.cs

#ifndef VMAPPLIANCESDIALOG_H
#define VMAPPLIANCESDIALOG_H

#include <QDialog>
#include <QSharedPointer>

namespace Ui
{
    class VMAppliancesDialog;
}

class Pool;
class VMAppliance;
class XenObject;
class QTimer;

//! Lists a pool's vApps, with their VMs, and creates, deletes, starts, stops and exports them
class VMAppliancesDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit VMAppliancesDialog(QSharedPointer<Pool> pool, QWidget* parent = nullptr);
        ~VMAppliancesDialog() override;

        //! Shows the pool's dialog, raising it if it's already open (C# ShowPerConnectionWizard)
        static void ShowFor(QSharedPointer<Pool> pool, QWidget* parent);

    private slots:
        void reload();
        void refreshButtons();
        void refreshMembers();
        void onCacheObjectChanged(QSharedPointer<XenObject> object);
        void onNew();
        void onDelete();
        void onProperties();
        void onStart();
        void onShutdown();
        void onImport();
        void onExport();

    private:
        QList<QSharedPointer<VMAppliance>> selectedAppliances() const;

        Ui::VMAppliancesDialog* ui;
        QSharedPointer<Pool> m_pool;
        QTimer* m_reloadTimer;
};

#endif // VMAPPLIANCESDIALOG_H
