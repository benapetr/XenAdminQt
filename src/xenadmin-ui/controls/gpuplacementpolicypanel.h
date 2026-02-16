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


#ifndef GPUPLACEMENTPOLICYPANEL_H
#define GPUPLACEMENTPOLICYPANEL_H

#include <QWidget>
#include <QSharedPointer>

class QLabel;
class QPushButton;
class XenObject;
class GPUGroup;
class XenConnection;

namespace XenAPI
{
    enum class AllocationAlgorithm;
}

class GpuPlacementPolicyPanel : public QWidget
{
    Q_OBJECT

    public:
        explicit GpuPlacementPolicyPanel(QWidget* parent = nullptr);
        void SetXenObject(const QSharedPointer<XenObject>& object);
        void UnregisterHandlers();

        static QString AllocationAlgorithmText(XenAPI::AllocationAlgorithm algorithm);

    private slots:
        void onEditClicked();
        void onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref);
        void onCacheObjectRemoved(XenConnection* connection, const QString& type, const QString& ref);
        void onCacheCleared();

    private:
        void RegisterHandlers();
        void PopulatePage();

        QSharedPointer<XenObject> m_object;
        QLabel* m_policyLabel = nullptr;
        QPushButton* m_editButton = nullptr;
};

#endif // GPUPLACEMENTPOLICYPANEL_H
