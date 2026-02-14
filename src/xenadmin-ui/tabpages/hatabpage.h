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

#ifndef HATABPAGE_H
#define HATABPAGE_H

#include "basetabpage.h"

class QTimer;
class QPoint;
class Pool;
class SR;
namespace Ui
{
    class HATabPage;
}

class HATabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit HATabPage(QWidget* parent = nullptr);
        ~HATabPage() override;

        QString GetTitle() const override
        {
            return "HA";
        }

        Type GetType() const override
        {
            return Type::Ha;
        }

        QString HelpID() const override
        {
            return "TabPageHA";
        }

        bool IsApplicableForObjectType(const QString& objectType) const override;
        void OnPageShown() override;
        void OnPageHidden() override;

    protected:
        void refreshContent() override;
        void removeObject() override;
        void updateObject() override;

    private slots:
        void onConfigureClicked();
        void onDisableClicked();
        void onHeartbeatTableContextMenuRequested(const QPoint& pos);
        void onCopyHeartbeatRows();
        void onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref);
        void onCacheObjectRemoved(XenConnection* connection, const QString& type, const QString& ref);
        void onCacheBulkUpdateComplete(const QString& type, int count);
        void onCacheCleared();
        void onOperationUpdated();
        void onHeartbeatInitializationElapsed();

    private:
        Ui::HATabPage* ui;
        QSharedPointer<Pool> getPool() const;
        QList<QSharedPointer<SR>> getHeartbeatSRs(const QSharedPointer<Pool>& pool) const;
        bool hasActiveHAAction(const QSharedPointer<Pool>& pool) const;
        void ensureHeartbeatInitializationTimer(const QSharedPointer<Pool>& pool);
        void rebuildHeartbeatTable(const QSharedPointer<Pool>& pool);
        void updateCommandButtonStates(const QSharedPointer<Pool>& pool);
        QString formatCurrentCapacity(const QSharedPointer<Pool>& pool) const;

        QTimer* m_heartbeatInitDelayTimer = nullptr;
        bool m_heartbeatInitDelayElapsed = false;
        QString m_heartbeatInitConnectionId;
};

#endif // HATABPAGE_H
