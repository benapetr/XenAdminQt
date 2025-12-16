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

#ifndef NETWORKTABPAGE_H
#define NETWORKTABPAGE_H

#include "basetabpage.h"
#include <QVariantMap>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class NetworkTabPage;
}
QT_END_NAMESPACE

/**
 * Network tab page showing network configuration and interfaces.
 * Matches C# XenAdmin NetworkPage.cs with two sections:
 * 1. Networks - shows all networks with PIFs (Name, Description, NIC, VLAN, Auto, Link Status, MAC, MTU, SR-IOV)
 * 2. IP Address Configuration - shows management interfaces with IP settings
 *
 * Applicable to Hosts and Pools.
 */
class NetworkTabPage : public BaseTabPage
{
    Q_OBJECT

public:
    explicit NetworkTabPage(QWidget* parent = nullptr);
    ~NetworkTabPage();

    // Override setXenLib to connect to signals
    void setXenLib(XenLib* xenLib) override;

    QString tabTitle() const override
    {
        // C# uses context-specific titles: "Pool Networks", "Server Networks", "Virtual Network Interfaces"
        // For simplicity in Qt, we use "Networking" to match C# tab name pattern
        return "Networking";
    }
    QString helpID() const override
    {
        return "TabPageNetwork";
    }
    bool isApplicableForObjectType(const QString& objectType) const override;

protected:
    void refreshContent() override;

private:
    Ui::NetworkTabPage* ui;

    // Column setup
    void setupVifColumns();     // For VMs: Device, MAC, Limit, Network, IP, Active
    void setupNetworkColumns(); // For Host/Pool: Name, Description, NIC, VLAN, etc.

    // Networks section population
    void populateVIFsForVM();
    void populateNetworksForHost();
    void populateNetworksForPool();
    void addNetworkRow(const QVariantMap& networkData);
    bool shouldShowNetwork(const QVariantMap& networkData);

    // IP Address Configuration section population
    void populateIPConfigForHost();
    void populateIPConfigForPool();
    void addIPConfigRow(const QVariantMap& pifData, const QVariantMap& hostData = QVariantMap());

    // Helper methods
    QString getSelectedNetworkRef() const;
    QString getSelectedPifRef() const;
    QString getSelectedVifRef() const;  // For VM network interfaces
    QVariantMap getSelectedVif() const; // Get full VIF record

    // PIF/Network helper methods (matching C# XenAPI extension methods)
    QString getPifLinkStatus(const QVariantMap& pifData) const;     // PIF.LinkStatusString()
    QString getNetworkLinkStatus(const QVariantMap& networkData) const; // Network.LinkStatusString()
    bool isPifPhysical(const QVariantMap& pifData) const;            // PIF.IsPhysical()
    bool networkCanUseJumboFrames(const QVariantMap& networkData) const; // Network.CanUseJumboFrames()
    QString getPifNetworkSriov(const QVariantMap& pifData) const;    // PIF.NetworkSriov()

    // Button enablement (matches C# UpdateEnablement)
    void updateButtonStates();

    // Context menu handlers
    void showNetworksContextMenu(const QPoint& pos);
    void showIPConfigContextMenu(const QPoint& pos);
    void copyToClipboard();

    // Network operations (matches C# button click handlers)
    void onAddNetwork();     // AddNetworkButton_Click
    void onEditNetwork();    // EditNetworkButton_Click
    void onRemoveNetwork();  // RemoveNetworkButton_Click
    void onActivateToggle(); // buttonActivateToggle_Click
    void removeNetwork(const QString& networkRef);

private slots:
    void onConfigureClicked();
    void onNetworksTableSelectionChanged();
    void onIPConfigTableSelectionChanged();
    void onNetworksDataUpdated(const QVariantList& networks);
};

#endif // NETWORKTABPAGE_H
