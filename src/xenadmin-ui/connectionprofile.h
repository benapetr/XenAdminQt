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

#ifndef CONNECTIONPROFILE_H
#define CONNECTIONPROFILE_H

#include <QString>
#include <QVariantMap>
#include <QStringList>

class ConnectionProfile
{
public:
    ConnectionProfile();
    ConnectionProfile(const QString& name, const QString& hostname, int port = 443,
                      const QString& username = QString(), bool rememberPassword = false);

    // Getters
    QString name() const
    {
        return m_name;
    }
    QString hostname() const
    {
        return m_hostname;
    }
    int port() const
    {
        return m_port;
    }
    QString username() const
    {
        return m_username;
    }
    bool rememberPassword() const
    {
        return m_rememberPassword;
    }
    QString password() const
    {
        return m_password;
    }
    bool useSSL() const
    {
        return m_useSSL;
    }
    QString friendlyName() const
    {
        return m_friendlyName;
    }
    bool saveDisconnected() const
    {
        return m_saveDisconnected;
    }
    QStringList poolMembers() const
    {
        return m_poolMembers;
    }
    bool autoConnect() const
    {
        return m_autoConnect;
    }
    qint64 lastConnected() const
    {
        return m_lastConnected;
    }

    // Setters
    void setName(const QString& name)
    {
        m_name = name;
    }
    void setHostname(const QString& hostname)
    {
        m_hostname = hostname;
    }
    void setPort(int port)
    {
        m_port = port;
    }
    void setUsername(const QString& username)
    {
        m_username = username;
    }
    void setRememberPassword(bool remember)
    {
        m_rememberPassword = remember;
    }
    void setPassword(const QString& password)
    {
        m_password = password;
    }
    void setUseSSL(bool useSSL)
    {
        m_useSSL = useSSL;
    }
    void setFriendlyName(const QString& name)
    {
        m_friendlyName = name;
    }
    void setSaveDisconnected(bool save)
    {
        m_saveDisconnected = save;
    }
    void setPoolMembers(const QStringList& members)
    {
        m_poolMembers = members;
    }
    void setAutoConnect(bool autoConnect)
    {
        m_autoConnect = autoConnect;
    }
    void setLastConnected(qint64 timestamp)
    {
        m_lastConnected = timestamp;
    }

    // Serialization
    QVariantMap toVariantMap() const;
    static ConnectionProfile fromVariantMap(const QVariantMap& map);

    // Display name for UI
    QString displayName() const;

    bool isValid() const;

private:
    QString m_name;
    QString m_hostname;
    int m_port;
    QString m_username;
    bool m_rememberPassword;
    QString m_password; // Note: will be encrypted in storage
    bool m_useSSL;
    QString m_friendlyName;
    bool m_saveDisconnected;
    QStringList m_poolMembers; // List of pool member hostnames for failover
    bool m_autoConnect;        // Auto-connect on startup
    qint64 m_lastConnected;    // Timestamp of last successful connection
};

#endif // CONNECTIONPROFILE_H
