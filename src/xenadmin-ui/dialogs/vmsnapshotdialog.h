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

#ifndef VMSNAPSHOTDIALOG_H
#define VMSNAPSHOTDIALOG_H

#include <QDialog>
#include <QVariantMap>

namespace Ui
{
    class VmSnapshotDialog;
}

class VM;

/**
 * @brief Dialog for creating VM snapshots
 *
 * This dialog allows the user to configure snapshot creation with:
 * - Name and description
 * - Snapshot type: disk-only, disk with quiesce (VSS), or disk+memory (checkpoint)
 *
 * Matches the C# VmSnapshotDialog implementation.
 */
class VmSnapshotDialog : public QDialog
{
    Q_OBJECT

    public:
        enum SnapshotType
        {
            DISK,           // Disk-only snapshot
            QUIESCED_DISK,  // Disk snapshot with quiesce (VSS)
            DISK_AND_MEMORY // Disk and memory snapshot (checkpoint)
        };

        explicit VmSnapshotDialog(QSharedPointer<VM> vm, QWidget* parent = nullptr);
        ~VmSnapshotDialog();

        QString snapshotName() const;
        QString snapshotDescription() const;
        SnapshotType snapshotType() const;

    private slots:
        void onNameChanged();
        void onDiskRadioToggled(bool checked);
        void onMemoryRadioToggled(bool checked);
        void onQuiesceCheckBoxToggled(bool checked);

    private:
        void setupDialog();
        void updateOkButton();
        void updateWarnings();
        bool canUseQuiesce() const;
        bool canUseCheckpoint() const;

        Ui::VmSnapshotDialog* ui;
        QSharedPointer<VM> m_vm;
};

#endif // VMSNAPSHOTDIALOG_H
