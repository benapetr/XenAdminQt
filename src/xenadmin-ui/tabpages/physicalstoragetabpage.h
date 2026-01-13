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

#ifndef PHYSICALSTORAGETABPAGE_H
#define PHYSICALSTORAGETABPAGE_H

#include "basetabpage.h"

class MainWindow;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class PhysicalStorageTabPage;
}
QT_END_NAMESPACE

/**
 * Physical Storage tab page showing storage repositories for Hosts and Pools.
 * 
 * C# Equivalent: PhysicalStoragePage
 * 
 * This tab shows all storage repositories (SRs) visible to a host or pool,
 * with columns for name, description, type, shared status, usage, size, and virtual allocation.
 * 
 * Buttons:
 * - New SR: Create a new storage repository
 * - Reclaim Freed Space (Trim): Reclaim space from thin-provisioned storage
 * - Properties: Open SR properties dialog
 */
class PhysicalStorageTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit PhysicalStorageTabPage(QWidget* parent = nullptr);
        ~PhysicalStorageTabPage();

        QString GetTitle() const override
        {
            return "Storage";
        }
        Type GetType() const override
        {
            return Type::PhysicalStorage;
        }

        QString HelpID() const override
        {
            return "TabPageStorage";
        }

        bool IsApplicableForObjectType(const QString& objectType) const override;

    protected:
        void refreshContent() override;

    private slots:
        void onNewSRButtonClicked();
        void onTrimButtonClicked();
        void onPropertiesButtonClicked();
        void onStorageTableCustomContextMenuRequested(const QPoint& pos);
        void onStorageTableSelectionChanged();
        void onStorageTableDoubleClicked(const QModelIndex& index);

    private:
        Ui::PhysicalStorageTabPage* ui;

        void populateHostStorage();
        void populatePoolStorage();
        void updateButtonStates();
        QString getSelectedSRRef() const;
        MainWindow* getMainWindow() const;
};

#endif // PHYSICALSTORAGETABPAGE_H
