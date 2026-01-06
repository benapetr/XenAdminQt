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

#include "logdestinationeditpage.h"
#include "ui_logdestinationeditpage.h"
#include "../../xenlib/xen/asyncoperation.h"
#include "../../xenlib/xen/network/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/api.h"
#include <QIcon>
#include <QToolTip>
#include <QDebug>

LogDestinationEditPage::LogDestinationEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::LogDestinationEditPage), m_validToSave(true)
{
    this->ui->setupUi(this);

    // Hostname/IP validation regex (matches C# pattern)
    // From C#: @"^[a-zA-Z0-9]([-a-zA-Z0-9]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([-a-zA-Z0-9]{0,61}[a-zA-Z0-9])?)*$"
    this->m_hostnameRegex.setPattern("^[a-zA-Z0-9]([-a-zA-Z0-9]{0,61}[a-zA-Z0-9])?(\\.[a-zA-Z0-9]([-a-zA-Z0-9]{0,61}[a-zA-Z0-9])?)*$");

    // Connect signals
    this->connect(this->ui->checkBoxRemote, &QCheckBox::toggled, this, &LogDestinationEditPage::onCheckBoxRemoteToggled);
    this->connect(this->ui->lineEditServer, &QLineEdit::textChanged, this, &LogDestinationEditPage::onServerTextChanged);

    // Install event filter to detect focus
    this->ui->lineEditServer->installEventFilter(this);
}

LogDestinationEditPage::~LogDestinationEditPage()
{
    delete this->ui;
}

bool LogDestinationEditPage::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == this->ui->lineEditServer && event->type() == QEvent::FocusIn)
    {
        this->onServerEditFocused();
    }
    return IEditPage::eventFilter(obj, event);
}

QString LogDestinationEditPage::GetText() const
{
    return tr("Log Destination");
}

QString LogDestinationEditPage::GetSubText() const
{
    if (this->ui->checkBoxRemote->isChecked())
    {
        QString server = this->remoteServer();
        if (!server.isEmpty())
        {
            return tr("Local and Remote: %1").arg(server);
        }
        return tr("Remote logging enabled");
    }
    return tr("Local only");
}

QIcon LogDestinationEditPage::GetImage() const
{
    // Matches C# Images.StaticImages.log_destination_16
    return QIcon(":/icons/log_destination_16.png");
}

void LogDestinationEditPage::SetXenObjects(const QString& objectRef,
                                           const QString& objectType,
                                           const QVariantMap& objectDataBefore,
                                           const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectType);

    this->m_hostRef = objectRef;
    this->m_objectDataCopy = objectDataCopy; // Store copy for modification

    this->repopulate();
}

void LogDestinationEditPage::repopulate()
{
    // Block signals while populating
    this->ui->checkBoxRemote->blockSignals(true);
    this->ui->lineEditServer->blockSignals(true);

    // Get syslog destination from host.logging["syslog_destination"]
    // C# equivalent: _host.GetSysLogDestination()
    QVariantMap logging = this->m_objectDataCopy.value("logging").toMap();
    this->m_originalLocation = logging.value("syslog_destination").toString();

    // If there's a destination, enable checkbox and show it
    this->ui->checkBoxRemote->setChecked(!this->m_originalLocation.isEmpty());
    this->ui->lineEditServer->setText(this->m_originalLocation);

    // Enable/disable server field based on checkbox
    this->ui->lineEditServer->setEnabled(this->ui->checkBoxRemote->isChecked());

    // Unblock signals
    this->ui->checkBoxRemote->blockSignals(false);
    this->ui->lineEditServer->blockSignals(false);

    this->revalidate();
}

QString LogDestinationEditPage::remoteServer() const
{
    return this->ui->lineEditServer->text().trimmed();
}

void LogDestinationEditPage::revalidate()
{
    // C# logic:
    // _validToSave = !checkBoxRemote.Checked ||
    //                !string.IsNullOrEmpty(RemoteServer) && regex.IsMatch(RemoteServer);

    if (!this->ui->checkBoxRemote->isChecked())
    {
        this->m_validToSave = true; // If not sending to remote, always valid
    } else
    {
        QString server = this->remoteServer();
        this->m_validToSave = !server.isEmpty() && this->m_hostnameRegex.match(server).hasMatch();
    }
}

AsyncOperation* LogDestinationEditPage::SaveSettings()
{
    if (!this->m_connection || this->m_hostRef.isEmpty() || !this->HasChanged())
    {
        return nullptr;
    }

    // Step 1: Modify objectDataCopy (similar to C# _host.SetSysLogDestination)
    // This modifies host.logging["syslog_destination"]
    QVariantMap logging = this->m_objectDataCopy.value("logging").toMap();

    if (this->ui->checkBoxRemote->isChecked())
    {
        logging["syslog_destination"] = this->remoteServer();
    } else
    {
        logging.remove("syslog_destination"); // null = remove
    }

    this->m_objectDataCopy["logging"] = logging;

    // Step 2: Return AsyncOperation that calls Host.syslog_reconfigure
    // C# equivalent: new DelegatedAsyncAction(..., delegate { Host.syslog_reconfigure(...) })

    // Create inline AsyncOperation (DelegatedAsyncAction pattern)
    class SyslogReconfigureOperation : public AsyncOperation
    {
    public:
        SyslogReconfigureOperation(XenConnection* conn, const QString& hostRef, QObject* parent = nullptr)
            : AsyncOperation(conn,
                             QObject::tr("Change Log Destination"),
                             QObject::tr("Changing log destination settings..."),
                             parent),
              m_hostRef(hostRef)
        {
            SetSuppressHistory(true);
        }

    protected:
        void run() override
        {
            if (!GetConnection() || !GetSession())
            {
                qWarning() << "SyslogReconfigureOperation: No connection or session";
                return;
            }

            SetPercentComplete(0);
            SetDescription(tr("Reconfiguring syslog..."));

            try
            {
                // Call Host.syslog_reconfigure(GetSession, host_ref)
                XenRpcAPI api(GetSession());

                QVariantList params;
                params << GetSession()->getSessionId() << m_hostRef;

                QByteArray request = api.BuildJsonRpcCall("host.syslog_reconfigure", params);
                QByteArray response = GetConnection()->SendRequest(request);

                // Parse response to check for errors
                api.ParseJsonRpcResponse(response);

                SetPercentComplete(100);
                SetDescription(tr("Log destination updated successfully"));

            } catch (const std::exception& e)
            {
                qWarning() << "SyslogReconfigureOperation failed:" << e.what();
                SetDescription(tr("Failed to reconfigure syslog: %1").arg(e.what()));
            }
        }

    private:
        QString m_hostRef;
    };

    return new SyslogReconfigureOperation(this->m_connection, this->m_hostRef, this);
}

bool LogDestinationEditPage::IsValidToSave() const
{
    return this->m_validToSave;
}

void LogDestinationEditPage::ShowLocalValidationMessages()
{
    if (!this->m_validToSave && this->ui->checkBoxRemote->isChecked())
    {
        // Show tooltip on server field
        QToolTip::showText(this->ui->lineEditServer->mapToGlobal(QPoint(0, this->ui->lineEditServer->height())),
                           tr("Please enter a valid hostname or IP address"),
                           this->ui->lineEditServer);
    }
}

void LogDestinationEditPage::HideLocalValidationMessages()
{
    QToolTip::hideText();
}

void LogDestinationEditPage::Cleanup()
{
    // Nothing to cleanup
}

bool LogDestinationEditPage::HasChanged() const
{
    // C# logic:
    // if (checkBoxRemote.Checked)
    //     return _origLocation != RemoteServer;
    // return !string.IsNullOrWhiteSpace(_origLocation);

    if (this->ui->checkBoxRemote->isChecked())
    {
        return this->m_originalLocation != this->remoteServer();
    }

    // If checkbox unchecked but there was an original location, it changed
    return !this->m_originalLocation.isEmpty();
}

void LogDestinationEditPage::onCheckBoxRemoteToggled(bool checked)
{
    this->ui->lineEditServer->setEnabled(checked);
    this->revalidate();
}

void LogDestinationEditPage::onServerTextChanged(const QString& text)
{
    Q_UNUSED(text);
    this->revalidate();
}

void LogDestinationEditPage::onServerEditFocused()
{
    // When user focuses on server field, auto-enable the checkbox
    // C# equivalent: ServerTextBox_Enter
    if (!this->ui->checkBoxRemote->isChecked())
    {
        this->ui->checkBoxRemote->setChecked(true);
    }
}
