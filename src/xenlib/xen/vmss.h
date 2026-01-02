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

#ifndef VMSS_H
#define VMSS_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>

/*!
 * \brief VM Snapshot Schedule wrapper class
 * 
 * Represents a VM snapshot schedule configuration.
 * Provides access to snapshot scheduling, retention policies, and attached VMs.
 * First published in XenServer 7.2.
 */
class VMSS : public XenObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString uuid READ Uuid)
    Q_PROPERTY(QString nameLabel READ NameLabel)
    Q_PROPERTY(QString nameDescription READ NameDescription)
    Q_PROPERTY(bool enabled READ Enabled)
    Q_PROPERTY(QString type READ Type)
    Q_PROPERTY(qlonglong retainedSnapshots READ RetainedSnapshots)
    Q_PROPERTY(QString frequency READ Frequency)
    Q_PROPERTY(QDateTime lastRunTime READ LastRunTime)
    
    public:
        explicit VMSS(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);

        QString GetObjectType() const override;

        // Basic properties
        QString Uuid() const;
        QString NameLabel() const;
        QString NameDescription() const;
        bool Enabled() const;
        QString Type() const;
        qlonglong RetainedSnapshots() const;
        QString Frequency() const;
        QVariantMap Schedule() const;
        QDateTime LastRunTime() const;
        QStringList VMRefs() const;

        // Helper methods
        bool IsEnabled() const;
        int VMCount() const;
};

#endif // VMSS_H
