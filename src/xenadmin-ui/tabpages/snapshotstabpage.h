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

#ifndef SNAPSHOTSTABPAGE_H
#define SNAPSHOTSTABPAGE_H

#include "basetabpage.h"
#include <QTreeWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class SnapshotsTabPage;
}
QT_END_NAMESPACE

class SnapshotsTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit SnapshotsTabPage(QWidget* parent = nullptr);
        ~SnapshotsTabPage();

        QString GetTitle() const override
        {
            return "Snapshots";
        }
        bool IsApplicableForObjectType(const QString& objectType) const override;

    protected:
        void refreshContent() override;

    private slots:
        void onTakeSnapshot();
        void onDeleteSnapshot();
        void onRevertToSnapshot();
        void onSnapshotSelectionChanged();
        void refreshSnapshotList();
        void onVirtualMachinesDataUpdated(QVariantList vms);
        void onCacheObjectChanged(XenConnection *connection, const QString& type, const QString& ref);
        void onSnapshotContextMenu(const QPoint& pos);

    private:
        void removeObject() override;
        void updateObject() override;

        Ui::SnapshotsTabPage* ui;
        void populateSnapshotTree();
        void updateButtonStates();
};

#endif // SNAPSHOTSTABPAGE_H
