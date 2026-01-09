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

#ifndef NEWVIRTUALDISKDIALOG_H
#define NEWVIRTUALDISKDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class VM;

namespace Ui
{
    class NewVirtualDiskDialog;
}

class XenConnection;

class NewVirtualDiskDialog : public QDialog
{
    Q_OBJECT

    public:
        enum class DialogMode
        {
            Add,
            Edit
        };

        explicit NewVirtualDiskDialog(XenConnection* connection, const QString& vmRef, QWidget* parent = nullptr);
        explicit NewVirtualDiskDialog(QSharedPointer<VM> vm, QWidget* parent = nullptr);
        ~NewVirtualDiskDialog();

        void setDialogMode(DialogMode mode);
        void setWizardContext(const QString& vmName, const QStringList& usedDevices, const QString& homeHostRef);
        void setInitialDisk(const QString& name,
                            const QString& description,
                            qint64 sizeBytes,
                            const QString& srRef);
        void setMinSizeBytes(qint64 minSizeBytes);
        void setCanResize(bool canResize);

        QString getVDIName() const;
        QString getVDIDescription() const;
        QString getSelectedSR() const;
        qint64 getSize() const;
        QString getDevicePosition() const;
        QString getMode() const; // "RO" or "RW"
        bool isBootable() const;

    private slots:
        void onSRChanged();
        void onSizeChanged(double value);
        void onRescanClicked();
        void validateAndAccept();

    private:
        void init();
        void populateSRList();
        void validateInput();
        int findNextAvailableDevice() const;
        void updateDefaultName();
        void applyInitialDisk();

        Ui::NewVirtualDiskDialog* ui;
        XenConnection* m_connection;
        QString m_vmRef;
        QSharedPointer<VM> m_vm;
        QVariantMap m_vmData;
        QString m_homeHostRef;
        QString m_vmNameOverride;
        QStringList m_usedDevices;
        QString m_initialName;
        QString m_initialDescription;
        QString m_initialSrRef;
        qint64 m_initialSizeBytes = 0;
        qint64 m_minSizeBytes = 0;
        bool m_canResize = true;
        DialogMode m_mode = DialogMode::Add;
};

#endif // NEWVIRTUALDISKDIALOG_H
