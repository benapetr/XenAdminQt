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

// C# equivalent: xenadmin/XenAdmin/SettingsPanels/ClusteringEditPage.cs

#ifndef CLUSTERINGEDITPAGE_H
#define CLUSTERINGEDITPAGE_H

#include "ieditpage.h"

namespace Ui
{
    class ClusteringEditPage;
}

class Cluster;
class Network;
class Pool;

//! Enables or disables corosync clustering (needed for GFS2 storage) on a pool or standalone server
class ClusteringEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit ClusteringEditPage(QWidget* parent = nullptr);
        ~ClusteringEditPage() override;

        QString GetText() const override;
        QString GetSubText() const override;
        QIcon GetImage() const override;

        void SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy) override;
        AsyncOperation* SaveSettings() override;
        bool IsValidToSave() const override;
        void ShowLocalValidationMessages() override;
        void HideLocalValidationMessages() override;
        void Cleanup() override;
        bool HasChanged() const override;

    private slots:
        void onNetworkChanged();

    private:
        void loadNetworks(QSharedPointer<Cluster> cluster);
        //! C# NetworkComboBox.PopulateComboBox with ExcludeNetworksWithoutIpAddresses; selects selectRef if listed
        void populateNetworks(const QString& selectRef);
        void setPage();
        void disableControls(const QString& message);
        QSharedPointer<Network> selectedNetwork() const;

        Ui::ClusteringEditPage* ui;
        QSharedPointer<Pool> m_pool;
        bool m_clusteringEnabled = false;
};

#endif // CLUSTERINGEDITPAGE_H
