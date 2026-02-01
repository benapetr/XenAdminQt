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

#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QByteArray>
#include <QStringList>
#include <QVariantMap>

class ConnectionProfile;

/**
 * @brief Centralized settings management for XenAdmin Qt
 *
 * Provides persistent storage for application preferences, window state,
 * connection settings, and user preferences using QSettings.
 */
class SettingsManager : public QObject
{
    Q_OBJECT

    public:
        static SettingsManager& instance();

        void Load();
        void Save();

        // Window state
        void saveMainWindowGeometry(const QByteArray& geometry);
        QByteArray loadMainWindowGeometry() const;
        void saveMainWindowState(const QByteArray& state);
        QByteArray loadMainWindowState() const;
        void saveSplitterState(const QByteArray& state);
        QByteArray loadSplitterState() const;

        // Connection settings
        struct ConnectionInfo
        {
            QString hostname;
            int port;
            QString username;
            QString passwordHash; // Encrypted/hashed password
            bool savePassword;
            bool autoConnect;
            QString friendlyName;
            qint64 lastConnected;
        };

        void saveConnection(const QString& id, const ConnectionInfo& info);
        QList<ConnectionInfo> loadConnections() const;
        void removeConnection(const QString& id);
        QString getLastConnectedServer() const;
        void setLastConnectedServer(const QString& id);

        // Server history (matches C# Settings.UpdateServerHistory)
        QStringList getServerHistory() const;
        void updateServerHistory(const QString& hostnameWithPort);

        // Connection profiles
        void saveConnectionProfile(const ConnectionProfile& profile);
        QList<ConnectionProfile> loadConnectionProfiles() const;
        void removeConnectionProfile(const QString& name);
        ConnectionProfile getLastConnectionProfile() const;
        void setLastConnectionProfile(const QString& name);

        bool GetAutoReconnect() const;
        void SetAutoReconnect(bool autoReconnect);

        bool getCheckForUpdates() const;
        void setCheckForUpdates(bool check);

        QString getDefaultExportPath() const;
        void setDefaultExportPath(const QString& path);

        QString getDefaultImportPath() const;
        void setDefaultImportPath(const QString& path);

        bool getConfirmOnExit() const;
        void setConfirmOnExit(bool confirm);

        bool getShowHiddenObjects() const;
        void setShowHiddenObjects(bool show);

        bool getDefaultTemplatesVisible() const;
        void setDefaultTemplatesVisible(bool visible);

        bool getUserTemplatesVisible() const;
        void setUserTemplatesVisible(bool visible);

        bool getLocalSRsVisible() const;
        void setLocalSRsVisible(bool visible);

        int getConsoleRefreshInterval() const;
        void setConsoleRefreshInterval(int seconds);

        int getGraphUpdateInterval() const;
        void setGraphUpdateInterval(int seconds);

        // Tree view settings
        enum TreeViewMode
        {
            Infrastructure,
            Organization,
            Custom
        };

        TreeViewMode getTreeViewMode() const;
        void setTreeViewMode(TreeViewMode mode);

        QStringList getExpandedTreeItems() const;
        void setExpandedTreeItems(const QStringList& items);

        // Debug settings
        bool getDebugConsoleVisible() const;
        void setDebugConsoleVisible(bool visible);

        int getLogLevel() const;
        void setLogLevel(int level);

        // Network settings
        QString getProxyServer() const;
        void setProxyServer(const QString& server);

        int getProxyPort() const;
        void setProxyPort(int port);

        bool getUseProxy() const;
        void setUseProxy(bool use);

        QString getProxyUsername() const;
        void setProxyUsername(const QString& username);

        // Recent files/paths
        QStringList getRecentExportPaths() const;
        void addRecentExportPath(const QString& path);

        QStringList getRecentImportPaths() const;
        void addRecentImportPath(const QString& path);

        // Miscellaneous
        QVariant GetValue(const QString& key, const QVariant& defaultValue = QVariant()) const;
        void SetValue(const QString& key, const QVariant& value);

        void Sync();  // Force write to disk
        void Clear(); // Clear all settings
        
        QString GetFileName() const; // Get the settings file path

        // Main password derived key + verification data
        QByteArray GetMainKey() const;
        void SetMainKey(const QByteArray& key);
        QByteArray GetMainPasswordHash() const;
        void SetMainPasswordHash(const QByteArray& passwordHash);
        QByteArray GetMainPasswordHashSalt() const;
        void SetMainPasswordHashSalt(const QByteArray& salt);
        QByteArray GetMainKeySalt() const;
        void SetMainKeySalt(const QByteArray& salt);
        int GetMainKdfIterations() const;
        void SetMainKdfIterations(int iterations);

        // Save and restore settings (matches C# Properties.Settings.Default)
        bool GetSaveSession() const;
        void SetSaveSession(bool save);

        bool GetSavePasswords() const;
        void SetSavePasswords(bool save);
        
        bool GetRequirePass() const;
        void SetRequirePass(bool require);

    signals:
        void settingsChanged(const QString& key);

    private:
        explicit SettingsManager(QObject* parent = nullptr);
        ~SettingsManager();
        SettingsManager(const SettingsManager&) = delete;
        SettingsManager& operator=(const SettingsManager&) = delete;

        QSettings* m_settings;
        QByteArray m_mainKey;

        // Cached simple settings (loaded from QSettings on startup)
        bool m_autoReconnect;
        bool m_checkForUpdates;
        QString m_defaultExportPath;
        QString m_defaultImportPath;
        bool m_confirmOnExit;
        bool m_showHiddenObjects;
        bool m_defaultTemplatesVisible;
        bool m_userTemplatesVisible;
        bool m_localSRsVisible;
        int m_consoleRefreshInterval;
        int m_graphUpdateInterval;
        TreeViewMode m_treeViewMode;
        QStringList m_expandedTreeItems;
        bool m_debugConsoleVisible;
        int m_logLevel;
        QString m_proxyServer;
        int m_proxyPort;
        bool m_useProxy;
        QString m_proxyUsername;
        QStringList m_recentExportPaths;
        QStringList m_recentImportPaths;
        QString m_lastConnectedServer;
        QStringList m_serverHistory;
        bool m_saveSession;
        bool m_savePasswords;
        bool m_requirePass;
        QByteArray m_mainPasswordHash;
        QByteArray m_mainPasswordHashSalt;
        QByteArray m_mainKeySalt;
        int m_mainKdfIterations;
        QString m_localKey;

        // Helper methods
        QString encryptPassword(const QString& password) const;
        QString decryptPassword(const QString& encrypted) const;
        void addToRecentList(const QString& settingsKey, const QString& path, int maxItems = 10);
};

#endif // SETTINGSMANAGER_H
