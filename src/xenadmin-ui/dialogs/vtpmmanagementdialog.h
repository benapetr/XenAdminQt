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

#ifndef VTPMMANAGEMENTDIALOG_H
#define VTPMMANAGEMENTDIALOG_H

#include <QDialog>
#include <QHash>
#include <QPointer>
#include <QSharedPointer>
#include "xenlib/xen/xenobjecttype.h"

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class VM;
class VTPM;
class XenConnection;

/**
 * @brief Attach, remove and inspect the vTPMs of a VM
 *
 * Qt port of C# VtpmManagementDialog and VtpmManagementPage: the vTPMs are listed on the left,
 * the selected one's properties and Remove button on the right.
 */
class VtpmManagementDialog : public QDialog
{
    Q_OBJECT

    public:
        //! One dialog per VM, as C# ShowPerXenObject: an open one is brought to the front
        static void ShowForVM(const QSharedPointer<VM>& vm, QWidget* parent);

    private:
        explicit VtpmManagementDialog(const QSharedPointer<VM>& vm, QWidget* parent);

        void refresh();
        void updateDetails();
        QSharedPointer<VTPM> selectedVtpm() const;
        bool canRemoveVtpm(const QSharedPointer<VTPM>& vtpm, QString* cannotReason) const;
        void addVtpm();
        void removeVtpm();
        void onCacheItemChanged(XenConnection* connection, XenObjectType type, const QString& ref);

        static QString key(const QSharedPointer<VM>& vm);
        static QHash<QString, QPointer<VtpmManagementDialog>> s_openDialogs;

        QSharedPointer<VM> m_vm;
        QListWidget* m_vtpmList = nullptr;
        QPushButton* m_addButton = nullptr;
        QStackedWidget* m_pages = nullptr;
        QLabel* m_protectedValue = nullptr;
        QLabel* m_uniqueValue = nullptr;
        QPushButton* m_removeButton = nullptr;
};

#endif // VTPMMANAGEMENTDIALOG_H
