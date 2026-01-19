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

#ifndef XENAPI_VBD_H
#define XENAPI_VBD_H

#include <QString>
#include <QVariantMap>
#include <QVariantList>

namespace XenAPI
{
    class Session;

    /**
     * @brief Static XenAPI VBD (Virtual Block Device) bindings
     *
     * Qt port of C# XenAPI.VBD class. All methods are static and match
     * the C# signatures exactly for easy porting.
     */
    class VBD
    {
        private:
            VBD() = delete; // Static-only class

        public:
            // VBD lifecycle operations
            static QString create(Session* session, const QVariantMap& vbdRecord);

            /**
             * @brief Asynchronously create a VBD
             * @param session XenSession
             * @param vbdRecord VBD record
             * @return Task ref
             *
             * Matches C# VBD.async_create()
             */
            static QString async_create(Session* session, const QVariantMap& vbdRecord);
            static QString async_plug(Session* session, const QString& vbd);
            static QString async_unplug(Session* session, const QString& vbd);
            static QString async_destroy(Session* session, const QString& vbd);
            static QString async_eject(Session* session, const QString& vbd);
            static QString async_insert(Session* session, const QString& vbd, const QString& vdi);
            static void plug(Session* session, const QString& vbd);
            static void unplug(Session* session, const QString& vbd);
            static void destroy(Session* session, const QString& vbd);
            static void eject(Session* session, const QString& vbd);
            static void insert(Session* session, const QString& vbd, const QString& vdi);

            // VBD query operations
            static QVariantList get_allowed_operations(Session* session, const QString& vbd);
            static QString get_VM(Session* session, const QString& vbd);
            static QString get_VDI(Session* session, const QString& vbd);
            static QString get_device(Session* session, const QString& vbd);
            static QString get_userdevice(Session* session, const QString& vbd);
            static bool get_bootable(Session* session, const QString& vbd);
            static QString get_mode(Session* session, const QString& vbd);
            static QString get_type(Session* session, const QString& vbd);
            static bool get_unpluggable(Session* session, const QString& vbd);
            static bool get_currently_attached(Session* session, const QString& vbd);
            static bool get_empty(Session* session, const QString& vbd);
            static QVariantMap get_qos_algorithm_params(Session* session, const QString& vbd);

            // VBD modification operations
            static void set_bootable(Session* session, const QString& vbd, bool bootable);
            static void set_mode(Session* session, const QString& vbd, const QString& mode);
            static void set_userdevice(Session* session, const QString& vbd, const QString& userdevice);
            static void set_qos_algorithm_type(Session* session, const QString& vbd, const QString& algorithmType);
            static void set_qos_algorithm_params(Session* session, const QString& vbd, const QVariantMap& params);

            // Bulk query operations
            static QVariantMap get_record(Session* session, const QString& vbd);
            static QVariantList get_all(Session* session);
            static QVariantMap get_all_records(Session* session);
    };
} // namespace XenAPI

#endif // XENAPI_VBD_H
