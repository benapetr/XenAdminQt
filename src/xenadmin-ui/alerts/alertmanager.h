/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
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
