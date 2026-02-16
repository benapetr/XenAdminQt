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


#ifndef GPUROW_H
#define GPUROW_H

#include <QMap>
#include <QSharedPointer>
#include <QWidget>

class QLabel;
class QTableWidget;
class QGridLayout;
class QCheckBox;
class QPushButton;
class PGPU;
class XenObject;
class GpuShinyBar;

class GpuRow : public QWidget
{
    Q_OBJECT

    public:
        explicit GpuRow(const QSharedPointer<XenObject>& scopeObject, const QList<QSharedPointer<PGPU>>& pGpus, QWidget* parent = nullptr);

        QList<QSharedPointer<PGPU>> GetPGPUs() const
        {
            return this->m_pGpus;
        }

        void RefreshGpu(const QSharedPointer<PGPU>& pgpu);

    private slots:
        void onSelectAll();
        void onClearAll();
        void onEditClicked();
        void onSelectionChanged();

    private:
        void RebuildBars();
        void RepopulateAllowedTypes(const QSharedPointer<PGPU>& pgpu);

        QSharedPointer<XenObject> m_scopeObject;
        QList<QSharedPointer<PGPU>> m_pGpus;
        bool m_vgpuCapability = false;

        QLabel* m_nameLabel = nullptr;
        QTableWidget* m_allowedTypesGrid = nullptr;
        QWidget* m_barsContainer = nullptr;
        QGridLayout* m_barsLayout = nullptr;
        QWidget* m_multiSelectPanel = nullptr;
        QPushButton* m_selectAllButton = nullptr;
        QPushButton* m_clearAllButton = nullptr;
        QPushButton* m_editButton = nullptr;

        QMap<QString, GpuShinyBar*> m_barsByPgpuRef;
        QMap<QString, QCheckBox*> m_checkByPgpuRef;
};

#endif // GPUROW_H
