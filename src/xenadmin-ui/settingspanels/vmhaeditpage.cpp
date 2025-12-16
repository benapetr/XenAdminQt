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

#include "vmhaeditpage.h"
#include "ui_vmhaeditpage.h"
#include "../../xenlib/xen/asyncoperation.h"
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/api.h"

VMHAEditPage::VMHAEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::VMHAEditPage), m_origStartOrder(0), m_origStartDelay(0)
{
    ui->setupUi(this);
}

VMHAEditPage::~VMHAEditPage()
{
    delete ui;
}

QString VMHAEditPage::text() const
{
    return tr("High Availability");
}

QString VMHAEditPage::subText() const
{
    int index = ui->comboBoxRestartPriority->currentIndex();
    QString priority;
    switch (index)
    {
    case 0:
        priority = tr("Do not restart");
        break;
    case 1:
        priority = tr("Restart");
        break;
    case 2:
        priority = tr("Best-effort");
        break;
    default:
        priority = tr("Unknown");
        break;
    }

    return tr("Restart priority: %1").arg(priority);
}

QIcon VMHAEditPage::image() const
{
    return QIcon::fromTheme("system-reboot");
}

void VMHAEditPage::setXenObjects(const QString& objectRef,
                                 const QString& objectType,
                                 const QVariantMap& objectDataBefore,
                                 const QVariantMap& objectDataCopy)
{
    m_vmRef = objectRef;
    m_objectDataBefore = objectDataBefore;
    m_objectDataCopy = objectDataCopy;

    // Get HA restart priority
    m_origRestartPriority = objectDataBefore.value("ha_restart_priority", "").toString();
    int priorityIndex = getRestartPriorityIndex(m_origRestartPriority);
    ui->comboBoxRestartPriority->setCurrentIndex(priorityIndex);

    // Get start order
    m_origStartOrder = objectDataBefore.value("order", 0).toLongLong();
    ui->spinBoxStartOrder->setValue(m_origStartOrder);

    // Get start delay
    m_origStartDelay = objectDataBefore.value("start_delay", 0).toLongLong();
    ui->spinBoxStartDelay->setValue(m_origStartDelay);
}

AsyncOperation* VMHAEditPage::saveSettings()
{
    if (!hasChanged())
    {
        return nullptr;
    }

    // Update objectDataCopy
    QString newRestartPriority = getRestartPriorityString(ui->comboBoxRestartPriority->currentIndex());
    m_objectDataCopy["ha_restart_priority"] = newRestartPriority;
    m_objectDataCopy["order"] = (qlonglong) ui->spinBoxStartOrder->value();
    m_objectDataCopy["start_delay"] = (qlonglong) ui->spinBoxStartDelay->value();

    // Return inline AsyncOperation
    class VMHAOperation : public AsyncOperation
    {
    public:
        VMHAOperation(XenConnection* conn,
                      const QString& vmRef,
                      const QString& restartPriority,
                      long startOrder,
                      long startDelay,
                      QObject* parent)
            : AsyncOperation(conn, tr("Change HA Settings"),
                             tr("Changing high availability settings..."), parent),
              m_vmRef(vmRef), m_restartPriority(restartPriority), m_startOrder(startOrder), m_startDelay(startDelay)
        {}

    protected:
        void run() override
        {
            XenRpcAPI api(connection()->getSession());

            setPercentComplete(10);

            // Set HA restart priority
            QVariantList params1;
            params1 << connection()->getSessionId() << m_vmRef << m_restartPriority;
            QByteArray request1 = api.buildJsonRpcCall("VM.set_ha_restart_priority", params1);
            connection()->sendRequest(request1);

            setPercentComplete(40);

            // Set start order
            QVariantList params2;
            params2 << connection()->getSessionId() << m_vmRef << (qlonglong) m_startOrder;
            QByteArray request2 = api.buildJsonRpcCall("VM.set_order", params2);
            connection()->sendRequest(request2);

            setPercentComplete(70);

            // Set start delay
            QVariantList params3;
            params3 << connection()->getSessionId() << m_vmRef << (qlonglong) m_startDelay;
            QByteArray request3 = api.buildJsonRpcCall("VM.set_start_delay", params3);
            connection()->sendRequest(request3);

            setPercentComplete(100);
        }

    private:
        QString m_vmRef;
        QString m_restartPriority;
        long m_startOrder;
        long m_startDelay;
    };

    return new VMHAOperation(m_connection, m_vmRef,
                             getRestartPriorityString(ui->comboBoxRestartPriority->currentIndex()),
                             ui->spinBoxStartOrder->value(),
                             ui->spinBoxStartDelay->value(),
                             this);
}

bool VMHAEditPage::isValidToSave() const
{
    return true;
}

void VMHAEditPage::showLocalValidationMessages()
{
    // No validation needed
}

void VMHAEditPage::hideLocalValidationMessages()
{
    // No validation messages to hide
}

void VMHAEditPage::cleanup()
{
    // Nothing to clean up
}

bool VMHAEditPage::hasChanged() const
{
    QString currentPriority = getRestartPriorityString(ui->comboBoxRestartPriority->currentIndex());
    long currentOrder = ui->spinBoxStartOrder->value();
    long currentDelay = ui->spinBoxStartDelay->value();

    return (currentPriority != m_origRestartPriority ||
            currentOrder != m_origStartOrder ||
            currentDelay != m_origStartDelay);
}

QString VMHAEditPage::getRestartPriorityString(int index) const
{
    // Match C# VM.HaRestartPriority enum values
    switch (index)
    {
    case 0:
        return ""; // Do not restart (empty string in XenAPI)
    case 1:
        return "restart"; // Restart
    case 2:
        return "best-effort"; // Best-effort
    default:
        return "";
    }
}

int VMHAEditPage::getRestartPriorityIndex(const QString& priority) const
{
    if (priority.isEmpty() || priority == "0")
        return 0; // Do not restart
    if (priority == "restart" || priority == "1")
        return 1; // Restart
    if (priority == "best-effort")
        return 2; // Best-effort
    return 0;     // Default to do not restart
}
