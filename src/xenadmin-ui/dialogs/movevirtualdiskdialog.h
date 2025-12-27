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

#ifndef MOVEVIRTUALDISKDIALOG_H
#define MOVEVIRTUALDISKDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include "../controls/srpicker.h"

namespace Ui
{
    class MoveVirtualDiskDialog;
}

class XenLib;
class XenConnection;

/**
 * @brief Dialog for moving one or more VDIs to a different SR
 *
 * Qt equivalent of C# XenAdmin.Dialogs.MoveVirtualDiskDialog
 *
 * Allows the user to:
 * 1. Select a destination SR from a list of compatible SRs
 * 2. Rescan SRs to refresh the list
 * 3. Move one or more VDIs to the selected SR
 *
 * Features:
 * - Filters out incompatible SRs (same as source, read-only, etc.)
 * - Shows SR details (name, type, size, free space, shared status)
 * - Supports moving multiple VDIs in parallel (batch size: 3)
 * - Uses MoveVirtualDiskAction for each VDI
 *
 * C# Reference: XenAdmin/Dialogs/MoveVirtualDiskDialog.cs
 */
class MoveVirtualDiskDialog : public QDialog
{
    Q_OBJECT

    public:
        /**
         * @brief Constructor for single VDI move
         * @param xenLib XenLib instance
         * @param vdiRef VDI reference to move
         * @param parent Parent widget
         */
        explicit MoveVirtualDiskDialog(XenConnection* conn, const QString& vdiRef, QWidget* parent = nullptr);

        /**
         * @brief Constructor for multiple VDI move
         * @param xenLib XenLib instance
         * @param vdiRefs List of VDI references to move
         * @param parent Parent widget
         */
        explicit MoveVirtualDiskDialog(XenConnection* conn, const QStringList& vdiRefs, QWidget* parent = nullptr);

        ~MoveVirtualDiskDialog();

    protected slots:
        void onSRSelectionChanged();
        void onSRDoubleClicked();
        void onRescanButtonClicked();
        void onCanBeScannedChanged();
        virtual void onMoveButtonClicked();

    protected:
        /**
         * @brief Create and execute move/migrate actions for the VDI(s)
         * @param targetSRRef Target SR reference
         * @param targetSRName Target SR name
         *
         * This method is virtual to allow MigrateVirtualDiskDialog to override
         * and use MigrateVirtualDiskAction instead of MoveVirtualDiskAction.
         */
        virtual void createAndRunActions(const QString& targetSRRef, const QString& targetSRName);

        virtual SrPicker::SRPickerType srPickerType() const;
        void setupUI();
        void updateMoveButton();

    protected:
        Ui::MoveVirtualDiskDialog* ui;
        XenConnection* m_connection;
        QStringList m_vdiRefs;   // VDI(s) to move
};

#endif // MOVEVIRTUALDISKDIALOG_H
