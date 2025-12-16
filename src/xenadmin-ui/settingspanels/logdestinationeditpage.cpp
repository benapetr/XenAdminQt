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
#include "../../xenlib/xen/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/api.h"
#include <QIcon>
#include <QToolTip>
#include <QDebug>

LogDestinationEditPage::LogDestinationEditPage(QWidget* parent)
    : IEditPage(parent), ui(new Ui::LogDestinationEditPage), m_validToSave(true)
{
    ui->setupUi(this);

    // Hostname/IP validation regex (matches C# pattern)
    // From C#: @"^[a-zA-Z0-9]([-a-zA-Z0-9]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([-a-zA-Z0-9]{0,61}[a-zA-Z0-9])?)*$"
    m_hostnameRegex.setPattern("^[a-zA-Z0-9]([-a-zA-Z0-9]{0,61}[a-zA-Z0-9])?(\\.[a-zA-Z0-9]([-a-zA-Z0-9]{0,61}[a-zA-Z0-9])?)*$");

    // Connect signals
    connect(ui->checkBoxRemote, &QCheckBox::toggled, this, &LogDestinationEditPage::onCheckBoxRemoteToggled);
    connect(ui->lineEditServer, &QLineEdit::textChanged, this, &LogDestinationEditPage::onServerTextChanged);

    // Install event filter to detect focus
    ui->lineEditServer->installEventFilter(this);
}

LogDestinationEditPage::~LogDestinationEditPage()
{
    delete ui;
}

bool LogDestinationEditPage::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == ui->lineEditServer && event->type() == QEvent::FocusIn)
    {
        onServerEditFocused();
    }
    return IEditPage::eventFilter(obj, event);
}

QString LogDestinationEditPage::text() const
{
    return tr("Log Destination");
}

QString LogDestinationEditPage::subText() const
{
    if (ui->checkBoxRemote->isChecked())
    {
        QString server = remoteServer();
        if (!server.isEmpty())
        {
            return tr("Local and Remote: %1").arg(server);
        }
        return tr("Remote logging enabled");
    }
    return tr("Local only");
}

QIcon LogDestinationEditPage::image() const
{
    // TODO: Use proper icon
    return QIcon(":/icons/log_16.png");
}

void LogDestinationEditPage::setXenObjects(const QString& objectRef,
                                           const QString& objectType,
                                           const QVariantMap& objectDataBefore,
                                           const QVariantMap& objectDataCopy)
{
    Q_UNUSED(objectDataBefore);
    Q_UNUSED(objectType);

    m_hostRef = objectRef;
    m_objectDataCopy = objectDataCopy; // Store copy for modification

    repopulate();
}

void LogDestinationEditPage::repopulate()
{
    // Block signals while populating
    ui->checkBoxRemote->blockSignals(true);
    ui->lineEditServer->blockSignals(true);

    // Get syslog destination from host.logging["syslog_destination"]
    // C# equivalent: _host.GetSysLogDestination()
    QVariantMap logging = m_objectDataCopy.value("logging").toMap();
    m_originalLocation = logging.value("syslog_destination").toString();

    // If there's a destination, enable checkbox and show it
    ui->checkBoxRemote->setChecked(!m_originalLocation.isEmpty());
    ui->lineEditServer->setText(m_originalLocation);

    // Enable/disable server field based on checkbox
    ui->lineEditServer->setEnabled(ui->checkBoxRemote->isChecked());

    // Unblock signals
    ui->checkBoxRemote->blockSignals(false);
    ui->lineEditServer->blockSignals(false);

    revalidate();
}

QString LogDestinationEditPage::remoteServer() const
{
    return ui->lineEditServer->text().trimmed();
}

void LogDestinationEditPage::revalidate()
{
    // C# logic:
    // _validToSave = !checkBoxRemote.Checked ||
    //                !string.IsNullOrEmpty(RemoteServer) && regex.IsMatch(RemoteServer);

    if (!ui->checkBoxRemote->isChecked())
    {
        m_validToSave = true; // If not sending to remote, always valid
    } else
    {
        QString server = remoteServer();
        m_validToSave = !server.isEmpty() && m_hostnameRegex.match(server).hasMatch();
    }
}

AsyncOperation* LogDestinationEditPage::saveSettings()
{
    if (!m_connection || m_hostRef.isEmpty() || !hasChanged())
    {
        return nullptr;
    }

    // Step 1: Modify objectDataCopy (similar to C# _host.SetSysLogDestination)
    // This modifies host.logging["syslog_destination"]
    QVariantMap logging = m_objectDataCopy.value("logging").toMap();

    if (ui->checkBoxRemote->isChecked())
    {
        logging["syslog_destination"] = remoteServer();
    } else
    {
        logging.remove("syslog_destination"); // null = remove
    }

    m_objectDataCopy["logging"] = logging;

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
            setSuppressHistory(true);
        }

    protected:
        void run() override
        {
            if (!connection() || !session())
            {
                qWarning() << "SyslogReconfigureOperation: No connection or session";
                return;
            }

            setPercentComplete(0);
            setDescription(tr("Reconfiguring syslog..."));

            try
            {
                // Call Host.syslog_reconfigure(session, host_ref)
                XenRpcAPI api(session());

                QVariantList params;
                params << session()->getSessionId() << m_hostRef;

                QByteArray request = api.buildJsonRpcCall("host.syslog_reconfigure", params);
                QByteArray response = connection()->sendRequest(request);

                // Parse response to check for errors
                api.parseJsonRpcResponse(response);

                setPercentComplete(100);
                setDescription(tr("Log destination updated successfully"));

            } catch (const std::exception& e)
            {
                qWarning() << "SyslogReconfigureOperation failed:" << e.what();
                setDescription(tr("Failed to reconfigure syslog: %1").arg(e.what()));
            }
        }

    private:
        QString m_hostRef;
    };

    return new SyslogReconfigureOperation(m_connection, m_hostRef, this);
}

bool LogDestinationEditPage::isValidToSave() const
{
    return m_validToSave;
}

void LogDestinationEditPage::showLocalValidationMessages()
{
    if (!m_validToSave && ui->checkBoxRemote->isChecked())
    {
        // Show tooltip on server field
        QToolTip::showText(ui->lineEditServer->mapToGlobal(QPoint(0, ui->lineEditServer->height())),
                           tr("Please enter a valid hostname or IP address"),
                           ui->lineEditServer);
    }
}

void LogDestinationEditPage::hideLocalValidationMessages()
{
    QToolTip::hideText();
}

void LogDestinationEditPage::cleanup()
{
    // Nothing to cleanup
}

bool LogDestinationEditPage::hasChanged() const
{
    // C# logic:
    // if (checkBoxRemote.Checked)
    //     return _origLocation != RemoteServer;
    // return !string.IsNullOrWhiteSpace(_origLocation);

    if (ui->checkBoxRemote->isChecked())
    {
        return m_originalLocation != remoteServer();
    }

    // If checkbox unchecked but there was an original location, it changed
    return !m_originalLocation.isEmpty();
}

void LogDestinationEditPage::onCheckBoxRemoteToggled(bool checked)
{
    ui->lineEditServer->setEnabled(checked);
    revalidate();
}

void LogDestinationEditPage::onServerTextChanged(const QString& text)
{
    Q_UNUSED(text);
    revalidate();
}

void LogDestinationEditPage::onServerEditFocused()
{
    // When user focuses on server field, auto-enable the checkbox
    // C# equivalent: ServerTextBox_Enter
    if (!ui->checkBoxRemote->isChecked())
    {
        ui->checkBoxRemote->setChecked(true);
    }
}
