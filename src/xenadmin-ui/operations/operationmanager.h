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

#ifndef OPERATIONMANAGER_H
#define OPERATIONMANAGER_H

#include <QObject>
#include <QPointer>
#include <QDateTime>
#include <QHash>
#include <QList>

#include <xen/asyncoperation.h>

class MeddlingActionManager;

class OperationManager : public QObject
{
    Q_OBJECT

public:
    struct OperationRecord
    {
        QPointer<AsyncOperation> operation;
        QString title;
        QString description;
        QString errorMessage;
        int progress = 0;
        AsyncOperation::OperationState state = AsyncOperation::NotStarted;
        QDateTime started;
        QDateTime finished;
    };

    static OperationManager* instance();
    ~OperationManager() override;

    const QList<OperationRecord*>& records() const;

    void registerOperation(AsyncOperation* operation);

    /**
     * Remove a record from the operation history.
     * C# Equivalent: ConnectionsManager.History.Remove()
     * @param record The record to remove
     */
    void removeRecord(OperationRecord* record);

    /**
     * Remove multiple records from the operation history.
     * C# Equivalent: ConnectionsManager.History.RemoveAll()
     * @param records The records to remove
     */
    void removeRecords(const QList<OperationRecord*>& records);

    // Cleanup for shutdown - removes UUIDs from all task.other_config
    void prepareAllOperationsForRestart();

    // Task rehydration manager accessor
    MeddlingActionManager* meddlingActionManager() const;

signals:
    void recordAdded(OperationManager::OperationRecord* record);
    void recordUpdated(OperationManager::OperationRecord* record);
    void recordRemoved(OperationManager::OperationRecord* record);

    // Equivalent to C# ActionBase.NewAction event
    void newOperation(AsyncOperation* operation);

private:
    explicit OperationManager(QObject* parent = nullptr);
    void connectOperationSignals(AsyncOperation* operation, OperationRecord* record);
    void updateRecordState(OperationRecord* record, AsyncOperation::OperationState state);
    void updateRecordProgress(OperationRecord* record, int percent);
    void updateRecordTitle(OperationRecord* record, const QString& title);
    void updateRecordDescription(OperationRecord* record, const QString& description);
    void updateRecordError(OperationRecord* record, const QString& error);

    static OperationManager* s_instance;
    QList<OperationRecord*> m_records;
    QHash<AsyncOperation*, OperationRecord*> m_lookup;
    MeddlingActionManager* m_rehydrationManager;
};

#endif // OPERATIONMANAGER_H
