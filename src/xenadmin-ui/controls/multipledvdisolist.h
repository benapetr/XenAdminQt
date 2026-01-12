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

#ifndef MULTIPLEDVDISOLIST_H
#define MULTIPLEDVDISOLIST_H

#include <QWidget>
#include <QSharedPointer>
#include <QVariantMap>

namespace Ui
{
    class MultipleDvdIsoList;
}

class VM;
class VBD;
class XenConnection;

/**
 * @brief Multiple DVD/ISO list control for managing VM DVD drives
 *
 * Qt port of C# XenAdmin.Controls.MultipleDvdIsoList.
 * Allows users to view and select from multiple DVD/floppy drives attached to a VM.
 * Shows a dropdown for drive selection when multiple drives exist, or a simple label
 * for single drives. Includes eject functionality and "New CD drive" option.
 *
 * @note Depends on CDChanger control (not yet ported)
 */
class MultipleDvdIsoList : public QWidget
{
    Q_OBJECT

    public:
        explicit MultipleDvdIsoList(QWidget* parent = nullptr);
        ~MultipleDvdIsoList() override;

        void SetVM(const QSharedPointer<VM>& vm);
        QSharedPointer<VM> GetVM() const;

        // Designer browsable properties for customizing label/link colors
        void SetLabelSingleDvdForeColor(const QColor& color);
        QColor GetLabelSingleDvdForeColor() const;

        void SetLabelNewCdForeColor(const QColor& color);
        QColor GetLabelNewCdForeColor() const;

        void SetLinkLabelLinkColor(const QColor& color);
        QColor GetLinkLabelLinkColor() const;

    protected:
        void deregisterEvents();

    private slots:
        void onVmPropertyChanged();
        void onVbdPropertyChanged();
        void onCachePopulated();
        void onComboBoxDriveIndexChanged(int index);
        void onNewCdLabelClicked();
        void onLinkLabelEjectClicked();
        void onCreateDriveActionCompleted();

    private:
        /**
         * @brief Internal item class for combo box
         */
        class VbdCombiItem
        {
            public:
                QString name;
                QSharedPointer<VBD> vbd;

                VbdCombiItem(const QString& n, const QSharedPointer<VBD>& v) : name(n), vbd(v) {}

                QString toString() const { return this->name; }
        };

        void refreshDrives();
        void setupConnections();
        void updateCdChangerDrive(const QSharedPointer<VBD>& drive);

        Ui::MultipleDvdIsoList* ui;
        QSharedPointer<VM> m_vm;
        bool m_inRefresh = false;
        QList<QMetaObject::Connection> vbdConnections_;
        QMetaObject::Connection cacheConnection_;
        QMetaObject::Connection vmConnection_;
};

#endif // MULTIPLEDVDISOLIST_H
