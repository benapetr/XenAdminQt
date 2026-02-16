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


#ifndef GPUEDITPAGE_H
#define GPUEDITPAGE_H

#include "ieditpage.h"

class VM;
class QTableWidget;
class QLabel;
class QPushButton;
class VGPU;
class XenConnection;

class GpuEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit GpuEditPage(QWidget* parent = nullptr);
        ~GpuEditPage() override = default;

        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;

        void SetXenObjects(const QString& objectRef,
                           const QString& objectType,
                           const QVariantMap& objectDataBefore,
                           const QVariantMap& objectDataCopy) override;

        AsyncOperation* SaveSettings() override;
        bool IsValidToSave() const override;
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;
        void Cleanup() override;
        bool HasChanged() const override;
        QVariantList GetVGpuData() const;

    private slots:
        void onAddGpuClicked();
        void onRemoveGpuClicked();
        void onSelectionChanged();
        void onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref);
        void onCacheObjectRemoved(XenConnection* connection, const QString& type, const QString& ref);
        void onCacheBulkUpdateComplete(const QString& type, int count);
        void onCacheCleared();

    private:
        struct RowData
        {
            QString opaqueRef;
            QString gpuGroupRef;
            QString typeRef;
            QString device;
            QString displayType;
        };

        void clearRows();
        void populateRowsFromVm();
        void addRow(const RowData& row);
        QVariantList buildVgpuDataForSave() const;
        QList<QSharedPointer<VGPU>> existingVgpusForDialog() const;
        QString normalizedStateKey() const;
        void updateEnablement();
        void connectCacheSignals();
        void disconnectCacheSignals();
        void applyCacheRefreshIfNeeded(const QString& type, const QString& ref = QString());

        QSharedPointer<VM> m_vm;
        QString m_originalStateKey;
        QList<RowData> m_rows;
        bool m_waitingForCacheSync = false;

        QLabel* m_infoLabel = nullptr;
        QTableWidget* m_table = nullptr;
        QPushButton* m_addButton = nullptr;
        QPushButton* m_removeButton = nullptr;
};

#endif // GPUEDITPAGE_H
