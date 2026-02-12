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

#ifndef NETWORKINGPROPERTIESDIALOG_H
#define NETWORKINGPROPERTIESDIALOG_H

#include <QDialog>
#include <QSharedPointer>
#include <QList>
#include <QMap>

class PIF;
class Host;
class Pool;
class Network;
class NetworkingPropertiesPage;

namespace Ui
{
    class NetworkingPropertiesDialog;
}

class NetworkingPropertiesDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit NetworkingPropertiesDialog(QSharedPointer<Host> host, QSharedPointer<Pool> pool, QSharedPointer<PIF> selectedPif, QWidget* parent = nullptr);
        ~NetworkingPropertiesDialog();

        void accept() override;

    private slots:
        void onAddClicked();
        void onPageValidChanged();
        void onPageDeleteClicked();
        void onNetworkComboChanged();
        void onVerticalTabChanged(int index);

    private:
        void configure();
        void refreshButtons();
        void addTabContents(NetworkingPropertiesPage* page);
        void removePage(NetworkingPropertiesPage* page);
        void refreshNetworkComboBoxes();
        QMap<QString, QList<NetworkingPropertiesPage*>> makeProposedInUseMap() const;
        QList<QSharedPointer<PIF>> getKnownPifs(bool includeInvisible);
        QSharedPointer<PIF> findPifForHost(const QSharedPointer<Network>& network) const;
        QString managementNetworkRef() const;
        QString purposeForPif(const QSharedPointer<PIF>& pif) const;
        QString makeAuxTabName() const;
        void collateChanges(NetworkingPropertiesPage* page, const QSharedPointer<PIF>& oldPif, QList<QPair<QSharedPointer<PIF>, bool>>& newPifs, QList<QPair<QSharedPointer<PIF>, bool>>& newNamePifs, QMap<QString, QVariantMap>& updatedPifs);
        bool ipAddressSettingsChanged(const QSharedPointer<PIF>& pif1, const QSharedPointer<PIF>& pif2) const;
        bool managementInterfaceIpChanged(const QSharedPointer<PIF>& oldManagement, const QSharedPointer<PIF>& newManagement) const;

        Ui::NetworkingPropertiesDialog* ui;
        QSharedPointer<Host> m_host;
        QSharedPointer<Pool> m_pool;
        QSharedPointer<PIF> m_selectedPif;
        QList<NetworkingPropertiesPage*> m_pages;
        QList<QSharedPointer<PIF>> m_shownPifs;
        QList<QSharedPointer<PIF>> m_allPifs;
        QList<QSharedPointer<Network>> m_networks;
        QMap<QString, QList<NetworkingPropertiesPage*>> m_inUseMap;
        bool m_allowManagementOnVlan = true;
};

#endif // NETWORKINGPROPERTIESDIALOG_H
