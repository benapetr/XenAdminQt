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

#ifndef ALERTMANAGER_H
#define ALERTMANAGER_H

#include <QObject>
#include <QList>
#include <QMutex>
#include "alert.h"

/**
 * Singleton manager for the alert collection.
 * C# Reference: XenModel/Alerts/Types/Alert.cs (static methods and XenCenterAlerts collection)
 * 
 * Manages the global collection of alerts, provides thread-safe access,
 * and emits signals when the collection changes.
 */
class AlertManager : public QObject
{
    Q_OBJECT

public:
    static AlertManager* instance();
    ~AlertManager() override;

    // Alert collection management
    // C# Reference: Alert.cs line 44 - AddAlert
    void addAlert(Alert* alert);
    void addAlerts(const QList<Alert*>& alerts);
    
    // C# Reference: Alert.cs line 58 - RemoveAlert
    void removeAlert(Alert* alert);
    void removeAlerts(const std::function<bool(Alert*)>& predicate);
    
    // C# Reference: Alert.cs line 83 - FindAlert
    Alert* findAlert(const QString& uuid) const;
    Alert* findAlert(const std::function<bool(Alert*)>& predicate) const;
    int findAlertIndex(const std::function<bool(Alert*)>& predicate) const;
    
    // C# Reference: Alert.cs line 123 - AlertCount
    int alertCount() const;
    int nonDismissingAlertCount() const;
    
    // C# Reference: Alert.cs line 145 - NonDismissingAlerts
    QList<Alert*> nonDismissingAlerts() const;
    QList<Alert*> allAlerts() const;
    
    // Clear all alerts (for cleanup)
    void clearAllAlerts();

signals:
    // C# Reference: Alert.cs line 199 - RegisterAlertCollectionChanged
    void alertAdded(Alert* alert);
    void alertRemoved(Alert* alert);
    void alertChanged(Alert* alert);
    void collectionChanged();

private:
    explicit AlertManager(QObject* parent = nullptr);
    
    static AlertManager* s_instance;
    QList<Alert*> m_alerts;
    mutable QMutex m_mutex;
};

#endif // ALERTMANAGER_H
