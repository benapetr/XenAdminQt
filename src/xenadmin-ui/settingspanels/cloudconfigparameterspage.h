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

// C# equivalent: xenadmin/XenAdmin/Wizards/NewVMWizard/Page_CloudConfigParameters.cs (its IEditPage side)

#ifndef CLOUDCONFIGPARAMETERSPAGE_H
#define CLOUDCONFIGPARAMETERSPAGE_H

#include "ieditpage.h"

namespace Ui
{
    class CloudConfigParametersPage;
}

class VDI;
class VM;

//! Edits the cloud-config drive of a CoreOS VM or template through the xscontainer host plugin
class CloudConfigParametersPage : public IEditPage
{
    Q_OBJECT

    public:
        explicit CloudConfigParametersPage(QWidget* parent = nullptr);
        ~CloudConfigParametersPage() override;

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
        //! C# GetCloudConfigParameters: the drive's configuration, or the template's default without a drive
        void getCloudConfigParameters(QSharedPointer<VDI> configDrive);
        void showHideButtonsAndWarnings();
        QString configDriveTemplateText() const;

        Ui::CloudConfigParametersPage* ui;
        QSharedPointer<VM> m_vm;
        QString m_existingConfig;
        bool m_loading = false;
        bool m_errorRetrievingConfigParameters = false;
};

#endif // CLOUDCONFIGPARAMETERSPAGE_H
