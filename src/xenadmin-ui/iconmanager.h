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

#ifndef ICONMANAGER_H
#define ICONMANAGER_H

#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QVariantMap>
#include <QMap>

/**
 * @brief Manages icons for different object types and states
 *
 * Provides appropriate icons based on Xen API object types, power states,
 * and operational status. Icons are cached for performance.
 */
class IconManager
{
public:
    static IconManager& instance();

    // Icon retrieval methods
    QIcon getIconForObject(const QString& objectType, const QVariantMap& objectData) const;
    QIcon getIconForVM(const QVariantMap& vmData) const;
    QIcon getIconForHost(const QVariantMap& hostData) const;
    QIcon getIconForPool(const QVariantMap& poolData) const;
    QIcon getIconForSR(const QVariantMap& srData) const;
    QIcon getIconForNetwork(const QVariantMap& networkData) const;

    // Static icon getters
    QIcon getConnectedIcon() const;
    QIcon getDisconnectedIcon() const;
    QIcon getConnectingIcon() const;

private:
    IconManager();
    ~IconManager() = default;
    IconManager(const IconManager&) = delete;
    IconManager& operator=(const IconManager&) = delete;

    // Helper methods
    QIcon createStatusIcon(const QColor& color, const QString& symbol = QString()) const;
    QIcon createTextIcon(const QString& text, const QColor& bgColor) const;
    QString getVMPowerState(const QVariantMap& vmData) const;
    bool isVMOperationInProgress(const QVariantMap& vmData) const;

    // Icon cache
    mutable QMap<QString, QIcon> m_iconCache;

    // Preloaded icons
    QIcon m_connectedIcon;
    QIcon m_disconnectedIcon;
    QIcon m_connectingIcon;
};

#endif // ICONMANAGER_H
