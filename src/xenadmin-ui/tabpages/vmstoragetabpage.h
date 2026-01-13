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

#ifndef VMSTORAGETABPAGE_H
#define VMSTORAGETABPAGE_H

#include "basetabpage.h"
#include <QSet>

class VM;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class VMStorageTabPage;
}
QT_END_NAMESPACE

/**
 * VM storage tab showing virtual disks attached to VMs along with CD/DVD management.
 * C# Equivalent: VMStoragePage
 */
class VMStorageTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit VMStorageTabPage(QWidget* parent = nullptr);
        ~VMStorageTabPage();

        QString GetTitle() const override
        {
            // Tab caption shown as "Storage" in original XenCenter client
            return "Storage";
        }
        Type GetType() const override
        {
            return Type::VmStorage;
        }
        QString HelpID() const override
        {
            return "TabPageStorage";
        }
        bool IsApplicableForObjectType(const QString& objectType) const override;

        void SetObject(QSharedPointer<XenObject> object) override;

    protected:
        void refreshContent() override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private slots:
        void onDriveComboBoxChanged(int index);
        void onIsoComboBoxChanged(int index);
        void onEjectButtonClicked();
        void onNewCDDriveLinkClicked(const QString& link);
        void onObjectDataReceived(QString type, QString ref, QVariantMap data);
        void onCacheObjectChanged(XenConnection *connection, const QString& type, const QString& ref);

        // Storage table actions
        void onAddButtonClicked();
        void onAttachButtonClicked();
        void onActivateButtonClicked();
        void onDeactivateButtonClicked();
        void onMoveButtonClicked();
        void onDetachButtonClicked();
        void onDeleteButtonClicked();
        void onEditButtonClicked();
        void onStorageTableCustomContextMenuRequested(const QPoint& pos);
        void onStorageTableSelectionChanged();
        void onStorageTableDoubleClicked(const QModelIndex& index);

    private:
        Ui::VMStorageTabPage* ui;
        QSharedPointer<VM> m_vm;

        void populateVMStorage();
        // CD/DVD drive management
        void refreshCDDVDDrives();
        void refreshISOList();
        void updateCDDVDVisibility();
        QStringList m_vbdRefs;   // References to CD/DVD VBDs
        QString m_currentVBDRef; // Currently selected VBD

        // Storage table button management
        void updateStorageButtons();
        QString getSelectedVBDRef() const;
        QString getSelectedVDIRef() const;
        QStringList getSelectedVBDRefs() const;
        QStringList getSelectedVDIRefs() const;

        bool canActivateVBD(const QVariantMap& vbdData,
                            const QVariantMap& vdiData,
                            const QVariantMap& vmData) const;
        bool canDeactivateVBD(const QVariantMap& vbdData,
                              const QVariantMap& vdiData,
                              const QVariantMap& vmData) const;
        void runVbdPlugOperations(const QStringList& vbdRefs, bool plug);
        void runDetachOperations(const QStringList& vdiRefs);
        void runDeleteOperations(const QStringList& vdiRefs);

        QSet<QString> m_storageVbdRefs;
        QSet<QString> m_storageVdiRefs;
};

#endif // VMSTORAGETABPAGE_H
