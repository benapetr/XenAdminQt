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

#ifndef CONNECTIONWRAPPERWITHMORESTUFF_H
#define CONNECTIONWRAPPERWITHMORESTUFF_H

// Qt port of XenAdmin's ConnectionWrapperWithMoreStuff (XenAdmin/Controls).
// Used by New Pool dialog to wrap connections with UI metadata (status text,
// eligibility, and coordinator selection) for display in CustomTreeView.

#include "customtreenode.h"
#include "xenlib/xen/pooljoinrules.h"

class XenConnection;

class ConnectionWrapperWithMoreStuff : public CustomTreeNode
{
    public:
        explicit ConnectionWrapperWithMoreStuff(XenConnection* connection);

        XenConnection* Connection() const { return m_connection; }

        void SetCoordinator(ConnectionWrapperWithMoreStuff* coordinator);

        bool WillBeCoordinator() const { return m_reason == PoolJoinRules::Reason::WillBeCoordinator; }
        bool CanBeCoordinator() const;
        bool AllowedAsSupporter() const { return m_reason == PoolJoinRules::Reason::Allowed; }

        void Refresh();

        QString ToString() const override;

    protected:
        int SameLevelSortOrder(const CustomTreeNode* other) const override;

    private:
        XenConnection* m_connection = nullptr;
        XenConnection* m_coordinatorConnection = nullptr;
        PoolJoinRules::Reason m_reason = PoolJoinRules::Reason::NotConnected;
};

Q_DECLARE_METATYPE(ConnectionWrapperWithMoreStuff*)

#endif // CONNECTIONWRAPPERWITHMORESTUFF_H
