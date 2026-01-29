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

#ifndef NETWORKINGPROPERTIESPAGE_H
#define NETWORKINGPROPERTIESPAGE_H

#include <QWidget>
#include <QMap>
#include <QSharedPointer>

class Network;
class PIF;

namespace Ui
{
    class NetworkingPropertiesPage;
}

class NetworkingPropertiesPage : public QWidget
{
    Q_OBJECT

    public:
        enum class PageType
        {
            Primary,
            PrimaryWithHA,
            Secondary
        };

        explicit NetworkingPropertiesPage(PageType type, QWidget* parent = nullptr);
        ~NetworkingPropertiesPage() override;

        PageType Type() const;
        QIcon TabIcon() const;
        QString TabText() const;
        QString SubText() const;

        void SetPool(bool pool);
        void SetHostCount(int hostCount);
        void SetPurpose(const QString& purpose);
        QString Purpose() const;
        bool NameValid() const;

        void SetPif(const QSharedPointer<PIF>& pif);
        QSharedPointer<PIF> Pif() const;
        void LoadFromPif(const QSharedPointer<PIF>& pif);
        void SetSelectedNetworkRef(const QString& ref);
        void SetDefaultsForNew();

        void RefreshNetworkComboBox(const QMap<QString, QList<NetworkingPropertiesPage*>>& inUseMap, const QString& managementNetworkRef, bool allowManagementOnVlan, const QList<QSharedPointer<Network>>& networks);
        void SelectFirstUnusedNetwork(const QList<QSharedPointer<Network>>& networks, const QMap<QString, QList<NetworkingPropertiesPage*>>& inUseMap);
        void SelectName();

        bool IsValid() const;
        bool ClusteringEnabled() const;

        QString SelectedNetworkRef() const;
        QString IPAddress() const;
        QString Netmask() const;
        QString Gateway() const;
        QString PreferredDNS() const;
        QString AlternateDNS1() const;
        QString AlternateDNS2() const;
        bool IsDhcp() const;

    signals:
        void ValidChanged();
        void DeleteButtonClicked();
        void NetworkComboBoxChanged();

    private slots:
        void onSomethingChanged();
        void onNetworkComboChanged();
        void onPurposeChanged();
        void onIPAddressChanged();

    private:
        void refreshButtons();
        void setDnsControlsVisible(bool visible);
        QString findOtherPurpose(const QString& networkRef) const;
        bool isOptionalIPAddress(const QString& value) const;
        bool isValidIPAddress(const QString& value) const;
        bool isValidNetmask(const QString& value) const;
        void disableControls(const QString& message);

        Ui::NetworkingPropertiesPage* ui;
        PageType m_type;
        bool m_pool = false;
        int m_hostCount = 1;
        QString m_purpose;
        bool m_valid = false;
        bool m_nameValid = true;
        bool m_clusteringEnabled = false;
        bool m_squelchNetworkComboChange = false;
        bool m_triggeringChange = false;
        QString m_inUseWarning;
        QString m_managementNetworkRef;
        QMap<QString, QList<NetworkingPropertiesPage*>> m_inUseMap;
        QSharedPointer<PIF> m_pif;
};

#endif // NETWORKINGPROPERTIESPAGE_H
