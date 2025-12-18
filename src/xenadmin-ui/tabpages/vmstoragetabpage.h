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

QT_BEGIN_NAMESPACE
namespace Ui
{
    class VMStorageTabPage;
}
QT_END_NAMESPACE

/**
 * Storage tab page showing storage-related information.
 * For VMs: Shows attached virtual disks and their properties.
 * For Hosts: Shows available storage repositories.
 * For SRs: Shows detailed storage repository information and VDIs.
 */
class VMStorageTabPage : public BaseTabPage
{
    Q_OBJECT

public:
    explicit VMStorageTabPage(QWidget* parent = nullptr);
    ~VMStorageTabPage();

    QString tabTitle() const override
    {
        // C# Reference: VMStoragePage.cs and SrStoragePage.cs both use "Virtual Disks"
        return "Virtual Disks";
    }
    QString helpID() const override
    {
        return "TabPageStorage";
    }
    bool isApplicableForObjectType(const QString& objectType) const override;

    void setXenObject(const QString& objectType, const QString& objectRef, const QVariantMap& objectData) override;

protected:
    void refreshContent() override;

private slots:
    void onDriveComboBoxChanged(int index);
    void onIsoComboBoxChanged(int index);
    void onEjectButtonClicked();
    void onNewCDDriveLinkClicked(const QString& link);
    void onObjectDataReceived(QString type, QString ref, QVariantMap data);

    // Storage table actions
    void onAddButtonClicked();
    void onAttachButtonClicked();
    void onRescanButtonClicked();
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

    void populateVMStorage();
    void populateSRStorage();

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
};

#endif // VMSTORAGETABPAGE_H
