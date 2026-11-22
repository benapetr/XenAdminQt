/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

// C# equivalent: xenadmin/XenAdmin/Dialogs/ScheduledSnapshots/ScheduledSnapshotsDialog.cs

#ifndef SCHEDULEDSNAPSHOTSDIALOG_H
#define SCHEDULEDSNAPSHOTSDIALOG_H

#include <QDialog>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QSharedPointer>

namespace Ui
{
    class ScheduledSnapshotsDialog;
}

class Pool;
class VMSS;
class XenObject;
class QTimer;

/**
 * Lists a pool's VM snapshot schedules with their next run times (in the user's and the
 * server's time zone) and last result, and creates, edits, enables, runs and deletes them.
 * The run history comes from the schedules' VMSS messages.
 */
class ScheduledSnapshotsDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit ScheduledSnapshotsDialog(QSharedPointer<Pool> pool, QWidget* parent = nullptr);
        ~ScheduledSnapshotsDialog() override;

        //! Shows the pool's dialog, raising it if it's already open (C# ShowPerConnectionWizard)
        static void ShowFor(QSharedPointer<Pool> pool, QWidget* parent);

    private slots:
        void loadPolicies();
        void refreshButtons();
        void refreshHistory();
        void onCacheObjectChanged(QSharedPointer<XenObject> object);
        void onHistoryCellClicked(int row, int column);
        void onNew();
        void onProperties();
        void onEnable();
        void onRunNow();
        void onDelete();
        void onToggleHistory();

    private:
        void fillPolicies();
        void refreshPoolTitle();
        QList<QSharedPointer<VMSS>> selectedPolicies() const;
        //! The schedule's VMSS messages, newest first
        QList<QVariantMap> messagesFor(const QSharedPointer<VMSS>& policy) const;

        Ui::ScheduledSnapshotsDialog* ui;
        QSharedPointer<Pool> m_pool;
        QTimer* m_reloadTimer;
        bool m_hasServerTime = false;
        QDateTime m_serverLocalTime;
        qint64 m_diffSeconds = 0;
        bool m_filling = false;
        QSet<QString> m_busy;              //!< schedules with an action running (C# PolicyRow.IsBusy)
        QSet<QString> m_expandedMessages;  //!< history rows showing their details
};

#endif // SCHEDULEDSNAPSHOTSDIALOG_H
