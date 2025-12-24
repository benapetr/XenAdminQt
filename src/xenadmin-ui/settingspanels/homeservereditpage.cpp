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

#include "homeservereditpage.h"
#include "ui_homeservereditpage.h"
#include "../controls/affinitypicker.h"
#include "../../xenlib/xen/network/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/xenapi/xenapi_VM.h"
#include "../../xenlib/xencache.h"
#include "../../xenlib/xen/actions/delegatedasyncoperation.h"
#include <QTableWidgetItem>

HomeServerEditPage::HomeServerEditPage(QWidget* parent) : IEditPage(parent), ui(new Ui::HomeServerEditPage)
{
    this->ui->setupUi(this);

    connect(this->ui->picker, &AffinityPicker::selectedAffinityChanged, this, &HomeServerEditPage::onSelectedAffinityChanged);
}

HomeServerEditPage::~HomeServerEditPage()
{
    delete this->ui;
}

QString HomeServerEditPage::text() const
{
    return tr("Home Server");
}

QString HomeServerEditPage::subText() const
{
    if (!ui->picker->validState())
        return tr("None defined");

    QString hostRef = ui->picker->selectedAffinityRef();
    if (hostRef.isEmpty())
        return tr("None defined");

    if (connection())
    {
        QVariantMap hostData = connection()->GetCache()->ResolveObjectData("host", hostRef);
        QString name = hostData.value("name_label").toString();
        if (!name.isEmpty())
            return name;
    }

    return tr("None defined");
}

QIcon HomeServerEditPage::image() const
{
    return QIcon(":/icons/server_home_16.png");
}

void HomeServerEditPage::setXenObjects(const QString& objectRef,
                                       const QString& objectType,
                                       const QVariantMap& objectDataBefore,
                                       const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectType);
    Q_UNUSED(objectDataCopy);

    m_vmRef = objectRef;

    // Get VM's current affinity
    m_originalAffinityRef = objectDataBefore.value("affinity").toString();

    ui->picker->setAutoSelectAffinity(false);
    ui->picker->setAffinity(connection(), m_originalAffinityRef, QString());
}

AsyncOperation* HomeServerEditPage::saveSettings()
{
    if (!hasChanged())
    {
        return nullptr;
    }

    // Determine new affinity
    QString newAffinityRef;
    QString selectedRef = ui->picker->selectedAffinityRef();
    newAffinityRef = selectedRef.isEmpty() ? QString("OpaqueRef:NULL") : selectedRef;

    auto* op = new DelegatedAsyncOperation(
        m_connection,
        tr("Change Home Server"),
        tr("Setting VM home server..."),
        [vmRef = m_vmRef, affinityRef = newAffinityRef](DelegatedAsyncOperation* self) {
            XenSession* session = self->connection()->GetSession();
            if (!session || !session->IsLoggedIn())
                throw std::runtime_error("No valid session");
            XenAPI::VM::set_affinity(session, vmRef, affinityRef);
        },
        this);
    op->addApiMethodToRoleCheck("VM.set_affinity");
    return op;
}

bool HomeServerEditPage::isValidToSave() const
{
    return ui->picker->validState();
}

void HomeServerEditPage::showLocalValidationMessages()
{
    // Could show message if static selected but no host chosen
}

void HomeServerEditPage::hideLocalValidationMessages()
{
    // Nothing to hide
}

void HomeServerEditPage::cleanup()
{
    // Nothing to clean up
}

bool HomeServerEditPage::hasChanged() const
{
    QString currentAffinityRef = ui->picker->selectedAffinityRef();
    if (currentAffinityRef.isEmpty())
        currentAffinityRef = "OpaqueRef:NULL";

    QString origRef = m_originalAffinityRef;
    if (origRef.isEmpty() || origRef == "OpaqueRef:NULL")
        origRef = "OpaqueRef:NULL";

    return origRef != currentAffinityRef;
}

void HomeServerEditPage::onSelectedAffinityChanged()
{
    emit populated();
}
