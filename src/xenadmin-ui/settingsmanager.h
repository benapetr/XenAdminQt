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
        static void SetConfigDir(const QString& path);

        void Load();
        void Save();

        // Window state
        void SaveMainWindowGeometry(const QByteArray& geometry);
        QByteArray LoadMainWindowGeometry() const;
        void SaveMainWindowState(const QByteArray& state);
        QByteArray LoadMainWindowState() const;
        void SaveSplitterState(const QByteArray& state);
        QByteArray LoadSplitterState() const;

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

        void SaveConnection(const QString& id, const ConnectionInfo& info);
        QList<ConnectionInfo> LoadConnections() const;
        void RemoveConnection(const QString& id);
        QString GetLastConnectedServer() const;
        void SetLastConnectedServer(const QString& id);

        // Server history (matches C# Settings.UpdateServerHistory)
        QStringList GetServerHistory() const;
        void UpdateServerHistory(const QString& hostnameWithPort);

        // Connection profiles
        void SaveConnectionProfile(const ConnectionProfile& profile);
        QList<ConnectionProfile> LoadConnectionProfiles() const;
        void RemoveConnectionProfile(const QString& name);
        ConnectionProfile GetLastConnectionProfile() const;
        void SetLastConnectionProfile(const QString& name);

        bool GetAutoReconnect() const;
        void SetAutoReconnect(bool autoReconnect);

        bool GetCheckForUpdates() const;
        void SetCheckForUpdates(bool check);

        QString GetDefaultExportPath() const;
        void SetDefaultExportPath(const QString& path);

        QString GetDefaultImportPath() const;
        void SetDefaultImportPath(const QString& path);

        bool GetConfirmOnExit() const;
        void SetConfirmOnExit(bool confirm);

        bool GetShowHiddenObjects() const;
        void SetShowHiddenObjects(bool show);

        bool GetDefaultTemplatesVisible() const;
        void SetDefaultTemplatesVisible(bool visible);

        bool GetUserTemplatesVisible() const;
        void SetUserTemplatesVisible(bool visible);

        bool GetLocalSRsVisible() const;
        void SetLocalSRsVisible(bool visible);

        int GetConsoleRefreshInterval() const;
        void SetConsoleRefreshInterval(int seconds);

        int GetGraphUpdateInterval() const;
        void SetGraphUpdateInterval(int seconds);
        
        // Display settings
        bool GetFillAreaUnderGraphs() const;
        void SetFillAreaUnderGraphs(bool fill);
        bool GetRememberLastSelectedTab() const;
        void SetRememberLastSelectedTab(bool remember);
        
        // Confirmation settings
        bool GetDoNotConfirmDismissAlerts() const;
        void SetDoNotConfirmDismissAlerts(bool doNotConfirm);
        bool GetDoNotConfirmDismissUpdates() const;
        void SetDoNotConfirmDismissUpdates(bool doNotConfirm);
        bool GetDoNotConfirmDismissEvents() const;
        void SetDoNotConfirmDismissEvents(bool doNotConfirm);
        bool GetIgnoreOvfValidationWarnings() const;
        void SetIgnoreOvfValidationWarnings(bool ignoreWarnings);

        // Connection/proxy settings
        enum ProxySetting
        {
            DirectConnection = 0,
            SystemProxy = 1,
            SpecifiedProxy = 2
        };

        ProxySetting GetConnectionProxySetting() const;
        void SetConnectionProxySetting(ProxySetting setting);
        QString GetConnectionProxyAddress() const;
        void SetConnectionProxyAddress(const QString& address);
        int GetConnectionProxyPort() const;
        void SetConnectionProxyPort(int port);
        bool GetBypassProxyForServers() const;
        void SetBypassProxyForServers(bool bypass);
        bool GetProvideProxyAuthentication() const;
        void SetProvideProxyAuthentication(bool provide);
        int GetProxyAuthenticationMethod() const;
        void SetProxyAuthenticationMethod(int method);
        QString GetConnectionProxyUsername() const;
        void SetConnectionProxyUsername(const QString& username);
        QString GetConnectionProxyPassword() const;
        void SetConnectionProxyPassword(const QString& password);
        int GetConnectionTimeoutMs() const;
        void SetConnectionTimeoutMs(int timeoutMs);
        void ApplyProxySettings() const;

        // Tree view settings
        enum TreeViewMode
        {
            Infrastructure,
            Organization,
            Custom
        };

        TreeViewMode GetTreeViewMode() const;
        void SetTreeViewMode(TreeViewMode mode);

        QStringList GetExpandedTreeItems() const;
        void SetExpandedTreeItems(const QStringList& items);

        // Debug settings
        bool GetDebugConsoleVisible() const;
        void SetDebugConsoleVisible(bool visible);

        int GetLogLevel() const;
        void SetLogLevel(int level);

        // Network settings
        QString GetProxyServer() const;
        void SetProxyServer(const QString& server);

        int GetProxyPort() const;
        void SetProxyPort(int port);

        bool GetUseProxy() const;
        void SetUseProxy(bool use);

        QString GetProxyUsername() const;
        void SetProxyUsername(const QString& username);

        // Recent files/paths
        QStringList GetRecentExportPaths() const;
        void AddRecentExportPath(const QString& path);

        QStringList GetRecentImportPaths() const;
        void AddRecentImportPath(const QString& path);

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

        static QString s_configDir;
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
        bool m_fillAreaUnderGraphs;
        bool m_rememberLastSelectedTab;
        bool m_doNotConfirmDismissAlerts;
        bool m_doNotConfirmDismissUpdates;
        bool m_doNotConfirmDismissEvents;
        bool m_ignoreOvfValidationWarnings;
        ProxySetting m_connectionProxySetting;
        QString m_connectionProxyAddress;
        int m_connectionProxyPort;
        bool m_bypassProxyForServers;
        bool m_provideProxyAuthentication;
        int m_proxyAuthenticationMethod;
        QString m_connectionProxyUsernameProtected;
        QString m_connectionProxyPasswordProtected;
        int m_connectionTimeoutMs;
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
        QString encryptPassword_(const QString& password) const;
        QString decryptPassword_(const QString& encrypted) const;
        void addToRecentList_(const QString& settingsKey, const QString& path, int maxItems = 10);
};

#endif // SETTINGSMANAGER_H
