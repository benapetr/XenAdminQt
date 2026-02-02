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

#ifndef VBDEDITPAGE_H
#define VBDEDITPAGE_H

#include "ieditpage.h"
#include <QSharedPointer>

namespace Ui
{
    class VBDEditPage;
}

class VBD;
class VDI;
class SR;
class VM;
namespace XenAPI
{
    class Session;
}

class VBDEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit VBDEditPage(QSharedPointer<VBD> vbd, QWidget* parent = nullptr);
        ~VBDEditPage() override;

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

        void UpdateDevicePositions(XenAPI::Session* session);

    private slots:
        void onInputsChanged();

    private:
        void repopulate();
        void updateSubText();
        QString selectedDevicePosition() const;
        QSharedPointer<VBD> findOtherVbdWithPosition(const QString& position) const;
        void warnSwapRequiresRestart(const QSharedPointer<VBD>& other) const;

        Ui::VBDEditPage* ui;
        QSharedPointer<VBD> m_vbd;
        QSharedPointer<VDI> m_vdi;
        QSharedPointer<SR> m_sr;
        QSharedPointer<VM> m_vm;
        bool m_validToSave;
        QString m_subText;
};

#endif // VBDEDITPAGE_H
