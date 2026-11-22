/*
 * Copyright (c) 2026, Petr Bena <petr@bena.rocks>
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

#ifndef VMSSACTIONS_H
#define VMSSACTIONS_H

#include "../../asyncoperation.h"
#include <QDateTime>
#include <QStringList>
#include <QVariantMap>

class Host;
class VMSS;

/**
 * Creates a snapshot schedule, puts VMs in it and optionally runs it once.
 * C# XenModel/Actions/VMSS/CreateVMPolicy.cs
 */
class XENLIB_EXPORT CreateVMPolicyAction : public AsyncOperation
{
    Q_OBJECT

    public:
        //! \\a record: name_label, name_description, enabled, type, retained_snapshots, frequency, schedule
        CreateVMPolicyAction(XenConnection* connection, const QVariantMap& record, const QStringList& vmRefs, bool runNow, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QVariantMap m_record;
        QStringList m_vmRefs;
        bool m_runNow;
};

/**
 * Takes the VMs out of schedules and destroys the schedules.
 * C# XenModel/Actions/VMSS/DestroyPolicyAction.cs
 */
class XENLIB_EXPORT DestroyPolicyAction : public AsyncOperation
{
    Q_OBJECT

    public:
        DestroyPolicyAction(XenConnection* connection, const QList<QSharedPointer<VMSS>>& policies, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        struct Item
        {
            QString ref;
            QString name;
            QStringList vmRefs;
        };
        QList<Item> m_items;
};

/**
 * Makes a schedule's members exactly the given VMs.
 * C# XenModel/Actions/VMSS/AssignVMsToPolicyAction.cs
 */
class XENLIB_EXPORT AssignVMsToPolicyAction : public AsyncOperation
{
    Q_OBJECT

    public:
        AssignVMsToPolicyAction(QSharedPointer<VMSS> policy, const QStringList& vmRefs, bool suppressHistory, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_policyRef;
        QStringList m_currentVmRefs;
        QStringList m_vmRefs;
};

/**
 * Takes VMs out of a schedule.
 * C# XenModel/Actions/VMSS/AssignVMsToPolicyAction.cs (RemoveVMsFromPolicyAction)
 */
class XENLIB_EXPORT RemoveVMsFromPolicyAction : public AsyncOperation
{
    Q_OBJECT

    public:
        RemoveVMsFromPolicyAction(QSharedPointer<VMSS> policy, const QStringList& vmRefs, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QStringList m_vmRefs;
};

/**
 * Turns a schedule on if it's off and off if it's on.
 * C# XenModel/Actions/VMSS/ChangePolicyEnabledAction.cs
 */
class XENLIB_EXPORT ChangePolicyEnabledAction : public AsyncOperation
{
    Q_OBJECT

    public:
        explicit ChangePolicyEnabledAction(QSharedPointer<VMSS> policy, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_policyRef;
        QString m_name;
        bool m_enable;
};

/**
 * Runs a schedule once now.
 * C# XenModel/Actions/VMSS/RunPolicyNowAction.cs
 */
class XENLIB_EXPORT RunPolicyNowAction : public AsyncOperation
{
    Q_OBJECT

    public:
        explicit RunPolicyNowAction(QSharedPointer<VMSS> policy, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_policyRef;
        QString m_name;
};

/**
 * Reads a server's local wall-clock time, which snapshot schedules are expressed in.
 * C# XenAdmin/Actions/GUIActions/GetServerLocalTimeAction.cs (no history entry)
 */
class XENLIB_EXPORT GetServerLocalTimeAction : public AsyncOperation
{
    Q_OBJECT

    public:
        explicit GetServerLocalTimeAction(QSharedPointer<Host> host, QObject* parent = nullptr);

        //! False until the time was read
        bool HasServerTime() const { return this->m_hasTime; }
        //! The server's wall-clock time (Qt::UTC spec, no time zone meaning)
        QDateTime ServerLocalTime() const { return this->m_serverLocalTime; }
        //! C# ServerClientTimeZoneDiff: seconds to add to a server wall-clock time to get the client's
        qint64 ServerClientTimeZoneDiff() const { return this->m_diffSeconds; }

        //! The client's current wall-clock time in the same form as ServerLocalTime
        static QDateTime ClientLocalNow();

    protected:
        void run() override;

    private:
        QString m_hostRef;
        bool m_hasTime = false;
        QDateTime m_serverLocalTime;
        qint64 m_diffSeconds = 0;
};

#endif // VMSSACTIONS_H
