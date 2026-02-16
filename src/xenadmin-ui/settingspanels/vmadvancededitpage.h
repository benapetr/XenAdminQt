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

#ifndef VMADVANCEDEDITPAGE_H
#define VMADVANCEDEDITPAGE_H

#include "ieditpage.h"
#include <QVariantMap>

class AsyncOperation;

namespace Ui
{
    class VMAdvancedEditPage;
}

/**
 * @brief VM advanced settings page (HVM only)
 *
 * Matches C# XenAdmin.SettingsPanels.VMAdvancedEditPage
 * Configures shadow memory multiplier for HVM VMs
 */
class VMAdvancedEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit VMAdvancedEditPage(QWidget* parent = nullptr);
        ~VMAdvancedEditPage() override;

        // IEditPage interface
        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;

        void SetXenObject(QSharedPointer<XenObject> object,
                          const QVariantMap& objectDataBefore,
                          const QVariantMap& objectDataCopy) override;

        AsyncOperation* SaveSettings() override;
        bool IsValidToSave() const override;
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;
        void Cleanup() override;
        bool HasChanged() const override;
        QVariantMap GetModifiedObjectData() const override;

    private slots:
        void onGeneralRadioToggled(bool checked);
        void onCitrixRadioToggled(bool checked);
        void onManualRadioToggled(bool checked);
        void onShadowMultiplierChanged(double value);

    private:
        double getCurrentShadowMultiplier() const;

        Ui::VMAdvancedEditPage* ui;
        QString m_vmRef;
        QString m_powerState;
        double m_originalShadowMultiplier;
        QVariantMap m_objectDataCopy;
        bool m_showCpsOptimisation;

        static constexpr double SHADOW_MULTIPLIER_GENERAL = 1.0;
        static constexpr double SHADOW_MULTIPLIER_CPS = 4.0;
};

#endif // VMADVANCEDEDITPAGE_H
