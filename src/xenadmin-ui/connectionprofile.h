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
        QString GetName() const
        {
            return this->m_name;
        }
        QString GetHostname() const
        {
            return this->m_hostname;
        }
        int GetPort() const
        {
            return this->m_port;
        }
        QString GetUsername() const
        {
            return this->m_username;
        }
        bool RememberPassword() const
        {
            return this->m_rememberPassword;
        }
        QString GetPassword() const
        {
            return this->m_password;
        }
        bool UseSSL() const
        {
            return this->m_useSSL;
        }
        QString GetFriendlyName() const
        {
            return this->m_friendlyName;
        }
        bool SaveDisconnected() const
        {
            return this->m_saveDisconnected;
        }
        QStringList GetPoolMembers() const
        {
            return this->m_poolMembers;
        }
        bool GetAutoConnect() const
        {
            return this->m_autoConnect;
        }
        qint64 GetLastConnected() const
        {
            return this->m_lastConnected;
        }

        // Setters
        void SetName(const QString& name)
        {
            this->m_name = name;
        }
        void SetHostname(const QString& hostname)
        {
            this->m_hostname = hostname;
        }
        void SetPort(int port)
        {
            this->m_port = port;
        }
        void SetUsername(const QString& username)
        {
            this->m_username = username;
        }
        void SetRememberPassword(bool remember)
        {
            this->m_rememberPassword = remember;
        }
        void SetPassword(const QString& password)
        {
            this->m_password = password;
        }
        void SetUseSSL(bool useSSL)
        {
            this->m_useSSL = useSSL;
        }
        void SetFriendlyName(const QString& name)
        {
            this->m_friendlyName = name;
        }
        void SetSaveDisconnected(bool save)
        {
            this->m_saveDisconnected = save;
        }
        void SetPoolMembers(const QStringList& members)
        {
            this->m_poolMembers = members;
        }
        void SetAutoConnect(bool autoConnect)
        {
            this->m_autoConnect = autoConnect;
        }
        void SetLastConnected(qint64 timestamp)
        {
            this->m_lastConnected = timestamp;
        }

        // Serialization
        QVariantMap ToVariantMap() const;
        static ConnectionProfile FromVariantMap(const QVariantMap& map);

        // Display name for UI
        QString DisplayName() const;

        bool IsValid() const;

    private:
        qint64 m_lastConnected;    // Timestamp of last successful connection
        QString m_name;
        QString m_hostname;
        QString m_username;
        QString m_password; // Note: will be encrypted in storage
        QString m_friendlyName;
        QStringList m_poolMembers; // List of pool member hostnames for failover
        int m_port;
        bool m_rememberPassword;
        bool m_useSSL;
        bool m_saveDisconnected;
        bool m_autoConnect;        // Auto-connect on startup
};

#endif // CONNECTIONPROFILE_H
