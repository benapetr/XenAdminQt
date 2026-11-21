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

#ifndef VMAPPLIANCEACTIONS_H
#define VMAPPLIANCEACTIONS_H

#include "../../asyncoperation.h"
#include <QStringList>
#include <QVariantMap>

class VMAppliance;

/**
 * Creates a vApp and puts VMs in it.
 * C# XenModel/Actions/VMAppliances/CreateVMApplianceAction.cs
 */
class XENLIB_EXPORT CreateVMApplianceAction : public AsyncOperation
{
    Q_OBJECT

    public:
        //! \a record holds at least name_label and name_description
        CreateVMApplianceAction(XenConnection* connection, const QVariantMap& record, const QStringList& vmRefs, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QVariantMap m_record;
        QStringList m_vmRefs;
};

/**
 * Takes the VMs out of vApps and destroys the vApps.
 * C# XenModel/Actions/VMAppliances/DestroyVMApplianceAction.cs
 */
class XENLIB_EXPORT DestroyVMApplianceAction : public AsyncOperation
{
    Q_OBJECT

    public:
        DestroyVMApplianceAction(XenConnection* connection, const QList<QSharedPointer<VMAppliance>>& appliances, QObject* parent = nullptr);

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
 * Makes a vApp's members exactly the given VMs.
 * C# XenModel/Actions/VMAppliances/AssignVMsToVMApplianceAction.cs
 */
class XENLIB_EXPORT AssignVMsToVMApplianceAction : public AsyncOperation
{
    Q_OBJECT

    public:
        AssignVMsToVMApplianceAction(QSharedPointer<VMAppliance> appliance, const QStringList& vmRefs, bool suppressHistory, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QString m_applianceRef;
        QStringList m_currentVmRefs;
        QStringList m_vmRefs;
};

/**
 * Takes VMs out of a vApp.
 * C# XenModel/Actions/VMAppliances/AssignVMsToVMApplianceAction.cs (RemoveVMsFromVMApplianceAction)
 */
class XENLIB_EXPORT RemoveVMsFromVMApplianceAction : public AsyncOperation
{
    Q_OBJECT

    public:
        RemoveVMsFromVMApplianceAction(QSharedPointer<VMAppliance> appliance, const QStringList& vmRefs, QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        QStringList m_vmRefs;
};

#endif // VMAPPLIANCEACTIONS_H
