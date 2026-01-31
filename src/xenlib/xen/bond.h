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

#ifndef BOND_H
#define BOND_H

#include "xenobject.h"
#include <QMap>
#include <QStringList>

/**
 * @brief Bond - Network interface bonding
 *
 * Qt equivalent of C# XenAPI.Bond class. Represents a bonded network interface.
 * First published in XenServer 4.1.
 *
 * Key properties:
 * - uuid: Unique identifier
 * - master: Reference to the master PIF
 * - slaves: List of slave PIF references
 * - other_config: Additional configuration
 * - primary_slave: Reference to the primary slave PIF
 * - mode: Bonding mode (e.g., balance-slb, active-backup, lacp)
 * - properties: Bond properties
 * - links_up: Number of links currently up
 * - auto_update_mac: Whether MAC address auto-updates
 */
class XENLIB_EXPORT Bond : public XenObject
{
    Q_OBJECT

    public:
        explicit Bond(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~Bond() override = default;

        XenObjectType GetObjectType() const override { return XenObjectType::Bond; }

        // Property accessors (read from cache)
        QString MasterRef() const;
        QStringList SlaveRefs() const;
        QMap<QString, QString> OtherConfig() const;
        QString PrimarySlaveRef() const;
        QString Mode() const;
        QMap<QString, QString> Properties() const;
        qint64 LinksUp() const;
        bool AutoUpdateMac() const;
};

#endif // BOND_H
