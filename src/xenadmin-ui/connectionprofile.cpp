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

#include "connectionprofile.h"
#include <QDateTime>

ConnectionProfile::ConnectionProfile()
    : m_lastConnected(0),
      m_port(443),
      m_rememberPassword(false),
      m_useSSL(true),
      m_saveDisconnected(false),
      m_autoConnect(false)
{
}

ConnectionProfile::ConnectionProfile(const QString& name, const QString& hostname,
                                     int port, const QString& username, bool rememberPassword)
    : m_lastConnected(0),
      m_name(name),
      m_hostname(hostname),
      m_username(username),
      m_port(port),
      m_rememberPassword(rememberPassword),
      m_useSSL(true),
      m_saveDisconnected(false),
      m_autoConnect(false)
{
}

QVariantMap ConnectionProfile::ToVariantMap() const
{
    QVariantMap map;
    map["name"] = this->m_name;
    map["hostname"] = this->m_hostname;
    map["port"] = this->m_port;
    map["username"] = this->m_username;
    map["rememberPassword"] = this->m_rememberPassword;
    map["useSSL"] = this->m_useSSL;
    map["friendlyName"] = this->m_friendlyName;
    map["saveDisconnected"] = this->m_saveDisconnected;
    map["poolMembers"] = this->m_poolMembers;
    map["autoConnect"] = this->m_autoConnect;
    map["lastConnected"] = this->m_lastConnected;
    // Note: password is NOT saved here - it should be handled separately with encryption
    return map;
}

ConnectionProfile ConnectionProfile::FromVariantMap(const QVariantMap& map)
{
    ConnectionProfile profile;
    profile.SetName(map.value("name").toString());
    profile.SetHostname(map.value("hostname").toString());
    profile.SetPort(map.value("port", 443).toInt());
    profile.SetUsername(map.value("username").toString());
    profile.SetRememberPassword(map.value("rememberPassword", false).toBool());
    profile.SetUseSSL(map.value("useSSL", true).toBool());
    profile.SetFriendlyName(map.value("friendlyName").toString());
    profile.SetSaveDisconnected(map.value("saveDisconnected", false).toBool());
    profile.SetPoolMembers(map.value("poolMembers").toStringList());
    profile.SetAutoConnect(map.value("autoConnect", false).toBool());
    profile.SetLastConnected(map.value("lastConnected", 0).toLongLong());
    return profile;
}

QString ConnectionProfile::DisplayName() const
{
    // Return friendly name if set
    if (!this->m_friendlyName.isEmpty())
        return this->m_friendlyName;

    // Fall back to name if available
    if (!this->m_name.isEmpty())
        return this->m_name;

    // Construct display name from hostname and username
    QString display = this->m_hostname;
    if (!this->m_username.isEmpty())
        display = this->m_username + "@" + this->m_hostname;

    if (this->m_port != 443)
        display += QString(":%1").arg(this->m_port);

    return display;
}

bool ConnectionProfile::IsValid() const
{
    return !this->m_hostname.isEmpty();
}
