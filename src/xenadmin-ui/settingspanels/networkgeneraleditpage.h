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

#ifndef NETWORKGENERALEDITPAGE_H
#define NETWORKGENERALEDITPAGE_H

#include "ieditpage.h"
#include <QVariantMap>

namespace Ui {
class NetworkGeneralEditPage;
}

class AsyncOperation;

/**
 * @brief Network-specific settings edit page
 *
 * Qt equivalent of C# XenAdmin.SettingsPanels.EditNetworkPage
 * Allows editing network-specific properties: NIC, VLAN, MTU,
 * automatic add to VMs, and Bond mode settings.
 *
 * NOTE: This is the 3rd tab "Network Settings" in Network Properties dialog.
 * The 1st tab "General" (name, description, tags, folder) uses GeneralEditPage (universal).
 *
 * Features (matching C# EditNetworkPage):
 * - NIC selection (internal vs external NICs)
 * - VLAN configuration
 * - MTU (jumbo frames) configuration
 * - "Automatically add to new VMs" toggle
 * - Bond mode selection (balance-slb, active-backup, LACP with hashing)
 * - Warnings for management interfaces and attached VMs
 * - SR-IOV network detection
 *
 * TODO: Add SR-IOV-specific settings page when needed. Current implementation
 * detects SR-IOV networks but doesn't provide SR-IOV-specific configuration UI.
 * See C# for whether a separate SR-IOV page is needed.
 *
 * C# Reference: xenadmin/XenAdmin/SettingsPanels/EditNetworkPage.cs
 */
class NetworkGeneralEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit NetworkGeneralEditPage(QWidget* parent = nullptr);
        ~NetworkGeneralEditPage() override;

        // IEditPage interface
        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;

        void SetXenObject(QSharedPointer<XenObject> object,
                          const QVariantMap& objectDataBefore,
                          const QVariantMap& objectDataCopy) override;

        AsyncOperation* SaveSettings() override;
        QVariantMap GetModifiedObjectData() const override;
        bool HasChanged() const override;
        bool IsValidToSave() const override;
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;
        void Cleanup() override;

    private slots:
        void onNicSelectionChanged();
        void onVlanValueChanged();
        void onMtuValueChanged();
        void onBondModeChanged();

    private:
        void populateNicList();
        void updateBondModeVisibility();
        void updateMtuEnablement();
        void updateControlsEnablement();
        bool isSelectedInternal() const;
        bool willDisrupt() const;
        bool mtuHasChanged() const;
        bool bondModeHasChanged() const;
        bool hashingAlgorithmHasChanged() const;
        bool nicOrVlanHasChanged() const;
        bool isNicVlanEditable() const;
        QString getSelectedBondMode() const;
        QString getSelectedHashingAlgorithm() const;
        QString getNetworkPifRef() const;
        QString getPhysicalPifRef() const;
        QString findVirtualPifRefForSelection(const QString& nicName, int vlan) const;

        Ui::NetworkGeneralEditPage* ui;

        QString m_networkRef_;
        QVariantMap m_objectDataBefore_;
        QVariantMap m_objectDataCopy_;
        QString m_hostRef_;  // Coordinator host for network operations
        QString m_originalBondMode_;
        QString m_originalHashingAlgorithm_;
        bool m_runningVMsWithoutTools_;
};

#endif // NETWORKGENERALEDITPAGE_H
