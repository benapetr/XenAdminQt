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


#ifndef GPUTABPAGE_H
#define GPUTABPAGE_H

#include "basetabpage.h"
#include <QHash>
#include <QSharedPointer>

class QVBoxLayout;
class QWidget;
class GpuPlacementPolicyPanel;
class GpuRow;
class PGPU;
class Host;
class XenConnection;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class GpuTabPage;
}
QT_END_NAMESPACE

class GpuTabPage : public BaseTabPage
{
    Q_OBJECT

    public:
        explicit GpuTabPage(QWidget* parent = nullptr);
        ~GpuTabPage() override;

        QString GetTitle() const override
        {
            return tr("GPU");
        }

        Type GetType() const override
        {
            return Type::Gpu;
        }

        QString HelpID() const override
        {
            return QStringLiteral("TabPageGPU");
        }

        bool IsApplicableForObjectType(const QString& objectType) const override;
        void OnPageShown() override;
        void OnPageHidden() override;

    protected:
        void refreshContent() override;
        void removeObject() override;
        void updateObject() override;

    private slots:
        void onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref);
        void onCacheObjectRemoved(XenConnection* connection, const QString& type, const QString& ref);
        void onCacheBulkUpdateComplete(const QString& type, int count);
        void onCacheCleared();

    private:
        void rebuild();
        bool shouldShowPlacementPolicyPanel() const;

        Ui::GpuTabPage* ui = nullptr;
        GpuPlacementPolicyPanel* m_policyPanel = nullptr;
        QWidget* m_noGpuLabelContainer = nullptr;
        QHash<QString, GpuRow*> m_rowsByPgpuRef;
};

#endif // GPUTABPAGE_H
