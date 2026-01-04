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
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

SettingsManager::SettingsManager(QObject* parent)
    : QObject(parent), m_settings(nullptr)
{
    // Set application details for QSettings
    QCoreApplication::setOrganizationName("XenAdmin");
    QCoreApplication::setOrganizationDomain("xenadmin.org");
    QCoreApplication::setApplicationName("XenAdmin Qt");

    // Create settings object (uses platform-specific location)
    this->m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
                                     "XenAdmin", "XenAdminQt", this);

    qDebug() << "Settings file location:" << this->m_settings->fileName();
}

SettingsManager::~SettingsManager()
{
    if (this->m_settings)
    {
        this->m_settings->sync();
    }
}

SettingsManager& SettingsManager::instance()
{
    static SettingsManager instance;
    return instance;
}

// Window state management
void SettingsManager::saveMainWindowGeometry(const QByteArray& geometry)
{
    this->m_settings->setValue("MainWindow/geometry", geometry);
    emit settingsChanged("MainWindow/geometry");
}

QByteArray SettingsManager::loadMainWindowGeometry() const
{
    return this->m_settings->value("MainWindow/geometry").toByteArray();
}

void SettingsManager::saveMainWindowState(const QByteArray& state)
{
    this->m_settings->setValue("MainWindow/state", state);
    emit settingsChanged("MainWindow/state");
}

QByteArray SettingsManager::loadMainWindowState() const
{
    return this->m_settings->value("MainWindow/state").toByteArray();
}

void SettingsManager::saveSplitterState(const QByteArray& state)
{
    this->m_settings->setValue("MainWindow/splitter", state);
    emit settingsChanged("MainWindow/splitter");
}

QByteArray SettingsManager::loadSplitterState() const
{
    return this->m_settings->value("MainWindow/splitter").toByteArray();
}

// Connection management
void SettingsManager::saveConnection(const QString& id, const ConnectionInfo& info)
{
    this->m_settings->beginGroup("Connections/" + id);
    this->m_settings->setValue("hostname", info.hostname);
    this->m_settings->setValue("port", info.port);
    this->m_settings->setValue("username", info.username);

    if (info.savePassword && !info.passwordHash.isEmpty())
    {
        this->m_settings->setValue("password", this->encryptPassword(info.passwordHash));
    } else
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

QList<SettingsManager::ConnectionInfo> SettingsManager::loadConnections() const
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
            info.passwordHash = this->decryptPassword(this->m_settings->value("password").toString());
        }

        info.autoConnect = this->m_settings->value("autoConnect", false).toBool();
        info.friendlyName = this->m_settings->value("friendlyName").toString();
        info.lastConnected = this->m_settings->value("lastConnected", 0).toLongLong();

        this->m_settings->endGroup();

        connections.append(info);
    }

    return connections;
}

void SettingsManager::removeConnection(const QString& id)
{
    this->m_settings->remove("Connections/" + id);
    emit settingsChanged("Connections/" + id);
}

QString SettingsManager::getLastConnectedServer() const
{
    return this->m_settings->value("General/lastConnectedServer").toString();
}

void SettingsManager::setLastConnectedServer(const QString& id)
{
    this->m_settings->setValue("General/lastConnectedServer", id);
    emit settingsChanged("General/lastConnectedServer");
}

QStringList SettingsManager::getServerHistory() const
{
    return this->m_settings->value("General/serverHistory").toStringList();
}

void SettingsManager::updateServerHistory(const QString& hostnameWithPort)
{
    if (hostnameWithPort.isEmpty())
        return;

    QStringList history = getServerHistory();
    if (!history.contains(hostnameWithPort))
    {
        while (history.size() >= 20)
            history.removeFirst();
        history.append(hostnameWithPort);
        this->m_settings->setValue("General/serverHistory", history);
        emit settingsChanged("General/serverHistory");
        this->sync();
    }
}

// Connection profiles
void SettingsManager::saveConnectionProfile(const ConnectionProfile& profile)
{
    if (!profile.isValid())
        return;

    this->m_settings->beginGroup("ConnectionProfiles");
    this->m_settings->beginGroup(profile.name());

    QVariantMap profileData = profile.toVariantMap();
    for (auto it = profileData.begin(); it != profileData.end(); ++it)
    {
        this->m_settings->setValue(it.key(), it.value());
    }

    // Store password separately if remember password is enabled
    if (profile.rememberPassword() && !profile.password().isEmpty())
    {
        QString encryptedPassword = this->encryptPassword(profile.password());
        this->m_settings->setValue("password", encryptedPassword);
    } else
    {
        this->m_settings->remove("password");
    }

    this->m_settings->endGroup();
    this->m_settings->endGroup();
    emit settingsChanged("ConnectionProfiles");
}

QList<ConnectionProfile> SettingsManager::loadConnectionProfiles() const
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

        ConnectionProfile profile = ConnectionProfile::fromVariantMap(profileData);

        // Load password if it was saved
        if (this->m_settings->contains("password"))
        {
            QString encryptedPassword = this->m_settings->value("password").toString();
            QString decryptedPassword = this->decryptPassword(encryptedPassword);
            profile.setPassword(decryptedPassword);
        }

        profiles.append(profile);
        this->m_settings->endGroup();
    }

    this->m_settings->endGroup();
    return profiles;
}

void SettingsManager::removeConnectionProfile(const QString& name)
{
    this->m_settings->beginGroup("ConnectionProfiles");
    this->m_settings->remove(name);
    this->m_settings->endGroup();
    emit settingsChanged("ConnectionProfiles");
}

ConnectionProfile SettingsManager::getLastConnectionProfile() const
{
    QString lastName = this->m_settings->value("General/lastConnectionProfile").toString();
    if (lastName.isEmpty())
        return ConnectionProfile();

    QList<ConnectionProfile> profiles = this->loadConnectionProfiles();
    for (const ConnectionProfile& profile : profiles)
    {
        if (profile.name() == lastName)
            return profile;
    }

    return ConnectionProfile();
}

void SettingsManager::setLastConnectionProfile(const QString& name)
{
    m_settings->setValue("General/lastConnectionProfile", name);
    emit settingsChanged("General/lastConnectionProfile");
}

// Application preferences
bool SettingsManager::getSaveSession() const
{
    return m_settings->value("General/saveSession", true).toBool();
}

void SettingsManager::setSaveSession(bool save)
{
    m_settings->setValue("General/saveSession", save);
    emit settingsChanged("General/saveSession");
}

bool SettingsManager::getAutoConnect() const
{
    return m_settings->value("General/autoConnect", false).toBool();
}

void SettingsManager::setAutoConnect(bool autoConnect)
{
    m_settings->setValue("General/autoConnect", autoConnect);
    emit settingsChanged("General/autoConnect");
}

bool SettingsManager::getCheckForUpdates() const
{
    return m_settings->value("General/checkForUpdates", true).toBool();
}

void SettingsManager::setCheckForUpdates(bool check)
{
    m_settings->setValue("General/checkForUpdates", check);
    emit settingsChanged("General/checkForUpdates");
}

QString SettingsManager::getDefaultExportPath() const
{
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return m_settings->value("Paths/defaultExport", defaultPath).toString();
}

void SettingsManager::setDefaultExportPath(const QString& path)
{
    m_settings->setValue("Paths/defaultExport", path);
    emit settingsChanged("Paths/defaultExport");
}

QString SettingsManager::getDefaultImportPath() const
{
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return m_settings->value("Paths/defaultImport", defaultPath).toString();
}

void SettingsManager::setDefaultImportPath(const QString& path)
{
    m_settings->setValue("Paths/defaultImport", path);
    emit settingsChanged("Paths/defaultImport");
}

bool SettingsManager::getConfirmOnExit() const
{
    return m_settings->value("General/confirmOnExit", true).toBool();
}

void SettingsManager::setConfirmOnExit(bool confirm)
{
    m_settings->setValue("General/confirmOnExit", confirm);
    emit settingsChanged("General/confirmOnExit");
}

bool SettingsManager::getShowHiddenObjects() const
{
    return m_settings->value("View/showHiddenObjects", false).toBool();
}

void SettingsManager::setShowHiddenObjects(bool show)
{
    m_settings->setValue("View/showHiddenObjects", show);
    emit settingsChanged("View/showHiddenObjects");
}

bool SettingsManager::getDefaultTemplatesVisible() const
{
    return m_settings->value("View/defaultTemplatesVisible", false).toBool();
}

void SettingsManager::setDefaultTemplatesVisible(bool visible)
{
    m_settings->setValue("View/defaultTemplatesVisible", visible);
    emit settingsChanged("View/defaultTemplatesVisible");
}

bool SettingsManager::getUserTemplatesVisible() const
{
    return m_settings->value("View/userTemplatesVisible", true).toBool();
}

void SettingsManager::setUserTemplatesVisible(bool visible)
{
    m_settings->setValue("View/userTemplatesVisible", visible);
    emit settingsChanged("View/userTemplatesVisible");
}

bool SettingsManager::getLocalSRsVisible() const
{
    return m_settings->value("View/localSRsVisible", true).toBool();
}

void SettingsManager::setLocalSRsVisible(bool visible)
{
    m_settings->setValue("View/localSRsVisible", visible);
    emit settingsChanged("View/localSRsVisible");
}

int SettingsManager::getConsoleRefreshInterval() const
{
    return m_settings->value("Console/refreshInterval", 5).toInt();
}

void SettingsManager::setConsoleRefreshInterval(int seconds)
{
    m_settings->setValue("Console/refreshInterval", seconds);
    emit settingsChanged("Console/refreshInterval");
}

int SettingsManager::getGraphUpdateInterval() const
{
    return m_settings->value("Performance/graphUpdateInterval", 1).toInt();
}

void SettingsManager::setGraphUpdateInterval(int seconds)
{
    m_settings->setValue("Performance/graphUpdateInterval", seconds);
    emit settingsChanged("Performance/graphUpdateInterval");
}

// Tree view settings
SettingsManager::TreeViewMode SettingsManager::getTreeViewMode() const
{
    int mode = m_settings->value("TreeView/mode", Infrastructure).toInt();
    return static_cast<TreeViewMode>(mode);
}

void SettingsManager::setTreeViewMode(TreeViewMode mode)
{
    m_settings->setValue("TreeView/mode", static_cast<int>(mode));
    emit settingsChanged("TreeView/mode");
}

QStringList SettingsManager::getExpandedTreeItems() const
{
    return m_settings->value("TreeView/expandedItems").toStringList();
}

void SettingsManager::setExpandedTreeItems(const QStringList& items)
{
    m_settings->setValue("TreeView/expandedItems", items);
    emit settingsChanged("TreeView/expandedItems");
}

// Debug settings
bool SettingsManager::getDebugConsoleVisible() const
{
    return m_settings->value("Debug/consoleVisible", false).toBool();
}

void SettingsManager::setDebugConsoleVisible(bool visible)
{
    m_settings->setValue("Debug/consoleVisible", visible);
    emit settingsChanged("Debug/consoleVisible");
}

int SettingsManager::getLogLevel() const
{
    return m_settings->value("Debug/logLevel", 2).toInt(); // 2 = Info
}

void SettingsManager::setLogLevel(int level)
{
    m_settings->setValue("Debug/logLevel", level);
    emit settingsChanged("Debug/logLevel");
}

// Network settings
QString SettingsManager::getProxyServer() const
{
    return m_settings->value("Network/proxyServer").toString();
}

void SettingsManager::setProxyServer(const QString& server)
{
    m_settings->setValue("Network/proxyServer", server);
    emit settingsChanged("Network/proxyServer");
}

int SettingsManager::getProxyPort() const
{
    return m_settings->value("Network/proxyPort", 8080).toInt();
}

void SettingsManager::setProxyPort(int port)
{
    m_settings->setValue("Network/proxyPort", port);
    emit settingsChanged("Network/proxyPort");
}

bool SettingsManager::getUseProxy() const
{
    return m_settings->value("Network/useProxy", false).toBool();
}

void SettingsManager::setUseProxy(bool use)
{
    m_settings->setValue("Network/useProxy", use);
    emit settingsChanged("Network/useProxy");
}

QString SettingsManager::getProxyUsername() const
{
    return m_settings->value("Network/proxyUsername").toString();
}

void SettingsManager::setProxyUsername(const QString& username)
{
    m_settings->setValue("Network/proxyUsername", username);
    emit settingsChanged("Network/proxyUsername");
}

// Recent files/paths
QStringList SettingsManager::getRecentExportPaths() const
{
    return m_settings->value("Recent/exportPaths").toStringList();
}

void SettingsManager::addRecentExportPath(const QString& path)
{
    addToRecentList("Recent/exportPaths", path);
}

QStringList SettingsManager::getRecentImportPaths() const
{
    return m_settings->value("Recent/importPaths").toStringList();
}

void SettingsManager::addRecentImportPath(const QString& path)
{
    addToRecentList("Recent/importPaths", path);
}

// Miscellaneous
QVariant SettingsManager::getValue(const QString& key, const QVariant& defaultValue) const
{
    return m_settings->value(key, defaultValue);
}

void SettingsManager::setValue(const QString& key, const QVariant& value)
{
    m_settings->setValue(key, value);
    emit settingsChanged(key);
}

void SettingsManager::sync()
{
    m_settings->sync();
}

void SettingsManager::clear()
{
    m_settings->clear();
    emit settingsChanged("*");
}

// Helper methods
QString SettingsManager::encryptPassword(const QString& password) const
{
    // Simple XOR-based obfuscation (NOT secure encryption!)
    // For production, use proper encryption like QCA or OpenSSL
    QByteArray data = password.toUtf8();
    QByteArray key = "XenAdminQtKey2024"; // Static key for obfuscation

    QByteArray result;
    for (int i = 0; i < data.size(); ++i)
    {
        result.append(data[i] ^ key[i % key.size()]);
    }

    return result.toBase64();
}

QString SettingsManager::decryptPassword(const QString& encrypted) const
{
    QByteArray data = QByteArray::fromBase64(encrypted.toUtf8());
    QByteArray key = "XenAdminQtKey2024";

    QByteArray result;
    for (int i = 0; i < data.size(); ++i)
    {
        result.append(data[i] ^ key[i % key.size()]);
    }

    return QString::fromUtf8(result);
}

void SettingsManager::addToRecentList(const QString& settingsKey, const QString& path, int maxItems)
{
    QStringList recent = m_settings->value(settingsKey).toStringList();

    // Remove if already exists
    recent.removeAll(path);

    // Add to front
    recent.prepend(path);

    // Limit to maxItems
    while (recent.size() > maxItems)
    {
        recent.removeLast();
    }

    m_settings->setValue(settingsKey, recent);
    emit settingsChanged(settingsKey);
}
