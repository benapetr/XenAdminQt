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

// C# equivalent: xenadmin/XenAdmin/SettingsPanels/NRPEEditPage.cs and NRPEEditPage.CheckGroup.cs

#ifndef NRPEEDITPAGE_H
#define NRPEEDITPAGE_H

#include "ieditpage.h"
#include "xenlib/xen/actions/nrpe/nrpehostconfiguration.h"

namespace Ui
{
    class NRPEEditPage;
}

class QTableWidgetItem;

//! NRPE monitoring settings of a pool or standalone server, read and written through the nrpe host plugin
class NRPEEditPage : public IEditPage
{
    Q_OBJECT

    public:
        //! How a metric's thresholds are checked (C# CheckGroup and its subclasses)
        enum class CheckKind
        {
            Usage,     //!< percentages, warning below critical
            Free,      //!< percentages, warning above critical
            Dom0Load   //!< three comma-separated numbers, each warning at most its critical
        };

        explicit NRPEEditPage(QWidget* parent = nullptr);
        ~NRPEEditPage() override;

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

        //! C# CheckGroup.CheckValue: empty when valid, otherwise the error for the warning or critical value
        static bool CheckThresholds(CheckKind kind, const QString& warning, const QString& critical,
                                    QString* warningError, QString* criticalError);

        //! C# IsAllowHostsValid: empty when valid
        static QString AllowHostsError(const QString& allowHosts);

        //! C# PropertiesDialog: xapi 23.27.0 or later, and a local root or pool-admin session
        static bool IsAvailable(XenConnection* connection);

    private slots:
        void updateEnabledState();
        void validateChecks();

    private:
        struct CheckRow
        {
            QString name;
            CheckKind kind;
            QTableWidgetItem* warning;
            QTableWidgetItem* critical;
        };

        enum class RetrieveStatus { Retrieving, Successful, Failed };

        void addCheck(const QString& name, const QString& label, CheckKind kind);
        void setRetrieveStatus(RetrieveStatus status);
        void applyConfiguration();
        NRPEHostConfiguration currentConfiguration() const;
        bool checksValid() const;

        Ui::NRPEEditPage* ui;
        QList<CheckRow> m_checks;
        NRPEHostConfiguration m_original;
        bool m_retrieved = false;
        mutable QString m_validationMessage;
};

#endif // NRPEEDITPAGE_H
