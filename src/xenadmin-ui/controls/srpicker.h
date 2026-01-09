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

#ifndef SRPICKER_H
#define SRPICKER_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QList>
#include <QSharedPointer>

namespace Ui
{
    class SrPicker;
}

class XenConnection;
class SrRefreshAction;

/**
 * @brief SR picker control for selecting storage repositories
 *
 * Qt port of C# XenAdmin.Controls.SrPicker.
 * 
 * This control displays a list of storage repositories and allows the user to:
 * - Select a compatible SR for the current operation
 * - Rescan SRs to refresh their VDI lists
 * - Filter SRs based on operation type (Move, Migrate, Copy, etc.)
 *
 * Key features:
 * - Supports multiple picker types (VM, Move, Copy, Migrate, InstallFromTemplate, LunPerVDI)
 * - Handles SR scanning via SrRefreshAction (max 3 parallel scans per connection)
 * - Auto-selects default SR or preselected SR after scanning completes
 * - Shows SR status (size, free space, scanning status, error messages)
 * - Filters based on affinity, existing VDIs, and operation requirements
 *
 * C# Reference: XenAdmin/Controls/SrPicker.cs
 */
class SrPicker : public QWidget
{
    Q_OBJECT

    public:
        // Migrate is the live VDI move operation
        enum SRPickerType
        {
            VM,
            InstallFromTemplate,
            Move,
            Copy,
            Migrate,
            LunPerVDI
        };

        explicit SrPicker(QWidget* parent = nullptr);
        ~SrPicker() override;

        /**
         * @brief Populate the SR picker with storage repositories
         * @param usage The picker type (determines filtering logic)
         * @param connection The XenServer connection
         * @param affinityRef Host affinity reference (nullptr for no affinity)
         * @param preselectedSRRef SR to auto-select after scanning (nullptr for pool default)
         * @param existingVDIRefs VDI references for existing disks (for space calculation)
         */
        void Populate(SRPickerType usage, XenConnection* connection,
                      const QString& affinityRef,
                      const QString& preselectedSRRef,
                      const QStringList& existingVDIRefs);

        /**
         * @brief Scan all visible, non-scanning SRs to refresh their VDI lists
         *
         * C# equivalent: ScanSRs()
         *
         * This method:
         * 1. Iterates through all SR items
         * 2. Creates a SrRefreshAction for each non-scanning SR
         * 3. Queues actions and runs at most MAX_SCANS_PER_CONNECTION (3) in parallel
         * 4. Marks each SR as "Scanning..." until the action completes
         */
        void ScanSRs();

        /**
         * @brief Get the currently selected SR reference
         * @return Selected SR reference, or empty string if none selected
         */
        QString GetSelectedSR() const;

        /**
         * @brief Check if any SR can be scanned (not already scanning and not detached)
         * @return True if at least one SR can be scanned
         */
        bool CanBeScanned() const;

    signals:
        /**
         * @brief Emitted when the selected SR changes
         */
        void selectedIndexChanged();

        /**
         * @brief Emitted when a row is double-clicked
         */
        void doubleClickOnRow();

        /**
         * @brief Emitted when the canBeScanned() state changes
         */
        void canBeScannedChanged();

    private slots:
        void onSelectionChanged();
        void onItemDoubleClicked(int row, int column);
        void onCacheUpdated(XenConnection *connection, const QString &type, const QString &ref);
        void onCacheRemoved(XenConnection *connection, const QString& type, const QString& ref);
        void onSrRefreshCompleted();

    private:
        struct SRItem // TODO replace with port of picker item
        {
            QString ref;
            QString name;
            QString description;
            QString type;
            qint64 physicalSize;
            qint64 freeSpace;
            bool shared;
            bool scanning;
            bool enabled;
            QString disableReason;
        };

        void populateSRList();
        void addSR(const QString& srRef);
        void updateSRItem(const QString& srRef);
        void removeSR(const QString& srRef);
        bool isValidSR(const QVariantMap& srData) const;
        bool canBeEnabled(const QString& srRef, const QVariantMap& srData, QString& reason) const;
        qint64 calculateFreeSpace(const QVariantMap& srData) const;
        QString formatSize(qint64 bytes) const;
        void selectDefaultSR();
        void onCanBeScannedChanged();
        void startNextScan();

        // Validation methods (match C# SrPickerItem logic)
        bool isCurrentLocation(const QString& srRef) const;
        bool isBroken(const QString& srRef, const QVariantMap& srData) const;
        bool isDetached(const QString& srRef, const QVariantMap& srData) const;
        bool supportsVdiCreate(const QVariantMap& srData) const;
        bool supportsStorageMigration(const QVariantMap& srData) const;
        bool canBeSeenFromAffinity(const QString& srRef, const QVariantMap& srData) const;
        bool canFitDisks(const QVariantMap& srData) const;

        Ui::SrPicker* ui;
        XenConnection* m_connection;
        SRPickerType m_usage;
        QString m_affinityRef;
        QString m_preselectedSRRef;
        QString m_defaultSRRef;
        QStringList m_existingVDIRefs;
        QList<SRItem> m_srItems;

        // Scan queue management (C# _refreshQueue equivalent)
        static const int MAX_SCANS_PER_CONNECTION = 3;
        QList<SrRefreshAction*> m_refreshQueue;
        int m_runningScans;
};

#endif // SRPICKER_H
