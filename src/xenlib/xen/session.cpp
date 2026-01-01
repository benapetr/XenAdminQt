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

#include "session.h"
#include "network/connection.h"
#include "jsonrpcclient.h"
#include "failure.h"
#include <QtNetwork/QNetworkReply>
#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>

namespace XenAPI
{
    class Session::Private
    {
        public:
            XenConnection* connection = nullptr;
            bool loggedIn = false;
            QString sessionId;
            QString username;
            QString password; // Store password for session duplication
            QString lastError;
            QStringList lastErrorDescription;
            QNetworkReply* currentReply = nullptr;
            APIVersion apiVersion = APIVersion::UNKNOWN;
            bool ownsSessionToken = false;
            bool isDuplicate = false;

            QString createLoginJsonRpc(const QString& username, const QString& password);
            bool parseLoginResponse(const QByteArray& response, QString& sessionId);
    };

    Session::Session(XenConnection* connection, QObject* parent) : QObject(parent), d(new Private)
    {
        this->d->connection = connection;
        if (connection)
        {
            QObject::connect(connection, &QObject::destroyed, this, [this]() {
                this->d->connection = nullptr;
            });
        }
    }

    Session::~Session()
    {
        if (this->d->loggedIn || this->d->connection)
            this->Logout();
        delete this->d;
    }

    bool Session::Login(const QString& username, const QString& password)
    {
        this->d->lastError.clear();
        this->d->lastErrorDescription.clear();

        // Store credentials for this session (for duplication)
        this->d->username = username;
        this->d->password = password;

        // Ensure connection is available (this will connect if not connected)
        if (!this->d->connection)
        {
            this->d->lastError = "No connection object available";
            return false;
        }

        QString request = this->buildLoginRequest(username, password);

        // Send login request and get response (this may establish connection if needed)
        QByteArray response = this->d->connection->SendRequest(request.toUtf8());
        if (response.isEmpty())
        {
            this->d->lastError = "Failed to send login request or empty response";
            return false;
        }

        return this->parseLoginResponse(response);
    }

    void Session::Logout()
    {
        if (!this->d->connection && !this->d->loggedIn)
            return;

        this->LogoutWithoutDisconnect();

        if (this->d->connection)
            this->d->connection->Disconnect();
    }

    void Session::LogoutWithoutDisconnect()
    {
        if (!this->d->connection && !this->d->loggedIn)
            return;

        if (this->d->loggedIn && this->d->ownsSessionToken && !this->d->sessionId.isEmpty())
        {
            QByteArray requestData = this->buildLogoutRequest().toUtf8();
            this->d->connection->SendRequest(requestData);
        }

        bool wasLoggedIn = this->d->loggedIn;
        this->d->sessionId.clear();
        this->d->loggedIn = false;
        this->d->ownsSessionToken = false;

        if (wasLoggedIn)
            emit this->loggedOut();
    }

    bool Session::IsLoggedIn() const
    {
        return this->d->loggedIn;
    }

    QString Session::getSessionId() const
    {
        return this->d->sessionId;
    }

    QString Session::getUsername() const
    {
        return this->d->username;
    }

    QString Session::getPassword() const
    {
        return this->d->password;
    }

    QString Session::getLastError() const
    {
        return this->d->lastError;
    }

    QStringList Session::getLastErrorDescription() const
    {
        return this->d->lastErrorDescription;
    }

    XenConnection* Session::getConnection() const
    {
        return this->d->connection;
    }

    Session* Session::DuplicateSession(Session* originalSession, QObject* parent)
    {
        if (!originalSession || !originalSession->IsLoggedIn())
        {
            qWarning() << "XenSession::duplicateSession: Original session is null or not logged in";
            return nullptr;
        }

        // Get original connection details
        XenConnection* originalConn = originalSession->d->connection;
        if (!originalConn)
        {
            qWarning() << "XenSession::duplicateSession: Original session has no connection";
            return nullptr;
        }

        qDebug() << "XenSession::duplicateSession: Creating duplicate session to"
                 << originalConn->GetHostname() << ":" << originalConn->GetPort()
                 << "from" << originalSession->d->sessionId.left(20) + "...";

        // Create new connection to the same host with separate worker thread
        XenConnection* newConn = new XenConnection(parent);
        bool connected = newConn->ConnectToHost(originalConn->GetHostname(),
                                                originalConn->GetPort(),
                                                "", ""); // Empty credentials - we'll reuse session ID

        if (!connected)
        {
            qWarning() << "XenSession::duplicateSession: Failed to connect";
            delete newConn;
            return nullptr;
        }

        if (!newConn->IsConnected())
        {
            QEventLoop loop;
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(newConn, &XenConnection::connected, &loop, &QEventLoop::quit);
            QObject::connect(newConn, &XenConnection::error, &loop, &QEventLoop::quit);
            timer.start(10000);
            loop.exec();
            if (!newConn->IsConnected())
            {
                qWarning() << "XenSession::duplicateSession: Timed out waiting for duplicate connection";
                delete newConn;
                return nullptr;
            }
        }

        // Create new session with the duplicate connection
        Session* newSession = new Session(newConn, parent);
        newConn->SetSession(newSession);

        // Copy the session ID from original (this allows reusing authentication)
        newSession->d->sessionId = originalSession->d->sessionId;
        newSession->d->loggedIn = true; // Mark as logged in since we have valid session ID
        newSession->d->ownsSessionToken = false;
        newSession->d->isDuplicate = true;
        newSession->d->username = originalSession->d->username;
        newSession->d->password = originalSession->d->password;
        newSession->d->apiVersion = originalSession->d->apiVersion;

        qDebug() << "XenSession::duplicateSession: Duplicate session created with ID"
                 << newSession->d->sessionId.left(16) << "...";

        return newSession;
    }

    QByteArray Session::sendApiRequest(const QString& jsonRequest)
    {
        if (!this->d->connection || !this->d->loggedIn)
        {
            this->d->lastError = "Not connected or not logged in";
            return QByteArray();
        }

        // Send the JSON-RPC request through the connection
        QByteArray requestData = jsonRequest.toUtf8();
        QByteArray response = this->d->connection->SendRequest(requestData);

        if (response.isEmpty())
        {
            this->d->lastError = "Empty response from server";
            return QByteArray();
        }

        return response;
    }

    bool Session::ownsSessionToken() const
    {
        return this->d->ownsSessionToken;
    }

    void Session::setOwnsSessionToken(bool ownsToken)
    {
        this->d->ownsSessionToken = ownsToken;
    }

    void Session::detachConnection()
    {
        this->d->connection = nullptr;
    }

    QString Session::Private::createLoginJsonRpc(const QString& username, const QString& password)
    {
        // Create JSON-RPC request for session.login_with_password
        QVariantList params;
        params << username << password;

        QByteArray jsonRpc = Xen::JsonRpcClient::buildJsonRpcCall("session.login_with_password", params, 1);

        // qDebug() << "XenSession: Logging in with username:" << username;
        // qDebug() << "XenSession: JSON-RPC request:" << QString::fromUtf8(jsonRpc);

        return QString::fromUtf8(jsonRpc);
    }

    bool Session::Private::parseLoginResponse(const QByteArray& response, QString& sessionId)
    {
        // Parse JSON-RPC response
        QVariant result = Xen::JsonRpcClient::parseJsonRpcResponse(response);

        if (result.isNull())
        {
            qDebug() << "XenSession: Failed to parse login response:" << Xen::JsonRpcClient::lastError();
            return false;
        }

        // Session ID is returned as a string directly
        if (result.type() == QVariant::String)
        {
            sessionId = result.toString();
            // qDebug() << "XenSession: Got session ID:" << sessionId;
            return true;
        }

        qDebug() << "XenSession: Unexpected response type:" << result.typeName();
        return false;
    }

    bool Session::parseLoginResponse(const QByteArray& response)
    {
        QString sessionId;
        if (this->d->parseLoginResponse(response, sessionId))
        {
            this->d->sessionId = sessionId;
            this->d->loggedIn = true;
            this->d->ownsSessionToken = true;
            this->d->isDuplicate = false;
            this->d->lastErrorDescription.clear();
            qDebug() << "XenSession: Login successful, sessionId"
                     << this->d->sessionId.left(20) + "...";
            emit this->loginSuccessful();
            return true;
        } else
        {
            // Try to extract more specific error information from the response
            QString specificError = this->extractAuthenticationError(response);
            if (!specificError.isEmpty())
            {
                this->d->lastError = specificError;
            } else
            {
                this->d->lastError = "Authentication failed";
            }
            emit this->loginFailed(this->d->lastError);
            return false;
        }
    }

    QString Session::extractAuthenticationError(const QByteArray& response)
    {
        this->d->lastErrorDescription.clear();

        // Parse JSON-RPC error response
        QJsonDocument doc = QJsonDocument::fromJson(response);
        if (doc.isNull() || !doc.isObject())
        {
            return QString();
        }

        QJsonObject root = doc.object();

        // Check for JSON-RPC error object
        if (root.contains("error") && root["error"].isObject())
        {
            QJsonObject error = root["error"].toObject();

            // Check for HOST_IS_SLAVE error in message field (from JSON-RPC error response)
            // This happens when connecting to a pool slave instead of the master
            if (error.contains("message") && error["message"].toString() == "HOST_IS_SLAVE")
            {
                // Extract master address from data array
                if (error.contains("data") && error["data"].isArray())
                {
                    QJsonArray dataArray = error["data"].toArray();
                    if (!dataArray.isEmpty())
                    {
                        QString masterAddress = dataArray[0].toString();
                        qDebug() << "XenSession: HOST_IS_SLAVE detected, master is at:" << masterAddress;
                        // Signal that we need to redirect to the pool master
                        emit needsRedirectToMaster(masterAddress);
                        this->d->lastErrorDescription = QStringList()
                            << Failure::HOST_IS_SLAVE << masterAddress;
                        return QString("Redirecting to pool master: %1").arg(masterAddress);
                    }
                }
                this->d->lastErrorDescription = QStringList() << Failure::HOST_IS_SLAVE;
                return "Server is pool slave, master address not provided";
            }

            // Check for XenAPI error with ErrorDescription (used for other errors)
            if (error.contains("data") && error["data"].isObject())
            {
                QJsonObject data = error["data"].toObject();
                if (data.contains("ErrorDescription") && data["ErrorDescription"].isArray())
                {
                    QJsonArray errorDesc = data["ErrorDescription"].toArray();
                    if (!errorDesc.isEmpty())
                    {
                        QStringList description;
                        description.reserve(errorDesc.size());
                        for (const QJsonValue& val : errorDesc)
                            description.append(val.toString());
                        this->d->lastErrorDescription = description;

                        QString errorCode = description.first();
                        if (errorCode == Failure::SESSION_AUTHENTICATION_FAILED)
                        {
                            return "Authentication failed: Invalid username or password";
                        }
                        return QString("Server error: %1").arg(errorCode);
                    }
                }
            }

            // Fallback to error message
            if (error.contains("message"))
            {
                const QString errorMessage = error["message"].toString();
                if (!errorMessage.isEmpty())
                    this->d->lastErrorDescription = QStringList() << errorMessage;
                return QString("Server error: %1").arg(errorMessage);
            }
        }

        return QString();
    }

    QString Session::buildLoginRequest(const QString& username, const QString& password)
    {
        return this->d->createLoginJsonRpc(username, password);
    }

    QString Session::buildLogoutRequest()
    {
        if (this->d->sessionId.isEmpty())
        {
            return QString();
        }

        // Build JSON-RPC request for session.logout
        // C# signature: void session_logout(string session)
        QVariantList params;
        params << this->d->sessionId;

        QByteArray jsonRpcRequest = Xen::JsonRpcClient::buildJsonRpcCall("session.logout", params, 0);
        return QString::fromUtf8(jsonRpcRequest);
    }

    APIVersion Session::getAPIVersion() const
    {
        return this->d->apiVersion;
    }

    void Session::setAPIVersion(APIVersion version)
    {
        this->d->apiVersion = version;
    }

    bool Session::apiVersionMeets(APIVersion required) const
    {
        return APIVersionHelper::versionMeets(this->d->apiVersion, required);
    }
}
