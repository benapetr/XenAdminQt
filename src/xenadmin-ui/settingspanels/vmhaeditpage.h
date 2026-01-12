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

#ifndef VMHAEDITPAGE_H
#define VMHAEDITPAGE_H

#include "ieditpage.h"
#include <QMap>
#include <QVariantMap>
#include <QPointer>

class AsyncOperation;
class QThread;

namespace Ui
{
    class VMHAEditPage;
}

/**
 * @brief VM High Availability settings page
 *
 * Matches C# XenAdmin.SettingsPanels.VMHAEditPage
 * Allows configuring:
 * - HA restart priority
 * - VM start order
 * - VM start delay
 */
class VMHAEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit VMHAEditPage(QWidget* parent = nullptr);
        ~VMHAEditPage() override;

        // IEditPage interface
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

    private:
        QString restartPriorityDisplay(const QString& priority) const;
        QString restartPriorityDescription(const QString& priority) const;
        QString normalizePriority(const QString& priority) const;
        QString selectedPriority() const;
        bool isRestartPriority(const QString& priority) const;
        bool vmHasVgpus() const;
        bool poolHasHAEnabled() const;
        bool isHaEditable() const;
        QString ellipsiseName(const QString& name, int maxChars) const;
        QVariantMap getPoolData() const;
        void refillPrioritiesComboBox();
        void updateEnablement();
        void updateNtolLabelsCalculating();
        void updateNtolLabelsSuccess();
        void updateNtolLabelsFailure();
        void startVmAgilityCheck();
        void startNtolUpdate();
        QMap<QString, QVariantMap> buildVmStartupOptions(bool includePriority) const;
        QVariantMap buildNtolConfig() const;

        Ui::VMHAEditPage* ui;
        QString m_vmRef;
        QVariantMap m_objectDataBefore;
        QVariantMap m_objectDataCopy;

        // Original values
        QString m_origRestartPriority;
        long m_origStartOrder = 0;
        long m_origStartDelay = 0;
        qint64 m_origNtol = 0;
        bool m_vmIsAgile = false;
        bool m_agilityKnown = false;
        bool m_ntolUpdateInProgress = false;
        qint64 m_ntol = -1;
        qint64 m_ntolMax = -1;
        int m_ntolRequestId = 0;
        QString m_poolRef;

    private slots:
        void onPriorityChanged();
        void onLinkActivated(const QString& link);
        void onCacheObjectChanged(XenConnection *connection, const QString& type, const QString& ref);
};

#endif // VMHAEDITPAGE_H
