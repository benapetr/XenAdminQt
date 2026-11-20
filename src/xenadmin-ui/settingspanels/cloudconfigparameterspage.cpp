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

#include "cloudconfigparameterspage.h"
#include "ui_cloudconfigparameterspage.h"
#include "xenlib/xen/actions/host/runpluginaction.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/vm.h"
#include <QDebug>
#include <QFontDatabase>
#include <QPointer>

CloudConfigParametersPage::CloudConfigParametersPage(QWidget* parent) : IEditPage(parent), ui(new Ui::CloudConfigParametersPage)
{
    this->ui->setupUi(this);
    this->ui->ConfigDriveTemplateTextBox->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    this->ui->warningsTable->setVisible(false);

    // C#: the checkbox and Reset to default are only for the New VM wizard
    this->ui->IncludeConfigDriveCheckBox->setVisible(false);
    this->ui->reloadDefaults->setVisible(false);
}

CloudConfigParametersPage::~CloudConfigParametersPage()
{
    delete this->ui;
}

QString CloudConfigParametersPage::GetText() const
{
    return tr("Cloud-Config Parameters");
}

QString CloudConfigParametersPage::GetSubText() const
{
    return this->ui->IncludeConfigDriveCheckBox->isChecked() ? tr("Config drive included") : tr("Config drive not included");
}

QIcon CloudConfigParametersPage::GetImage() const
{
    return QIcon(":/icons/coreos_16.png");
}

void CloudConfigParametersPage::SetXenObject(QSharedPointer<XenObject> object, const QVariantMap& objectDataBefore, const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectDataCopy);
    this->m_object = object;
    this->m_vm = qSharedPointerDynamicCast<VM>(object);
    if (!this->m_vm)
        return;

    this->getCloudConfigParameters(this->m_vm->CloudConfigDrive());
}

void CloudConfigParametersPage::getCloudConfigParameters(QSharedPointer<VDI> configDrive)
{
    QVariantMap parameters;
    const bool defaultConfig = configDrive.isNull();
    if (defaultConfig)
        parameters.insert("templateuuid", this->m_vm->GetUUID());
    else
        parameters.insert("vdiuuid", configDrive->GetUUID());

    // C# runs this synchronously; here the text box waits read-only until the plugin answers
    this->m_loading = true;
    this->showHideButtonsAndWarnings();

    auto* action = new RunPluginAction(this->m_vm->GetConnection(), Host::GetCoordinator(this->m_vm->GetConnection()),
                                       "xscontainer", defaultConfig ? "get_config_drive_default" : "get_config_drive_configuration",
                                       parameters, true);
    QPointer<CloudConfigParametersPage> self(this);
    connect(action, &AsyncOperation::completed, this, [self, action]()
    {
        action->deleteLater();
        if (!self)
            return;

        self->m_loading = false;
        self->m_errorRetrievingConfigParameters = action->IsFailed();
        if (!self->m_errorRetrievingConfigParameters)
        {
            self->m_existingConfig = action->GetResult();
            self->ui->ConfigDriveTemplateTextBox->setPlainText(self->m_existingConfig);
        } else
        {
            qWarning() << "Could not get the config drive parameters:" << action->GetErrorMessage();
        }
        self->showHideButtonsAndWarnings();
    });
    action->RunAsync();
}

void CloudConfigParametersPage::showHideButtonsAndWarnings()
{
    // C#: an existing VM's config can only be edited when it is a template or halted,
    // and only once its parameters have been read
    const bool canEdit = !this->m_loading && !this->m_errorRetrievingConfigParameters
                         && (this->m_vm->IsTemplate() || this->m_vm->GetPowerState() == "Halted");

    this->ui->ConfigDriveTemplateTextBox->setReadOnly(!canEdit);
    this->ui->warningsTable->setVisible(!this->m_loading && (this->m_errorRetrievingConfigParameters || !canEdit));
    this->ui->labelWarning->setText(this->m_errorRetrievingConfigParameters
                                        ? tr("The cloud-config parameters could not be retrieved.")
                                        : tr("The cloud-config parameters can only be changed when the VM is shut down."));
}

QString CloudConfigParametersPage::configDriveTemplateText() const
{
    return this->ui->IncludeConfigDriveCheckBox->isChecked() ? this->ui->ConfigDriveTemplateTextBox->toPlainText().trimmed() : QString();
}

AsyncOperation* CloudConfigParametersPage::SaveSettings()
{
    if (!this->m_vm)
        return nullptr;

    QSharedPointer<VDI> configDrive = this->m_vm->CloudConfigDrive();
    const QString configuration = this->configDriveTemplateText();
    if (!configDrive || configuration.isEmpty())
        return nullptr;

    QSharedPointer<SR> sr = configDrive->GetSR();
    if (!sr)
        return nullptr;

    QVariantMap parameters;
    parameters.insert("vmuuid", this->m_vm->GetUUID());
    parameters.insert("sruuid", sr->GetUUID());
    parameters.insert("configuration", configuration);

    QSharedPointer<Host> host = this->m_vm->GetHome();
    if (!host)
        host = Host::GetCoordinator(this->m_vm->GetConnection());
    return new RunPluginAction(this->m_vm->GetConnection(), host, "xscontainer", "create_config_drive", parameters, true);
}

bool CloudConfigParametersPage::IsValidToSave() const
{
    return true;
}

void CloudConfigParametersPage::ShowLocalValidationMessages()
{
}

void CloudConfigParametersPage::HideLocalValidationMessages()
{
}

void CloudConfigParametersPage::Cleanup()
{
}

bool CloudConfigParametersPage::HasChanged() const
{
    // C# compares the untrimmed plugin answer with the trimmed text, so a trailing newline
    // counted as a change; both sides are trimmed here
    return !this->m_loading && !this->m_errorRetrievingConfigParameters
           && this->m_existingConfig.trimmed() != this->configDriveTemplateText();
}
