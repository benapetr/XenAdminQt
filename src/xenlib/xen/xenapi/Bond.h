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

#ifndef XENAPI_BOND_H
#define XENAPI_BOND_H

#include <QString>
#include <QVariantList>
#include <QVariantMap>

class XenSession;

namespace XenAPI
{

    /**
     * @brief Bond XenAPI bindings
     *
     * Static-only class providing XenServer Bond API calls.
     * Matches C# XenModel/XenAPI/Bond.cs structure.
     */
    class Bond
    {
    private:
        Bond() = delete; // Static-only class

    public:
        // Bond creation and destruction
        static QString async_create(XenSession* session, const QString& network,
                                    const QStringList& members, const QString& mac,
                                    const QString& mode, const QVariantMap& properties);
        static QString async_destroy(XenSession* session, const QString& bond);

        // Bond configuration
        static void set_mode(XenSession* session, const QString& bond, const QString& mode);
        static void set_property(XenSession* session, const QString& bond, const QString& name, const QString& value);

        // Bond queries
        static QVariantMap get_record(XenSession* session, const QString& bond);
        static QString get_master(XenSession* session, const QString& bond);
        static QVariantList get_slaves(XenSession* session, const QString& bond);
    };

} // namespace XenAPI

#endif // XENAPI_BOND_H
