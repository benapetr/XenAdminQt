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

#include "settingsmanager.h"
#include "connectionprofile.h"
#include "mainwindow.h"
#include "globals.h"
#include "xenlib/utils/encryption.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QStandardPaths>

QString SettingsManager::s_configDir;

SettingsManager::SettingsManager(QObject* parent) : QObject(parent), m_settings(nullptr)
{
    // Set application details for QSettings
    QCoreApplication::setOrganizationName(XENADMIN_BRANDING_ORG_NAME);
    //QCoreApplication::setOrganizationDomain(XENADMIN_BRANDING_ORG_DOMAIN);
    QCoreApplication::setApplicationName(XENADMIN_BRANDING_APP_NAME);

    // Create settings object (uses platform-specific location)
    if (!SettingsManager::s_configDir.isEmpty())
    {
        QDir configDir(SettingsManager::s_configDir);
        if (!configDir.exists())
            configDir.mkpath(".");
        this->m_settings = new QSettings(configDir.filePath("XenAdminQt.ini"), QSettings::IniFormat, this);
    }
    else
    {
        this->m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, XENADMIN_BRANDING_ORG_NAME, XENADMIN_BRANDING_APP_NAME, this);
    }

    qDebug() << "Settings file location:" << this->m_settings->fileName();

    this->Load();
}

SettingsManager::~SettingsManager()
{
    if (this->m_settings)
    {
        this->Save();
        this->m_settings->sync();
    }
}

SettingsManager& SettingsManager::instance()
{
    static SettingsManager instance;
    return instance;
}

void SettingsManager::SetConfigDir(const QString& path)
{
    SettingsManager::s_configDir = path;
}

void SettingsManager::Load()
{
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    this->m_autoReconnect = this->m_settings->value("Session/AutoReconnect", true).toBool();
    this->m_checkForUpdates = this->m_settings->value("General/checkForUpdates", true).toBool();
    this->m_defaultExportPath = this->m_settings->value("Paths/defaultExport", defaultPath).toString();
    this->m_defaultImportPath = this->m_settings->value("Paths/defaultImport", defaultPath).toString();
    this->m_confirmOnExit = this->m_settings->value("General/confirmOnExit", true).toBool();
    this->m_showHiddenObjects = this->m_settings->value("View/showHiddenObjects", false).toBool();
    this->m_defaultTemplatesVisible = this->m_settings->value("View/defaultTemplatesVisible", false).toBool();
    this->m_userTemplatesVisible = this->m_settings->value("View/userTemplatesVisible", true).toBool();
    this->m_localSRsVisible = this->m_settings->value("View/localSRsVisible", true).toBool();
    this->m_showAllServerEvents = this->m_settings->value("View/showAllServerEvents", false).toBool();
    this->m_consoleRefreshInterval = this->m_settings->value("Console/refreshInterval", 5).toInt();
    this->m_consoleFullScreenShortcutKey = this->m_settings->value("Console/FullScreenShortcutKey", 0).toInt();
    this->m_consoleDockShortcutKey = this->m_settings->value("Console/DockShortcutKey", 1).toInt();
    this->m_consoleUncaptureShortcutKey = this->m_settings->value("Console/UncaptureShortcutKey", 0).toInt();
    this->m_consoleWindowsShortcuts = this->m_settings->value("Console/WindowsShortcuts", true).toBool();
    this->m_consoleReceiveSoundFromRDP = this->m_settings->value("Console/ReceiveSoundFromRDP", false).toBool();
    this->m_consoleAutoSwitchToRDP = this->m_settings->value("Console/AutoSwitchToRDP", true).toBool();
    this->m_consoleClipboardAndPrinterRedirection = this->m_settings->value("Console/ClipboardAndPrinterRedirection", true).toBool();
    this->m_consoleConnectToServerConsole = this->m_settings->value("Console/ConnectToServerConsole", false).toBool();
    this->m_consolePreserveScaleWhenUndocked = this->m_settings->value("Console/PreserveScaleWhenUndocked", false).toBool();
    this->m_consolePreserveScaleWhenSwitchBackToVNC = this->m_settings->value("Console/PreserveScaleWhenSwitchBackToVNC", false).toBool();
    this->m_graphUpdateInterval = this->m_settings->value("Performance/graphUpdateInterval", 1).toInt();
    this->m_fillAreaUnderGraphs = this->m_settings->value("Display/FillAreaUnderGraphs", false).toBool();
    this->m_rememberLastSelectedTab = this->m_settings->value("Display/RememberLastSelectedTab", true).toBool();
    this->m_showDebugMenu = this->m_settings->value("Display/ShowDebugMenu", false).toBool();
    this->m_doNotConfirmDismissAlerts = this->m_settings->value("Confirmation/DoNotConfirmDismissAlerts", false).toBool();
    this->m_doNotConfirmDismissUpdates = this->m_settings->value("Confirmation/DoNotConfirmDismissUpdates", false).toBool();
    this->m_doNotConfirmDismissEvents = this->m_settings->value("Confirmation/DoNotConfirmDismissEvents", false).toBool();
    this->m_ignoreOvfValidationWarnings = this->m_settings->value("Confirmation/IgnoreOvfValidationWarnings", false).toBool();
    this->m_connectionProxySetting = static_cast<ProxySetting>(this->m_settings->value("Connection/ProxySetting", DirectConnection).toInt());
    this->m_connectionProxyAddress = this->m_settings->value("Connection/ProxyAddress", "").toString();
    this->m_connectionProxyPort = this->m_settings->value("Connection/ProxyPort", 80).toInt();
    this->m_bypassProxyForServers = this->m_settings->value("Connection/BypassProxyForServers", false).toBool();
    this->m_provideProxyAuthentication = this->m_settings->value("Connection/ProvideProxyAuthentication", false).toBool();
    this->m_proxyAuthenticationMethod = this->m_settings->value("Connection/ProxyAuthenticationMethod", 1).toInt();
    this->m_connectionProxyUsernameProtected = this->m_settings->value("Connection/ProxyUsername", "").toString();
    this->m_connectionProxyPasswordProtected = this->m_settings->value("Connection/ProxyPassword", "").toString();
    this->m_connectionTimeoutMs = this->m_settings->value("Connection/ConnectionTimeout", 20000).toInt();
    this->m_treeViewMode = static_cast<TreeViewMode>(this->m_settings->value("TreeView/mode", Infrastructure).toInt());
    this->m_expandedTreeItems = this->m_settings->value("TreeView/expandedItems").toStringList();
    this->m_debugConsoleVisible = this->m_settings->value("Debug/consoleVisible", false).toBool();
    this->m_logLevel = this->m_settings->value("Debug/logLevel", 2).toInt();
    // Backward-compatible mirrors of proxy settings.
    this->m_proxyServer = this->m_connectionProxyAddress;
    this->m_proxyPort = this->m_connectionProxyPort;
    this->m_useProxy = (this->m_connectionProxySetting == SpecifiedProxy);
    this->m_proxyUsername = EncryptionUtils::UnprotectString(this->m_connectionProxyUsernameProtected);
    this->m_recentExportPaths = this->m_settings->value("Recent/exportPaths").toStringList();
    this->m_recentImportPaths = this->m_settings->value("Recent/importPaths").toStringList();
    this->m_lastConnectedServer = this->m_settings->value("General/lastConnectedServer").toString();
    this->m_serverHistory = this->m_settings->value("General/serverHistory").toStringList();
    this->m_saveSession = this->m_settings->value("Session/SaveSession", true).toBool();
    this->m_savePasswords = this->m_settings->value("Session/SavePasswords", true).toBool();
    this->m_requirePass = this->m_settings->value("Session/RequirePass", false).toBool();
    this->m_mainPasswordHash = this->m_settings->value("Session/MainPasswordHash").toByteArray();
    this->m_mainPasswordHashSalt = this->m_settings->value("Session/MainPasswordHashSalt").toByteArray();
    this->m_mainKeySalt = this->m_settings->value("Session/MainKeySalt").toByteArray();
    this->m_mainKdfIterations = this->m_settings->value("Session/MainKdfIterations", 150000).toInt();
    this->m_localKey = this->m_settings->value("Security/LocalKey").toString();
    if (this->m_localKey.isEmpty())
    {
        this->m_localKey = EncryptionUtils::GenerateSessionKey();
        this->m_settings->setValue("Security/LocalKey", this->m_localKey);
    }
    EncryptionUtils::SetLocalKey(this->m_localKey);
}

void SettingsManager::Save()
{
    this->m_settings->setValue("Session/AutoReconnect", this->m_autoReconnect);
    this->m_settings->setValue("General/checkForUpdates", this->m_checkForUpdates);
    this->m_settings->setValue("Paths/defaultExport", this->m_defaultExportPath);
    this->m_settings->setValue("Paths/defaultImport", this->m_defaultImportPath);
    this->m_settings->setValue("General/confirmOnExit", this->m_confirmOnExit);
    this->m_settings->setValue("View/showHiddenObjects", this->m_showHiddenObjects);
    this->m_settings->setValue("View/defaultTemplatesVisible", this->m_defaultTemplatesVisible);
    this->m_settings->setValue("View/userTemplatesVisible", this->m_userTemplatesVisible);
    this->m_settings->setValue("View/localSRsVisible", this->m_localSRsVisible);
    this->m_settings->setValue("View/showAllServerEvents", this->m_showAllServerEvents);
    this->m_settings->setValue("Console/refreshInterval", this->m_consoleRefreshInterval);
    this->m_settings->setValue("Console/FullScreenShortcutKey", this->m_consoleFullScreenShortcutKey);
    this->m_settings->setValue("Console/DockShortcutKey", this->m_consoleDockShortcutKey);
    this->m_settings->setValue("Console/UncaptureShortcutKey", this->m_consoleUncaptureShortcutKey);
    this->m_settings->setValue("Console/WindowsShortcuts", this->m_consoleWindowsShortcuts);
    this->m_settings->setValue("Console/ReceiveSoundFromRDP", this->m_consoleReceiveSoundFromRDP);
    this->m_settings->setValue("Console/AutoSwitchToRDP", this->m_consoleAutoSwitchToRDP);
    this->m_settings->setValue("Console/ClipboardAndPrinterRedirection", this->m_consoleClipboardAndPrinterRedirection);
    this->m_settings->setValue("Console/ConnectToServerConsole", this->m_consoleConnectToServerConsole);
    this->m_settings->setValue("Console/PreserveScaleWhenUndocked", this->m_consolePreserveScaleWhenUndocked);
    this->m_settings->setValue("Console/PreserveScaleWhenSwitchBackToVNC", this->m_consolePreserveScaleWhenSwitchBackToVNC);
    this->m_settings->setValue("Performance/graphUpdateInterval", this->m_graphUpdateInterval);
    this->m_settings->setValue("Display/FillAreaUnderGraphs", this->m_fillAreaUnderGraphs);
    this->m_settings->setValue("Display/RememberLastSelectedTab", this->m_rememberLastSelectedTab);
    this->m_settings->setValue("Display/ShowDebugMenu", this->m_showDebugMenu);
    this->m_settings->setValue("Confirmation/DoNotConfirmDismissAlerts", this->m_doNotConfirmDismissAlerts);
    this->m_settings->setValue("Confirmation/DoNotConfirmDismissUpdates", this->m_doNotConfirmDismissUpdates);
    this->m_settings->setValue("Confirmation/DoNotConfirmDismissEvents", this->m_doNotConfirmDismissEvents);
    this->m_settings->setValue("Confirmation/IgnoreOvfValidationWarnings", this->m_ignoreOvfValidationWarnings);
    this->m_settings->setValue("Connection/ProxySetting", static_cast<int>(this->m_connectionProxySetting));
    this->m_settings->setValue("Connection/ProxyAddress", this->m_connectionProxyAddress);
    this->m_settings->setValue("Connection/ProxyPort", this->m_connectionProxyPort);
    this->m_settings->setValue("Connection/BypassProxyForServers", this->m_bypassProxyForServers);
    this->m_settings->setValue("Connection/ProvideProxyAuthentication", this->m_provideProxyAuthentication);
    this->m_settings->setValue("Connection/ProxyAuthenticationMethod", this->m_proxyAuthenticationMethod);
    this->m_settings->setValue("Connection/ProxyUsername", this->m_connectionProxyUsernameProtected);
    this->m_settings->setValue("Connection/ProxyPassword", this->m_connectionProxyPasswordProtected);
    this->m_settings->setValue("Connection/ConnectionTimeout", this->m_connectionTimeoutMs);
    this->m_settings->setValue("TreeView/mode", static_cast<int>(this->m_treeViewMode));
    this->m_settings->setValue("TreeView/expandedItems", this->m_expandedTreeItems);
    this->m_settings->setValue("Debug/consoleVisible", this->m_debugConsoleVisible);
    this->m_settings->setValue("Debug/logLevel", this->m_logLevel);
    // Backward-compatible keys.
    this->m_settings->setValue("Network/proxyServer", this->m_connectionProxyAddress);
    this->m_settings->setValue("Network/proxyPort", this->m_connectionProxyPort);
    this->m_settings->setValue("Network/useProxy", this->m_connectionProxySetting == SpecifiedProxy);
    this->m_settings->setValue("Network/proxyUsername", EncryptionUtils::UnprotectString(this->m_connectionProxyUsernameProtected));
    this->m_settings->setValue("Recent/exportPaths", this->m_recentExportPaths);
    this->m_settings->setValue("Recent/importPaths", this->m_recentImportPaths);
    this->m_settings->setValue("General/lastConnectedServer", this->m_lastConnectedServer);
    this->m_settings->setValue("General/serverHistory", this->m_serverHistory);
    this->m_settings->setValue("Session/SaveSession", this->m_saveSession);
    this->m_settings->setValue("Session/SavePasswords", this->m_savePasswords);
    this->m_settings->setValue("Session/RequirePass", this->m_requirePass);

    if (this->m_mainPasswordHash.isEmpty())
        this->m_settings->remove("Session/MainPasswordHash");
    else
        this->m_settings->setValue("Session/MainPasswordHash", this->m_mainPasswordHash);

    if (this->m_mainPasswordHashSalt.isEmpty())
        this->m_settings->remove("Session/MainPasswordHashSalt");
    else
        this->m_settings->setValue("Session/MainPasswordHashSalt", this->m_mainPasswordHashSalt);

    if (this->m_mainKeySalt.isEmpty())
        this->m_settings->remove("Session/MainKeySalt");
    else
        this->m_settings->setValue("Session/MainKeySalt", this->m_mainKeySalt);

    this->m_settings->setValue("Session/MainKdfIterations", this->m_mainKdfIterations);
    this->m_settings->setValue("Security/LocalKey", this->m_localKey);
}

// Window state management
void SettingsManager::SaveMainWindowGeometry(const QByteArray& geometry)
{
    this->m_settings->setValue("MainWindow/geometry", geometry);
    emit settingsChanged("MainWindow/geometry");
}

QByteArray SettingsManager::LoadMainWindowGeometry() const
{
    return this->m_settings->value("MainWindow/geometry").toByteArray();
}

void SettingsManager::SaveMainWindowState(const QByteArray& state)
{
    this->m_settings->setValue("MainWindow/state", state);
    emit settingsChanged("MainWindow/state");
}

QByteArray SettingsManager::LoadMainWindowState() const
{
    return this->m_settings->value("MainWindow/state").toByteArray();
}

void SettingsManager::SaveSplitterState(const QByteArray& state)
{
    this->m_settings->setValue("MainWindow/splitter", state);
    emit settingsChanged("MainWindow/splitter");
}

QByteArray SettingsManager::LoadSplitterState() const
{
    return this->m_settings->value("MainWindow/splitter").toByteArray();
}

// Connection management
void SettingsManager::SaveConnection(const QString& id, const ConnectionInfo& info)
{
    this->m_settings->beginGroup("Connections/" + id);
    this->m_settings->setValue("hostname", info.hostname);
    this->m_settings->setValue("port", info.port);
    this->m_settings->setValue("username", info.username);

    if (info.savePassword && !info.passwordHash.isEmpty())
    {
        QString encryptedPassword = this->encryptPassword_(info.passwordHash);
        if (encryptedPassword.isEmpty())
        {
            qWarning() << "SettingsManager: Failed to encrypt password for connection" << id << "- keeping existing value";
        }
        else
        {
            this->m_settings->setValue("password", encryptedPassword);
        }
    }
    else
    {
        this->m_settings->remove("password");
    }

    this->m_settings->setValue("savePassword", info.savePassword);
    this->m_settings->setValue("autoConnect", info.autoConnect);
    this->m_settings->setValue("friendlyName", info.friendlyName);
    this->m_settings->setValue("lastConnected", info.lastConnected);
    this->m_settings->endGroup();

    emit settingsChanged("Connections/" + id);
}

QList<SettingsManager::ConnectionInfo> SettingsManager::LoadConnections() const
{
    QList<ConnectionInfo> connections;

    this->m_settings->beginGroup("Connections");
    QStringList connectionIds = this->m_settings->childGroups();
    this->m_settings->endGroup();

    for (const QString& id : connectionIds)
    {
        ConnectionInfo info;
        this->m_settings->beginGroup("Connections/" + id);

        info.hostname = this->m_settings->value("hostname").toString();
        info.port = this->m_settings->value("port", 443).toInt();
        info.username = this->m_settings->value("username").toString();
        info.savePassword = this->m_settings->value("savePassword", false).toBool();

        if (info.savePassword && this->m_settings->contains("password"))
        {
            info.passwordHash = this->decryptPassword_(this->m_settings->value("password").toString());
        }

        info.autoConnect = this->m_settings->value("autoConnect", false).toBool();
        info.friendlyName = this->m_settings->value("friendlyName").toString();
        info.lastConnected = this->m_settings->value("lastConnected", 0).toLongLong();

        this->m_settings->endGroup();

        connections.append(info);
    }

    return connections;
}

void SettingsManager::RemoveConnection(const QString& id)
{
    this->m_settings->remove("Connections/" + id);
    emit settingsChanged("Connections/" + id);
}

QString SettingsManager::GetLastConnectedServer() const
{
    return this->m_lastConnectedServer;
}

void SettingsManager::SetLastConnectedServer(const QString& id)
{
    this->m_lastConnectedServer = id;
    emit settingsChanged("General/lastConnectedServer");
}

QStringList SettingsManager::GetServerHistory() const
{
    return this->m_serverHistory;
}

void SettingsManager::UpdateServerHistory(const QString& hostnameWithPort)
{
    if (hostnameWithPort.isEmpty())
        return;

    QStringList history = this->GetServerHistory();
    if (!history.contains(hostnameWithPort))
    {
        while (history.size() >= 20)
            history.removeFirst();
        history.append(hostnameWithPort);
        this->m_serverHistory = history;
        emit settingsChanged("General/serverHistory");
        this->Sync();
    }
}

// Connection profiles
void SettingsManager::SaveConnectionProfile(const ConnectionProfile& profile)
{
    if (!profile.IsValid())
        return;

    this->m_settings->beginGroup("ConnectionProfiles");
    this->m_settings->beginGroup(profile.GetName());

    QVariantMap profileData = profile.ToVariantMap();
    for (auto it = profileData.begin(); it != profileData.end(); ++it)
    {
        this->m_settings->setValue(it.key(), it.value());
    }

    // Store password separately if remember password is enabled
    if (profile.RememberPassword() && !profile.GetPassword().isEmpty())
    {
        QString encryptedPassword = this->encryptPassword_(profile.GetPassword());
        if (encryptedPassword.isEmpty())
        {
            qWarning() << "SettingsManager: Failed to encrypt password for profile" << profile.GetName()
                       << "- keeping existing value";
        }
        else
        {
            this->m_settings->setValue("password", encryptedPassword);
        }
    }
    else
    {
        this->m_settings->remove("password");
    }

    this->m_settings->endGroup();
    this->m_settings->endGroup();
    emit settingsChanged("ConnectionProfiles");
}

QList<ConnectionProfile> SettingsManager::LoadConnectionProfiles() const
{
    QList<ConnectionProfile> profiles;

    this->m_settings->beginGroup("ConnectionProfiles");
    QStringList profileNames = this->m_settings->childGroups();

    for (const QString& name : profileNames)
    {
        this->m_settings->beginGroup(name);

        QVariantMap profileData;
        QStringList keys = this->m_settings->allKeys();
        for (const QString& key : keys)
        {
            if (key != "password")
                profileData[key] = this->m_settings->value(key);
        }

        ConnectionProfile profile = ConnectionProfile::FromVariantMap(profileData);

        // Load password if it was saved
        if (this->m_settings->contains("password"))
        {
            QString encryptedPassword = this->m_settings->value("password").toString();
            QString decryptedPassword = this->decryptPassword_(encryptedPassword);
            profile.SetPassword(decryptedPassword);
        }

        profiles.append(profile);
        this->m_settings->endGroup();
    }

    this->m_settings->endGroup();
    return profiles;
}

void SettingsManager::RemoveConnectionProfile(const QString& name)
{
    this->m_settings->beginGroup("ConnectionProfiles");
    this->m_settings->remove(name);
    this->m_settings->endGroup();
    emit settingsChanged("ConnectionProfiles");
}

ConnectionProfile SettingsManager::GetLastConnectionProfile() const
{
    QString lastName = this->m_settings->value("General/lastConnectionProfile").toString();
    if (lastName.isEmpty())
        return ConnectionProfile();

    QList<ConnectionProfile> profiles = this->LoadConnectionProfiles();
    for (const ConnectionProfile& profile : profiles)
    {
        if (profile.GetName() == lastName)
            return profile;
    }

    return ConnectionProfile();
}

void SettingsManager::SetLastConnectionProfile(const QString& name)
{
    this->m_settings->setValue("General/lastConnectionProfile", name);
    emit settingsChanged("General/lastConnectionProfile");
}

// Application preferences
bool SettingsManager::GetAutoReconnect() const
{
    return this->m_autoReconnect;
}

void SettingsManager::SetAutoReconnect(bool autoReconnect)
{
    this->m_autoReconnect = autoReconnect;
    emit settingsChanged("Session/AutoReconnect");
}

bool SettingsManager::GetCheckForUpdates() const
{
    return this->m_checkForUpdates;
}

void SettingsManager::SetCheckForUpdates(bool check)
{
    this->m_checkForUpdates = check;
    emit settingsChanged("General/checkForUpdates");
}

QString SettingsManager::GetDefaultExportPath() const
{
    return this->m_defaultExportPath;
}

void SettingsManager::SetDefaultExportPath(const QString& path)
{
    this->m_defaultExportPath = path;
    emit settingsChanged("Paths/defaultExport");
}

QString SettingsManager::GetDefaultImportPath() const
{
    return this->m_defaultImportPath;
}

void SettingsManager::SetDefaultImportPath(const QString& path)
{
    this->m_defaultImportPath = path;
    emit settingsChanged("Paths/defaultImport");
}

bool SettingsManager::GetConfirmOnExit() const
{
    return this->m_confirmOnExit;
}

void SettingsManager::SetConfirmOnExit(bool confirm)
{
    this->m_confirmOnExit = confirm;
    emit settingsChanged("General/confirmOnExit");
}

bool SettingsManager::GetShowHiddenObjects() const
{
    return this->m_showHiddenObjects;
}

void SettingsManager::SetShowHiddenObjects(bool show)
{
    this->m_showHiddenObjects = show;
    emit settingsChanged("View/showHiddenObjects");
}

bool SettingsManager::GetDefaultTemplatesVisible() const
{
    return this->m_defaultTemplatesVisible;
}

void SettingsManager::SetDefaultTemplatesVisible(bool visible)
{
    this->m_defaultTemplatesVisible = visible;
    emit settingsChanged("View/defaultTemplatesVisible");
}

bool SettingsManager::GetUserTemplatesVisible() const
{
    return this->m_userTemplatesVisible;
}

void SettingsManager::SetUserTemplatesVisible(bool visible)
{
    this->m_userTemplatesVisible = visible;
    emit settingsChanged("View/userTemplatesVisible");
}

bool SettingsManager::GetLocalSRsVisible() const
{
    return this->m_localSRsVisible;
}

void SettingsManager::SetLocalSRsVisible(bool visible)
{
    this->m_localSRsVisible = visible;
    emit settingsChanged("View/localSRsVisible");
}

bool SettingsManager::GetShowAllServerEvents() const
{
    return this->m_showAllServerEvents;
}

void SettingsManager::SetShowAllServerEvents(bool show)
{
    this->m_showAllServerEvents = show;
    emit settingsChanged("View/showAllServerEvents");
}

int SettingsManager::GetConsoleRefreshInterval() const
{
    return this->m_consoleRefreshInterval;
}

void SettingsManager::SetConsoleRefreshInterval(int seconds)
{
    this->m_consoleRefreshInterval = seconds;
    emit settingsChanged("Console/refreshInterval");
}

int SettingsManager::GetConsoleFullScreenShortcutKey() const
{
    return this->m_consoleFullScreenShortcutKey;
}

void SettingsManager::SetConsoleFullScreenShortcutKey(int keyIndex)
{
    this->m_consoleFullScreenShortcutKey = keyIndex;
    emit settingsChanged("Console/FullScreenShortcutKey");
}

int SettingsManager::GetConsoleDockShortcutKey() const
{
    return this->m_consoleDockShortcutKey;
}

void SettingsManager::SetConsoleDockShortcutKey(int keyIndex)
{
    this->m_consoleDockShortcutKey = keyIndex;
    emit settingsChanged("Console/DockShortcutKey");
}

int SettingsManager::GetConsoleUncaptureShortcutKey() const
{
    return this->m_consoleUncaptureShortcutKey;
}

void SettingsManager::SetConsoleUncaptureShortcutKey(int keyIndex)
{
    this->m_consoleUncaptureShortcutKey = keyIndex;
    emit settingsChanged("Console/UncaptureShortcutKey");
}

bool SettingsManager::GetConsoleWindowsShortcuts() const
{
    return this->m_consoleWindowsShortcuts;
}

void SettingsManager::SetConsoleWindowsShortcuts(bool enabled)
{
    this->m_consoleWindowsShortcuts = enabled;
    emit settingsChanged("Console/WindowsShortcuts");
}

bool SettingsManager::GetConsoleReceiveSoundFromRDP() const
{
    return this->m_consoleReceiveSoundFromRDP;
}

void SettingsManager::SetConsoleReceiveSoundFromRDP(bool enabled)
{
    this->m_consoleReceiveSoundFromRDP = enabled;
    emit settingsChanged("Console/ReceiveSoundFromRDP");
}

bool SettingsManager::GetConsoleAutoSwitchToRDP() const
{
    return this->m_consoleAutoSwitchToRDP;
}

void SettingsManager::SetConsoleAutoSwitchToRDP(bool enabled)
{
    this->m_consoleAutoSwitchToRDP = enabled;
    emit settingsChanged("Console/AutoSwitchToRDP");
}

bool SettingsManager::GetConsoleClipboardAndPrinterRedirection() const
{
    return this->m_consoleClipboardAndPrinterRedirection;
}

void SettingsManager::SetConsoleClipboardAndPrinterRedirection(bool enabled)
{
    this->m_consoleClipboardAndPrinterRedirection = enabled;
    emit settingsChanged("Console/ClipboardAndPrinterRedirection");
}

bool SettingsManager::GetConsoleConnectToServerConsole() const
{
    return this->m_consoleConnectToServerConsole;
}

void SettingsManager::SetConsoleConnectToServerConsole(bool enabled)
{
    this->m_consoleConnectToServerConsole = enabled;
    emit settingsChanged("Console/ConnectToServerConsole");
}

bool SettingsManager::GetConsolePreserveScaleWhenUndocked() const
{
    return this->m_consolePreserveScaleWhenUndocked;
}

void SettingsManager::SetConsolePreserveScaleWhenUndocked(bool enabled)
{
    this->m_consolePreserveScaleWhenUndocked = enabled;
    emit settingsChanged("Console/PreserveScaleWhenUndocked");
}

bool SettingsManager::GetConsolePreserveScaleWhenSwitchBackToVNC() const
{
    return this->m_consolePreserveScaleWhenSwitchBackToVNC;
}

void SettingsManager::SetConsolePreserveScaleWhenSwitchBackToVNC(bool enabled)
{
    this->m_consolePreserveScaleWhenSwitchBackToVNC = enabled;
    emit settingsChanged("Console/PreserveScaleWhenSwitchBackToVNC");
}

int SettingsManager::GetGraphUpdateInterval() const
{
    return this->m_graphUpdateInterval;
}

void SettingsManager::SetGraphUpdateInterval(int seconds)
{
    this->m_graphUpdateInterval = seconds;
    emit settingsChanged("Performance/graphUpdateInterval");
}

bool SettingsManager::GetFillAreaUnderGraphs() const
{
    return this->m_fillAreaUnderGraphs;
}

void SettingsManager::SetFillAreaUnderGraphs(bool fill)
{
    this->m_fillAreaUnderGraphs = fill;
    emit settingsChanged("Display/FillAreaUnderGraphs");
}

bool SettingsManager::GetRememberLastSelectedTab() const
{
    return this->m_rememberLastSelectedTab;
}

void SettingsManager::SetRememberLastSelectedTab(bool remember)
{
    this->m_rememberLastSelectedTab = remember;
    emit settingsChanged("Display/RememberLastSelectedTab");
}

bool SettingsManager::GetShowDebugMenu() const
{
    return this->m_showDebugMenu;
}

void SettingsManager::SetShowDebugMenu(bool show)
{
    this->m_showDebugMenu = show;
    emit settingsChanged("Display/ShowDebugMenu");
}

bool SettingsManager::GetDoNotConfirmDismissAlerts() const
{
    return this->m_doNotConfirmDismissAlerts;
}

void SettingsManager::SetDoNotConfirmDismissAlerts(bool doNotConfirm)
{
    this->m_doNotConfirmDismissAlerts = doNotConfirm;
    emit settingsChanged("Confirmation/DoNotConfirmDismissAlerts");
}

bool SettingsManager::GetDoNotConfirmDismissUpdates() const
{
    return this->m_doNotConfirmDismissUpdates;
}

void SettingsManager::SetDoNotConfirmDismissUpdates(bool doNotConfirm)
{
    this->m_doNotConfirmDismissUpdates = doNotConfirm;
    emit settingsChanged("Confirmation/DoNotConfirmDismissUpdates");
}

bool SettingsManager::GetDoNotConfirmDismissEvents() const
{
    return this->m_doNotConfirmDismissEvents;
}

void SettingsManager::SetDoNotConfirmDismissEvents(bool doNotConfirm)
{
    this->m_doNotConfirmDismissEvents = doNotConfirm;
    emit settingsChanged("Confirmation/DoNotConfirmDismissEvents");
}

bool SettingsManager::GetIgnoreOvfValidationWarnings() const
{
    return this->m_ignoreOvfValidationWarnings;
}

void SettingsManager::SetIgnoreOvfValidationWarnings(bool ignoreWarnings)
{
    this->m_ignoreOvfValidationWarnings = ignoreWarnings;
    emit settingsChanged("Confirmation/IgnoreOvfValidationWarnings");
}

SettingsManager::ProxySetting SettingsManager::GetConnectionProxySetting() const
{
    return this->m_connectionProxySetting;
}

void SettingsManager::SetConnectionProxySetting(ProxySetting setting)
{
    this->m_connectionProxySetting = setting;
    emit settingsChanged("Connection/ProxySetting");
}

QString SettingsManager::GetConnectionProxyAddress() const
{
    return this->m_connectionProxyAddress;
}

void SettingsManager::SetConnectionProxyAddress(const QString& address)
{
    this->m_connectionProxyAddress = address;
    this->m_proxyServer = address;
    emit settingsChanged("Connection/ProxyAddress");
}

int SettingsManager::GetConnectionProxyPort() const
{
    return this->m_connectionProxyPort;
}

void SettingsManager::SetConnectionProxyPort(int port)
{
    this->m_connectionProxyPort = port;
    this->m_proxyPort = port;
    emit settingsChanged("Connection/ProxyPort");
}

bool SettingsManager::GetBypassProxyForServers() const
{
    return this->m_bypassProxyForServers;
}

void SettingsManager::SetBypassProxyForServers(bool bypass)
{
    this->m_bypassProxyForServers = bypass;
    emit settingsChanged("Connection/BypassProxyForServers");
}

bool SettingsManager::GetProvideProxyAuthentication() const
{
    return this->m_provideProxyAuthentication;
}

void SettingsManager::SetProvideProxyAuthentication(bool provide)
{
    this->m_provideProxyAuthentication = provide;
    emit settingsChanged("Connection/ProvideProxyAuthentication");
}

int SettingsManager::GetProxyAuthenticationMethod() const
{
    return this->m_proxyAuthenticationMethod;
}

void SettingsManager::SetProxyAuthenticationMethod(int method)
{
    this->m_proxyAuthenticationMethod = method;
    emit settingsChanged("Connection/ProxyAuthenticationMethod");
}

QString SettingsManager::GetConnectionProxyUsername() const
{
    return EncryptionUtils::UnprotectString(this->m_connectionProxyUsernameProtected);
}

void SettingsManager::SetConnectionProxyUsername(const QString& username)
{
    this->m_connectionProxyUsernameProtected = EncryptionUtils::ProtectString(username);
    this->m_proxyUsername = username;
    emit settingsChanged("Connection/ProxyUsername");
}

QString SettingsManager::GetConnectionProxyPassword() const
{
    return EncryptionUtils::UnprotectString(this->m_connectionProxyPasswordProtected);
}

void SettingsManager::SetConnectionProxyPassword(const QString& password)
{
    this->m_connectionProxyPasswordProtected = EncryptionUtils::ProtectString(password);
    emit settingsChanged("Connection/ProxyPassword");
}

int SettingsManager::GetConnectionTimeoutMs() const
{
    return this->m_connectionTimeoutMs;
}

void SettingsManager::SetConnectionTimeoutMs(int timeoutMs)
{
    this->m_connectionTimeoutMs = qMax(1000, timeoutMs);
    emit settingsChanged("Connection/ConnectionTimeout");
}

void SettingsManager::ApplyProxySettings() const
{
    if (QCoreApplication::instance())
        QCoreApplication::instance()->setProperty("ConnectionTimeoutMs", this->m_connectionTimeoutMs);

    switch (this->m_connectionProxySetting)
    {
    case DirectConnection:
    {
        QNetworkProxyFactory::setUseSystemConfiguration(false);
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        break;
    }
    case SystemProxy:
    {
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::DefaultProxy));
        break;
    }
    case SpecifiedProxy:
    {
        if (this->m_bypassProxyForServers || this->m_connectionProxyAddress.trimmed().isEmpty())
        {
            QNetworkProxyFactory::setUseSystemConfiguration(false);
            QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));
            break;
        }

        QNetworkProxy proxy(QNetworkProxy::HttpProxy,
                            this->m_connectionProxyAddress.trimmed(),
                            this->m_connectionProxyPort > 0 ? this->m_connectionProxyPort : 80);

        if (this->m_provideProxyAuthentication)
        {
            proxy.setUser(this->GetConnectionProxyUsername());
            proxy.setPassword(this->GetConnectionProxyPassword());
        }

        QNetworkProxyFactory::setUseSystemConfiguration(false);
        QNetworkProxy::setApplicationProxy(proxy);
        break;
    }
    }
}

// Tree view settings
SettingsManager::TreeViewMode SettingsManager::GetTreeViewMode() const
{
    return this->m_treeViewMode;
}

void SettingsManager::SetTreeViewMode(TreeViewMode mode)
{
    this->m_treeViewMode = mode;
    emit settingsChanged("TreeView/mode");
}

QStringList SettingsManager::GetExpandedTreeItems() const
{
    return this->m_expandedTreeItems;
}

void SettingsManager::SetExpandedTreeItems(const QStringList& items)
{
    this->m_expandedTreeItems = items;
    emit settingsChanged("TreeView/expandedItems");
}

// Debug settings
bool SettingsManager::GetDebugConsoleVisible() const
{
    return this->m_debugConsoleVisible;
}

void SettingsManager::SetDebugConsoleVisible(bool visible)
{
    this->m_debugConsoleVisible = visible;
    emit settingsChanged("Debug/consoleVisible");
}

int SettingsManager::GetLogLevel() const
{
    return this->m_logLevel;
}

void SettingsManager::SetLogLevel(int level)
{
    this->m_logLevel = level;
    emit settingsChanged("Debug/logLevel");
}

// Network settings
QString SettingsManager::GetProxyServer() const
{
    return this->m_connectionProxyAddress;
}

void SettingsManager::SetProxyServer(const QString& server)
{
    this->m_proxyServer = server;
    this->m_connectionProxyAddress = server;
    emit settingsChanged("Network/proxyServer");
}

int SettingsManager::GetProxyPort() const
{
    return this->m_connectionProxyPort;
}

void SettingsManager::SetProxyPort(int port)
{
    this->m_proxyPort = port;
    this->m_connectionProxyPort = port;
    emit settingsChanged("Network/proxyPort");
}

bool SettingsManager::GetUseProxy() const
{
    return this->m_connectionProxySetting == SpecifiedProxy;
}

void SettingsManager::SetUseProxy(bool use)
{
    this->m_useProxy = use;
    this->m_connectionProxySetting = use ? SpecifiedProxy : DirectConnection;
    emit settingsChanged("Network/useProxy");
}

QString SettingsManager::GetProxyUsername() const
{
    return this->GetConnectionProxyUsername();
}

void SettingsManager::SetProxyUsername(const QString& username)
{
    this->m_proxyUsername = username;
    this->m_connectionProxyUsernameProtected = EncryptionUtils::ProtectString(username);
    emit settingsChanged("Network/proxyUsername");
}

// Recent files/paths
QStringList SettingsManager::GetRecentExportPaths() const
{
    return this->m_recentExportPaths;
}

void SettingsManager::AddRecentExportPath(const QString& path)
{
    this->addToRecentList_("Recent/exportPaths", path);
}

QStringList SettingsManager::GetRecentImportPaths() const
{
    return this->m_recentImportPaths;
}

void SettingsManager::AddRecentImportPath(const QString& path)
{
    this->addToRecentList_("Recent/importPaths", path);
}

// Miscellaneous
QVariant SettingsManager::GetValue(const QString& key, const QVariant& defaultValue) const
{
    return this->m_settings->value(key, defaultValue);
}

void SettingsManager::SetValue(const QString& key, const QVariant& value)
{
    this->m_settings->setValue(key, value);
    emit settingsChanged(key);
}

void SettingsManager::Sync()
{
    this->Save();
    this->m_settings->sync();
}

void SettingsManager::Clear()
{
    this->m_settings->clear();
    this->Load();
    emit settingsChanged("*");
}

QString SettingsManager::GetFileName() const
{
    return this->m_settings->fileName();
}

// Helper methods
QString SettingsManager::encryptPassword_(const QString& password) const
{
    // Matches C# Settings.EncryptCredentials() logic at line 560:
    // Uses AES encryption with master password if RequirePass is enabled,
    // otherwise uses local machine protection

    if (!this->GetRequirePass())
    {
        qDebug() << "reqpass false";
    }

    if (this->GetRequirePass() && !this->GetMainKey().isEmpty())
    {
        // Encrypt using master password as key (AES-256-CBC)
        return EncryptionUtils::EncryptStringWithKey(password, this->GetMainKey());
    }
    
    // Default: use local machine protection (XOR with random key)
    return EncryptionUtils::ProtectString(password);
}

QString SettingsManager::decryptPassword_(const QString& encrypted) const
{
    // Matches C# Settings.DecryptCredentials() logic
    
    if (encrypted.isEmpty())
    {
        return QString();
    }

    // Check if using local machine protection (format: "enc:...")
    // This has to take precedence in order to convert from settings created before master password
    if (encrypted.startsWith("enc:"))
        return EncryptionUtils::UnprotectString(encrypted);
    
    // Check if encrypted with master password (AES format: "base64,base64")
    if (this->GetRequirePass() && !this->GetMainKey().isEmpty() && encrypted.contains(','))
    {
        // Try decrypting with master password (AES-256-CBC)
        QString decrypted = EncryptionUtils::DecryptStringWithKey(encrypted, this->GetMainKey());
        if (!decrypted.isEmpty())
        {
            return decrypted;
        }
        // Fall through to other methods if AES decryption fails
    }

    return encrypted;
}

void SettingsManager::addToRecentList_(const QString& settingsKey, const QString& path, int maxItems)
{
    QStringList* recent = nullptr;
    if (settingsKey == "Recent/exportPaths")
        recent = &this->m_recentExportPaths;
    else if (settingsKey == "Recent/importPaths")
        recent = &this->m_recentImportPaths;

    if (!recent)
        return;

    // Remove if already exists
    recent->removeAll(path);

    // Add to front
    recent->prepend(path);

    // Limit to maxItems
    while (recent->size() > maxItems)
    {
        recent->removeLast();
    }

    emit settingsChanged(settingsKey);
}

QByteArray SettingsManager::GetMainKey() const
{
    return this->m_mainKey;
}

void SettingsManager::SetMainKey(const QByteArray& key)
{
    this->m_mainKey = key;
}

QByteArray SettingsManager::GetMainPasswordHash() const
{
    return this->m_mainPasswordHash;
}

void SettingsManager::SetMainPasswordHash(const QByteArray& passwordHash)
{
    this->m_mainPasswordHash = passwordHash;
}

QByteArray SettingsManager::GetMainPasswordHashSalt() const
{
    return this->m_mainPasswordHashSalt;
}

void SettingsManager::SetMainPasswordHashSalt(const QByteArray& salt)
{
    this->m_mainPasswordHashSalt = salt;
}

QByteArray SettingsManager::GetMainKeySalt() const
{
    return this->m_mainKeySalt;
}

void SettingsManager::SetMainKeySalt(const QByteArray& salt)
{
    this->m_mainKeySalt = salt;
}

int SettingsManager::GetMainKdfIterations() const
{
    return this->m_mainKdfIterations;
}

void SettingsManager::SetMainKdfIterations(int iterations)
{
    this->m_mainKdfIterations = iterations;
}

bool SettingsManager::GetSaveSession() const
{
    return this->m_saveSession;
}

void SettingsManager::SetSaveSession(bool save)
{
    this->m_saveSession = save;
    emit settingsChanged("Session/SaveSession");
}

bool SettingsManager::GetSavePasswords() const
{
    return this->m_savePasswords;
}

void SettingsManager::SetSavePasswords(bool save)
{
    this->m_savePasswords = save;
    emit settingsChanged("Session/SavePasswords");
}

bool SettingsManager::GetRequirePass() const
{
    return this->m_requirePass;
}

void SettingsManager::SetRequirePass(bool require)
{
    this->m_requirePass = require;
    emit settingsChanged("Session/RequirePass");
}
