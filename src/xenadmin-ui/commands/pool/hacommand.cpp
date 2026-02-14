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

#include "hacommand.h"
#include "xenlib/operations/operationmanager.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/actions/pool/disablehaaction.h"
#include "xenlib/xen/actions/pool/enablehaaction.h"
#include "xenlib/xen/session.h"

HACommand::HACommand(MainWindow* mainWindow, QObject* parent) : PoolCommand(mainWindow, parent)
{
}

QSharedPointer<Pool> HACommand::getTargetPool() const
{
    QSharedPointer<Pool> selectedPool = this->getPool();
    if (selectedPool && selectedPool->IsValid())
        return selectedPool;

    QSharedPointer<XenObject> selectedObject = this->GetObject();
    if (!selectedObject || !selectedObject->GetConnection())
        return QSharedPointer<Pool>();

    XenCache* cache = selectedObject->GetConnection()->GetCache();
    if (!cache)
        return QSharedPointer<Pool>();

    return cache->GetPoolOfOne();
}

bool HACommand::CanRunHACommand() const
{
    return this->GetCantRunReason() == CantRunReason::None;
}

HACommand::CantRunReason HACommand::GetCantRunReason() const
{
    QSharedPointer<Pool> pool = this->getTargetPool();
    if (!pool || !pool->IsValid())
        return CantRunReason::NoPool;

    if (!pool->IsVisible())
        return CantRunReason::PoolHidden;

    if (!this->isPoolConnected(pool))
        return CantRunReason::PoolDisconnected;

    if (!this->hasCoordinator(pool))
        return CantRunReason::NoCoordinator;

    if (this->isPoolLocked(pool))
        return CantRunReason::PoolLocked;

    if (this->hasActiveHAAction(pool))
        return CantRunReason::ActiveHaAction;

    if (!this->CanRunOnPool(pool))
    {
        if (this->hasPendingPoolSecretRotationConflict(pool))
            return CantRunReason::PsrPendingConflict;
        return CantRunReason::UnsupportedState;
    }

    return CantRunReason::None;
}

QString HACommand::GetCantRunReasonText(CantRunReason reason) const
{
    switch (reason)
    {
        case CantRunReason::None:
            return QString();
        case CantRunReason::NoPool:
            return tr("No pool is available for this operation.");
        case CantRunReason::PoolHidden:
            return tr("HA can only be configured on pooled hosts.");
        case CantRunReason::PoolDisconnected:
            return tr("The pool connection is not active.");
        case CantRunReason::NoCoordinator:
            return tr("The pool coordinator is unavailable.");
        case CantRunReason::PoolLocked:
            return tr("A pool operation is currently in progress.");
        case CantRunReason::ActiveHaAction:
            return tr("Another HA operation is already running.");
        case CantRunReason::PsrPendingConflict:
            return tr("HA cannot be configured while pool secret rotation is pending.");
        case CantRunReason::UnsupportedState:
            return tr("This HA operation is not available in the current pool state.");
    }
    return tr("Operation is not available.");
}

bool HACommand::isPoolConnected(const QSharedPointer<Pool>& pool) const
{
    XenConnection* conn = pool ? pool->GetConnection() : nullptr;
    return conn && conn->IsConnected();
}

bool HACommand::hasCoordinator(const QSharedPointer<Pool>& pool) const
{
    if (!pool)
        return false;

    QSharedPointer<Host> coordinator = pool->GetMasterHost();
    return coordinator && coordinator->IsValid();
}

bool HACommand::isPoolLocked(const QSharedPointer<Pool>& pool) const
{
    return pool && !pool->CurrentOperations().isEmpty();
}

bool HACommand::hasActiveHAAction(const QSharedPointer<Pool>& pool) const
{
    XenConnection* conn = pool ? pool->GetConnection() : nullptr;
    if (!conn)
        return false;

    const QList<OperationManager::OperationRecord*>& records = OperationManager::instance()->GetRecords();
    for (OperationManager::OperationRecord* record : records)
    {
        if (!record || !record->operation)
            continue;

        if (record->state != AsyncOperation::NotStarted && record->state != AsyncOperation::Running)
            continue;

        AsyncOperation* op = record->operation;
        if (op->GetConnection() != conn)
            continue;

        if (qobject_cast<EnableHAAction*>(op) || qobject_cast<DisableHAAction*>(op))
            return true;
    }

    return false;
}

bool HACommand::allStatefilesUnresolvable(const QSharedPointer<Pool>& pool) const
{
    if (!pool)
        return true;

    const QStringList statefiles = pool->HAStatefiles();
    if (statefiles.isEmpty())
        return true;

    XenCache* cache = pool->GetCache();
    if (!cache)
        return true;

    for (const QString& vdiRef : statefiles)
    {
        if (cache->ResolveObject<VDI>(vdiRef))
            return false;
    }

    return true;
}

bool HACommand::hasPendingPoolSecretRotationConflict(const QSharedPointer<Pool>& pool) const
{
    if (!pool || !pool->IsPsrPending())
        return false;

    for (const QSharedPointer<Host>& host : pool->GetHosts())
    {
        if (host && host->RestrictPoolSecretRotation())
            return false;
    }

    return true;
}

bool HACommand::ConnectionRequiresRbac(const QSharedPointer<Pool>& pool) const
{
    XenConnection* conn = pool ? pool->GetConnection() : nullptr;
    if (!conn)
        return false;
    XenAPI::Session* session = conn->GetSession();
    if (!session || !session->IsLoggedIn())
        return false;
    return !session->IsLocalSuperuser() && session->ApiVersionMeets(APIVersion::API_1_7);
}

bool HACommand::HasRequiredPermissions(const QSharedPointer<Pool>& pool, const QStringList& requiredMethods, QStringList* missingMethods) const
{
    if (missingMethods)
        missingMethods->clear();

    if (!this->ConnectionRequiresRbac(pool))
        return true;

    XenConnection* conn = pool ? pool->GetConnection() : nullptr;
    XenAPI::Session* session = conn ? conn->GetSession() : nullptr;
    if (!session)
        return false;

    const QStringList permissions = session->GetPermissions();
    if (permissions.isEmpty())
        return true;

    QStringList missing;
    for (const QString& method : requiredMethods)
    {
        if (!permissions.contains(method, Qt::CaseInsensitive))
            missing.append(method);
    }

    if (missingMethods)
        *missingMethods = missing;

    return missing.isEmpty();
}
