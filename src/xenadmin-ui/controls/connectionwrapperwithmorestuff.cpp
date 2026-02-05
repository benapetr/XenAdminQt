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

#include "connectionwrapperwithmorestuff.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/pool.h"
#include "xenlib/xen/pooljoinrules.h"

ConnectionWrapperWithMoreStuff::ConnectionWrapperWithMoreStuff(XenConnection* connection) : m_connection(connection)
{
    this->Refresh();
}

void ConnectionWrapperWithMoreStuff::SetCoordinator(ConnectionWrapperWithMoreStuff* coordinator)
{
    this->m_coordinatorConnection = coordinator ? coordinator->Connection() : nullptr;
}

bool ConnectionWrapperWithMoreStuff::CanBeCoordinator() const
{
    return this->m_reason != PoolJoinRules::Reason::Connecting
        && this->m_reason != PoolJoinRules::Reason::NotConnected
        && this->m_reason != PoolJoinRules::Reason::LicenseRestriction
        && this->m_reason != PoolJoinRules::Reason::IsAPool;
}

void ConnectionWrapperWithMoreStuff::Refresh()
{
    this->m_reason = PoolJoinRules::CanJoinPool(this->m_connection, this->m_coordinatorConnection, true, true);

    this->Description = PoolJoinRules::ReasonMessage(this->m_reason);
    this->Enabled = (this->m_reason == PoolJoinRules::Reason::Allowed);
    this->CheckedIfDisabled = (this->m_reason == PoolJoinRules::Reason::WillBeCoordinator);
    if (this->m_reason == PoolJoinRules::Reason::WillBeCoordinator)
        this->SetState(Qt::Checked);
}

QString ConnectionWrapperWithMoreStuff::ToString() const
{
    if (!this->m_connection)
        return QString();

    QSharedPointer<Host> coordinator = PoolJoinRules::GetCoordinator(this->m_connection);
    if (coordinator && !coordinator->GetName().isEmpty())
        return coordinator->GetName();

    return this->m_connection->GetHostname();
}

int ConnectionWrapperWithMoreStuff::SameLevelSortOrder(const CustomTreeNode* other) const
{
    const auto* otherWrapper = dynamic_cast<const ConnectionWrapperWithMoreStuff*>(other);
    if (!otherWrapper)
        return -1;

    const int diff = static_cast<int>(this->m_reason) - static_cast<int>(otherWrapper->m_reason);
    if (diff < 0)
        return -1;
    if (diff > 0)
        return 1;

    return CustomTreeNode::SameLevelSortOrder(other);
}
