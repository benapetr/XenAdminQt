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

#include "operationmanager.h"
#include <QDebug>
#include "../actions/meddlingactionmanager.h"
#include <QDebug>
#include "../actions/meddlingaction.h"
#include <QDebug>
#include <QCoreApplication>
#include <QUuid>
#include <utility>

OperationManager* OperationManager::s_instance = nullptr;

OperationManager::OperationManager(QObject* parent)
    : QObject(parent), m_rehydrationManager(new MeddlingActionManager(this))
{
    // Connect rehydration manager's meddlingOperationCreated signal
    connect(m_rehydrationManager, &MeddlingActionManager::meddlingOperationCreated,
            this, &OperationManager::registerOperation);
}

OperationManager* OperationManager::instance()
{
    if (!s_instance)
    {
        QObject* parent = QCoreApplication::instance();
        s_instance = new OperationManager(parent);
    }
    return s_instance;
}

OperationManager::~OperationManager()
{
    for (auto* record : std::as_const(m_records))
        delete record;
    m_records.clear();
    m_lookup.clear();
}

const QList<OperationManager::OperationRecord*>& OperationManager::records() const
{
    return m_records;
}

MeddlingActionManager* OperationManager::meddlingActionManager() const
{
    return m_rehydrationManager;
}

void OperationManager::registerOperation(AsyncOperation* operation)
{
    if (!operation || m_lookup.contains(operation))
        return;

    // Honor SuppressHistory flag (C# ActionBase.SuppressHistory)
    if (operation->suppressHistory())
        return;

    // Assign UUID to operation if not already set (for task rehydration)
    if (operation->operationUuid().isEmpty())
    {
        QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        operation->setOperationUuid(uuid);
    }

    auto* record = new OperationRecord();
    record->operation = operation;
    record->title = operation->title();
    record->description = operation->description();
    record->progress = operation->percentComplete();
    record->state = operation->state();
    record->started = QDateTime::currentDateTime();

    m_records.append(record);
    m_lookup.insert(operation, record);

    connectOperationSignals(operation, record);

    // Emit new operation signal (C# ActionBase.NewAction equivalent)
    emit newOperation(operation);
    emit recordAdded(record);
}

void OperationManager::connectOperationSignals(AsyncOperation* operation, OperationRecord* record)
{
    connect(operation, &AsyncOperation::stateChanged, this, [this, record](AsyncOperation::OperationState state) {
        updateRecordState(record, state);
    });

    connect(operation, &AsyncOperation::progressChanged, this, [this, record](int percent) {
        updateRecordProgress(record, percent);
    });

    connect(operation, &AsyncOperation::titleChanged, this, [this, record](const QString& title) {
        updateRecordTitle(record, title);
    });

    connect(operation, &AsyncOperation::descriptionChanged, this, [this, record](const QString& description) {
        updateRecordDescription(record, description);
    });

    connect(operation, &AsyncOperation::failed, this, [this, record](const QString& error) {
        updateRecordError(record, error);
    });

    connect(operation, &QObject::destroyed, this, [this, record](QObject* obj) {
        auto* asyncOp = qobject_cast<AsyncOperation*>(obj);
        if (asyncOp)
            m_lookup.remove(asyncOp);
        record->operation = nullptr;
        emit recordUpdated(record);
    });
}

void OperationManager::updateRecordState(OperationRecord* record, AsyncOperation::OperationState state)
{
    if (!record)
        return;

    record->state = state;
    if (state == AsyncOperation::Completed ||
        state == AsyncOperation::Cancelled ||
        state == AsyncOperation::Failed)
    {
        record->finished = QDateTime::currentDateTime();
    }

    emit recordUpdated(record);
}

void OperationManager::updateRecordProgress(OperationRecord* record, int percent)
{
    if (!record)
        return;

    record->progress = percent;
    emit recordUpdated(record);
}

void OperationManager::updateRecordTitle(OperationRecord* record, const QString& title)
{
    if (!record)
        return;

    record->title = title;
    emit recordUpdated(record);
}

void OperationManager::updateRecordDescription(OperationRecord* record, const QString& description)
{
    if (!record)
        return;

    record->description = description;
    emit recordUpdated(record);
}

void OperationManager::updateRecordError(OperationRecord* record, const QString& error)
{
    if (!record)
        return;

    record->errorMessage = error;
    emit recordUpdated(record);
}

void OperationManager::removeRecord(OperationRecord* record)
{
    // C# Equivalent: ConnectionsManager.History.Remove()
    if (!record)
        return;

    // Remove from lookup
    if (record->operation)
        this->m_lookup.remove(record->operation);

    // Remove from list
    this->m_records.removeOne(record);

    // Emit signal before deleting
    emit recordRemoved(record);

    // Delete the record
    delete record;
}

void OperationManager::removeRecords(const QList<OperationRecord*>& records)
{
    // C# Equivalent: ConnectionsManager.History.RemoveAll()
    for (auto* record : records)
    {
        this->removeRecord(record);
    }
}

// Cleanup for shutdown - removes UUIDs from all task.other_config
// Matches C# MainWindow.OnClosing() calling PrepareForEventReloadAfterRestart()
void OperationManager::prepareAllOperationsForRestart()
{
    qDebug() << "OperationManager::prepareAllOperationsForRestart: Cleaning up" << m_records.count() << "operations";

    for (OperationRecord* record : m_records)
    {
        if (record && record->operation)
        {
            record->operation->prepareForEventReloadAfterRestart();
        }
    }
}
