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

#include "vmenlightenmenteditpage.h"
#include "ui_vmenlightenmenteditpage.h"
#include "../../xenlib/xen/asyncoperation.h"
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/api.h"

VMEnlightenmentEditPage::VMEnlightenmentEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::VMEnlightenmentEditPage), m_originalEnlightened(false)
{
    ui->setupUi(this);
}

VMEnlightenmentEditPage::~VMEnlightenmentEditPage()
{
    delete ui;
}

QString VMEnlightenmentEditPage::text() const
{
    return tr("Enlightenment");
}

QString VMEnlightenmentEditPage::subText() const
{
    return ui->checkBoxEnlightenment->isChecked() ? tr("Enabled") : tr("Disabled");
}

QIcon VMEnlightenmentEditPage::image() const
{
    return QIcon(":/icons/dc_16.png");
}

void VMEnlightenmentEditPage::setXenObjects(const QString& objectRef,
                                            const QString& objectType,
                                            const QVariantMap& objectDataBefore,
                                            const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectType);
    Q_UNUSED(objectDataCopy);

    m_vmRef = objectRef;

    // Check if VM is enlightened
    // In C#, this checks for platform:device_id in VM.platform
    m_originalEnlightened = isVMEnlightened(objectDataBefore);
    ui->checkBoxEnlightenment->setChecked(m_originalEnlightened);
}

AsyncOperation* VMEnlightenmentEditPage::saveSettings()
{
    if (!hasChanged())
    {
        return nullptr;
    }

    bool enable = ui->checkBoxEnlightenment->isChecked();

    // Return inline AsyncOperation for enlightenment change
    class VMEnlightenmentOperation : public AsyncOperation
    {
    public:
        VMEnlightenmentOperation(XenConnection* conn,
                                 const QString& vmRef,
                                 bool enable,
                                 const QString& vmUuid,
                                 QObject* parent)
            : AsyncOperation(conn,
                             enable ? tr("Enable VM Enlightenment") : tr("Disable VM Enlightenment"),
                             enable ? tr("Enabling Windows guest enlightenment...") : tr("Disabling Windows guest enlightenment..."),
                             parent),
              m_vmRef(vmRef), m_vmUuid(vmUuid), m_enable(enable)
        {}

    protected:
        void run() override
        {
            XenRpcAPI api(connection()->getSession());

            setPercentComplete(30);

            // Get coordinator host
            // For now, we'll use a simplified approach
            // In full implementation, would query pool coordinator

            // Call host.call_plugin with xscontainer plugin
            QString action = m_enable ? "register" : "deregister";

            QVariantMap args;
            args["vmuuid"] = m_vmUuid;

            QVariantList params;
            params << connection()->getSessionId()
                   << "coordinator_host_ref" // TODO: Get actual coordinator host ref
                   << "xscontainer"
                   << action
                   << args;

            QByteArray request = api.buildJsonRpcCall("host.call_plugin", params);

            // NOTE: This will fail without proper coordinator host ref
            // Full implementation needs to query pool to get coordinator
            connection()->sendRequest(request);

            setPercentComplete(100);
        }

    private:
        QString m_vmRef;
        QString m_vmUuid;
        bool m_enable;
    };

    // Get VM UUID for plugin call
    QString vmUuid = ""; // TODO: Extract from objectDataBefore

    return new VMEnlightenmentOperation(m_connection, m_vmRef, enable, vmUuid, this);
}

bool VMEnlightenmentEditPage::isValidToSave() const
{
    return true;
}

void VMEnlightenmentEditPage::showLocalValidationMessages()
{
    // No validation needed
}

void VMEnlightenmentEditPage::hideLocalValidationMessages()
{
    // No validation messages
}

void VMEnlightenmentEditPage::cleanup()
{
    // Nothing to clean up
}

bool VMEnlightenmentEditPage::hasChanged() const
{
    return ui->checkBoxEnlightenment->isChecked() != m_originalEnlightened;
}

bool VMEnlightenmentEditPage::isVMEnlightened(const QVariantMap& vmData) const
{
    // Check if VM has enlightenment enabled
    // In C#: vm.IsEnlightened() checks for platform["device_id"]
    QVariantMap platform = vmData.value("platform").toMap();
    return platform.contains("device_id");
}
