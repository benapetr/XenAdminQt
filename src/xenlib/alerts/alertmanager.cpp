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

#include "alertmanager.h"
#include <QMutexLocker>
#include <QDebug>

using namespace XenLib;

AlertManager* AlertManager::s_instance = nullptr;

AlertManager* AlertManager::instance()
{
    if (!s_instance)
        s_instance = new AlertManager();
    return s_instance;
}

AlertManager::AlertManager(QObject* parent) : QObject(parent)
{
}

AlertManager::~AlertManager()
{
    this->ClearAllAlerts();
}

void AlertManager::AddAlert(Alert* alert)
{
    if (!alert)
        return;
    
    {
        QMutexLocker locker(&this->m_mutex);
        this->m_alerts.append(alert);
    }
    
    emit this->alertAdded(alert);
    emit this->collectionChanged();
}

void AlertManager::AddAlerts(const QList<Alert*>& alerts)
{
    if (alerts.isEmpty())
        return;

    // The lock only covers the list. Signals are emitted and predicates run without it:
    // receivers call back into this class (e.g. GetNonDismissingAlerts), and QMutex is not
    // recursive, unlike the C# lock, so holding it there deadlocked.
    {
        QMutexLocker locker(&this->m_mutex);
        this->m_alerts.append(alerts);
    }

    for (Alert* alert : alerts)
    {
        emit this->alertAdded(alert);
    }
    emit this->collectionChanged();
}

void AlertManager::RemoveAlert(Alert* alert)
{
    if (!alert)
        return;

    bool removed = false;
    {
        QMutexLocker locker(&this->m_mutex);
        removed = this->m_alerts.removeOne(alert);
    }

    // Only the caller that took it out of the list deletes it
    if (removed)
    {
        emit this->alertRemoved(alert);
        emit this->collectionChanged();
        delete alert;
    }
}

void AlertManager::RemoveAlerts(const std::function<bool(Alert*)>& predicate)
{
    // The predicate runs on a copy, outside the lock (it may call FindAlert and the like)
    QList<Alert*> candidates;
    for (Alert* alert : this->GetAllAlerts())
    {
        if (predicate(alert))
            candidates.append(alert);
    }

    QList<Alert*> removed;
    {
        QMutexLocker locker(&this->m_mutex);
        for (Alert* alert : candidates)
        {
            if (this->m_alerts.removeOne(alert))
                removed.append(alert);
        }
    }

    for (Alert* alert : removed)
    {
        emit this->alertRemoved(alert);
        delete alert;
    }

    if (!removed.isEmpty())
        emit this->collectionChanged();
}

Alert* AlertManager::FindAlert(const QString& uuid) const
{
    QMutexLocker locker(&this->m_mutex);
    for (Alert* alert : this->m_alerts)
    {
        if (alert && alert->GetUUID() == uuid)
            return alert;
    }
    return nullptr;
}

Alert* AlertManager::FindAlert(const std::function<bool(Alert*)>& predicate) const
{
    QMutexLocker locker(&this->m_mutex);
    for (Alert* alert : this->m_alerts)
    {
        if (alert && predicate(alert))
            return alert;
    }
    return nullptr;
}

int AlertManager::FindAlertIndex(const std::function<bool(Alert*)>& predicate) const
{
    QMutexLocker locker(&this->m_mutex);
    for (int i = 0; i < this->m_alerts.size(); ++i)
    {
        if (this->m_alerts[i] && predicate(this->m_alerts[i]))
            return i;
    }
    return -1;
}

int AlertManager::AlertCount() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_alerts.count();
}

int AlertManager::NonDismissingAlertCount() const
{
    QMutexLocker locker(&this->m_mutex);
    int count = 0;
    for (Alert* alert : this->m_alerts)
    {
        if (alert && !alert->IsDismissing())
            count++;
    }
    return count;
}

QList<Alert*> AlertManager::GetNonDismissingAlerts() const
{
    QMutexLocker locker(&this->m_mutex);
    QList<Alert*> result;
    for (Alert* alert : this->m_alerts)
    {
        if (alert && !alert->IsDismissing())
            result.append(alert);
    }
    return result;
}

QList<Alert*> AlertManager::GetAllAlerts() const
{
    QMutexLocker locker(&this->m_mutex);
    return this->m_alerts;
}

void AlertManager::ClearAllAlerts()
{
    QList<Alert*> alerts;
    {
        QMutexLocker locker(&this->m_mutex);
        alerts.swap(this->m_alerts);
    }

    // Alert destructors run without the lock as well
    qDeleteAll(alerts);
    emit this->collectionChanged();
}

void AlertManager::NotifyCollectionChanged()
{
    emit this->collectionChanged();
}
