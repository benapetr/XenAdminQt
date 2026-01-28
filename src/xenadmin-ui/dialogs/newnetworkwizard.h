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

#ifndef NEWNETWORKWIZARD_H
#define NEWNETWORKWIZARD_H

#include <QWizard>
#include <QPointer>
#include <QSet>
#include <QSharedPointer>
#include "ui_newnetworkwizard.h"

class XenConnection;
class Host;
class Pool;
class Network;
class PIF;
class WizardNavigationPane;

class NewNetworkWizard : public QWizard
{
    Q_OBJECT

public:
    enum class NetworkType
    {
        Internal,
        CHIN,
        External,
        Bonded,
        SRIOV
    };

    enum PageId
    {
        Page_TypeSelect = 0,
        Page_Name = 1,
        Page_Details = 2,
        Page_BondDetails = 3,
        Page_ChinDetails = 4,
        Page_SriovDetails = 5
    };

    explicit NewNetworkWizard(XenConnection* connection,
                              const QSharedPointer<Pool>& pool,
                              const QSharedPointer<Host>& host,
                              QWidget* parent = nullptr);
    ~NewNetworkWizard() override;

protected:
    void initializePage(int id) override;
    bool validateCurrentPage() override;
    void accept() override;
    int nextId() const override;

private slots:
    void onNetworkTypeChanged();
    void onNameChanged();
    void onDetailsInputsChanged();
    void onChinSelectionChanged();
    void onSriovSelectionChanged();

private:
    void setupWizardUi();
    void configurePages();
    void updateNavigationSteps();
    void updateNavigationSelection();
    void applyNetworkTypeFlow();
    void updateTypePage();
    void updateNamePage();
    void updateDetailsPage();
    void updateBondDetailsPage();
    void updateChinDetailsPage();
    void updateSriovDetailsPage();

    bool validateNamePage();
    bool validateDetailsPage();
    bool validateBondDetailsPage();
    bool validateChinDetailsPage();
    bool validateSriovDetailsPage();

    NetworkType selectedNetworkType() const;
    QString defaultNetworkName(NetworkType type) const;
    QStringList existingNetworkNames() const;
    QString makeUniqueName(const QString& base, const QStringList& existing) const;

    void populateExternalNics();
    void populateChinInterfaces();
    void populateSriovNics();

    void updateVlanValidation();
    void updateMtuValidation();

    bool vlanValueIsValid(QString* warningText, bool* isError) const;
    bool mtuValueIsValid(QString* warningText, bool* isError) const;

    QSharedPointer<Host> coordinatorHost() const;
    QSharedPointer<Pool> poolObject() const;

    Ui::NewNetworkWizard ui;
    WizardNavigationPane* m_navigationPane = nullptr;

    XenConnection* m_connection = nullptr;
    QSharedPointer<Pool> m_pool;
    QSharedPointer<Host> m_host;

    QSet<QString> m_existingNetworkNames;
    NetworkType m_cachedType = NetworkType::External;
    bool m_vlanError = false;
    bool m_mtuError = false;
    bool m_populatingNics = false;
};

#endif // NEWNETWORKWIZARD_H
