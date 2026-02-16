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

#ifndef PERFMONALERTEDITPAGE_H
#define PERFMONALERTEDITPAGE_H

#include "ieditpage.h"
#include "xenlib/xen/xenobjecttype.h"
#include <QMap>
#include <QVariantMap>

class AsyncOperation;

namespace Ui
{
    class PerfmonAlertEditPage;
}

class PerfmonAlertEditPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit PerfmonAlertEditPage(QWidget* parent = nullptr);
        ~PerfmonAlertEditPage() override;

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

    private:
        struct AlertConfig
        {
            bool enabled = false;
            double threshold = 0.0;
            int durationSeconds = 0;
            int intervalSeconds = 0;
        };

        QMap<QString, AlertConfig> parsePerfmonDefinitions(const QString& perfmonXml) const;
        QString buildPerfmonXml(const QMap<QString, AlertConfig>& definitions) const;

        AlertConfig getAlert(const QMap<QString, AlertConfig>& definitions, const QString& perfmonName) const;
        void setAlert(QMap<QString, AlertConfig>& definitions, const QString& perfmonName, const AlertConfig& config) const;

        AlertConfig readCpuAlertFromUi() const;
        AlertConfig readNetworkAlertFromUi() const;
        AlertConfig readDiskAlertFromUi() const;
        AlertConfig readSrAlertFromUi() const;
        AlertConfig readMemoryAlertFromUi() const;
        AlertConfig readDom0AlertFromUi() const;

        void setCpuAlertToUi(const AlertConfig& config);
        void setNetworkAlertToUi(const AlertConfig& config);
        void setDiskAlertToUi(const AlertConfig& config);
        void setSrAlertToUi(const AlertConfig& config);
        void setMemoryAlertToUi(const AlertConfig& config);
        void setDom0AlertToUi(const AlertConfig& config);

        void configureVisibilityByObjectType();

        Ui::PerfmonAlertEditPage* ui;
        QString m_objectRef;
        QString m_objectTypeApiName;
        XenObjectType m_objectType = XenObjectType::Null;
        QVariantMap m_objectDataBefore;
        QVariantMap m_objectDataCopy;

        AlertConfig m_origCPUAlert;
        AlertConfig m_origNetworkAlert;
        AlertConfig m_origDiskAlert;
        AlertConfig m_origSrAlert;
        AlertConfig m_origMemoryAlert;
        AlertConfig m_origDom0Alert;
};

#endif // PERFMONALERTEDITPAGE_H
