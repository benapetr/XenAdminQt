/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 */

#include "alertmanager.h"
#include <QMutexLocker>
#include <QDebug>

AlertManager* AlertManager::s_instance = nullptr;

AlertManager* AlertManager::instance()
{
    if (!s_instance)
        s_instance = new AlertManager();
    return s_instance;
}

AlertManager::AlertManager(QObject* parent)
    : QObject(parent)
{
}

AlertManager::~AlertManager()
{
    this->clearAllAlerts();
}

void AlertManager::addAlert(Alert* alert)
{
    if (!alert)
        return;
    
    QMutexLocker locker(&this->m_mutex);
    this->m_alerts.append(alert);
    
    emit this->alertAdded(alert);
    emit this->collectionChanged();
}

void AlertManager::addAlerts(const QList<Alert*>& alerts)
{
    if (alerts.isEmpty())
        return;
    
    QMutexLocker locker(&this->m_mutex);
    this->m_alerts.append(alerts);
    
    for (Alert* alert : alerts)
    {
        emit this->alertAdded(alert);
    }
    emit this->collectionChanged();
}

void AlertManager::removeAlert(Alert* alert)
{
    if (!alert)
        return;
    
    QMutexLocker locker(&this->m_mutex);
    if (this->m_alerts.removeOne(alert))
    {
        emit this->alertRemoved(alert);
        emit this->collectionChanged();
        delete alert;
    }
}

void AlertManager::removeAlerts(const std::function<bool(Alert*)>& predicate)
{
    QMutexLocker locker(&this->m_mutex);
    
    QList<Alert*> toRemove;
    for (Alert* alert : this->m_alerts)
    {
        if (predicate(alert))
            toRemove.append(alert);
    }
    
    for (Alert* alert : toRemove)
    {
        this->m_alerts.removeOne(alert);
        emit this->alertRemoved(alert);
        delete alert;
    }
    
    if (!toRemove.isEmpty())
        emit this->collectionChanged();
}

Alert* AlertManager::findAlert(const QString& uuid) const
{
    QMutexLocker locker(&this->m_mutex);
    for (Alert* alert : this->m_alerts)
    {
        if (alert && alert->uuid() == uuid)
            return alert;
    }
    return nullptr;
}

Alert* AlertManager::findAlert(const std::function<bool(Alert*)>& predicate) const
{
    QMutexLocker locker(&this->m_mutex);
    for (Alert* alert : this->m_alerts)
    {
        if (alert && predicate(alert))
            return alert;
    }
    return nullptr;
}

int AlertManager::findAlertIndex(const std::function<bool(Alert*)>& predicate) const
{
    QMutexLocker locker(&this->m_mutex);
    for (int i = 0; i < this->m_alerts.size(); ++i)
    {
        if (this->m_alerts[i] && predicate(this->m_alerts[i]))
            return i;
    }
    return -1;
}

int AlertManager::alertCount() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_alerts.count();
}

int AlertManager::nonDismissingAlertCount() const
{
    QMutexLocker locker(&this->m_mutex);
    int count = 0;
    for (Alert* alert : this->m_alerts)
    {
        if (alert && !alert->dismissing())
            count++;
    }
    return count;
}

QList<Alert*> AlertManager::nonDismissingAlerts() const
{
    QMutexLocker locker(&this->m_mutex);
    QList<Alert*> result;
    for (Alert* alert : this->m_alerts)
    {
        if (alert && !alert->dismissing())
            result.append(alert);
    }
    return result;
}

QList<Alert*> AlertManager::allAlerts() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_alerts;
}

void AlertManager::clearAllAlerts()
{
    QMutexLocker locker(&this->m_mutex);
    qDeleteAll(this->m_alerts);
    this->m_alerts.clear();
    emit this->collectionChanged();
}
