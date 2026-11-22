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

#ifndef XENAPI_VMSS_H
#define XENAPI_VMSS_H

#include <QString>
#include <QVariantMap>
#include "xenlib_global.h"

namespace XenAPI
{
    class Session;

    //! Static-only class providing XenServer VM snapshot schedule calls (C# XenAPI.VMSS)
    class XENLIB_EXPORT VMSS
    {
        public:
            VMSS() = delete;

            //! VMSS.create from a record (name_label, name_description, enabled, type,
            //! retained_snapshots, frequency, schedule); returns the new ref
            static QString create(Session* session, const QVariantMap& record);
            static void destroy(Session* session, const QString& vmss);
            //! VMSS.snapshot_now: runs the schedule once; returns the plugin's result
            static QString snapshot_now(Session* session, const QString& vmss);

            static void set_name_label(Session* session, const QString& vmss, const QString& value);
            static void set_name_description(Session* session, const QString& vmss, const QString& value);
            static void set_enabled(Session* session, const QString& vmss, bool value);
            //! "snapshot", "checkpoint" or "snapshot_with_quiesce"
            static void set_type(Session* session, const QString& vmss, const QString& value);
            static void set_retained_snapshots(Session* session, const QString& vmss, qint64 value);
            //! "hourly", "daily" or "weekly"
            static void set_frequency(Session* session, const QString& vmss, const QString& value);
            static void set_schedule(Session* session, const QString& vmss, const QVariantMap& value);
    };
}

#endif // XENAPI_VMSS_H
