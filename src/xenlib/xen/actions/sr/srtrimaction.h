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

#ifndef SRTRIMACTION_H
#define SRTRIMACTION_H

#include "../../asyncoperation.h"
#include <QString>
#include <QVariantMap>
#include <QSharedPointer>

class SR;

/**
 * @brief SrTrimAction - Reclaim freed space from thin-provisioned storage
 *
 * Qt equivalent of C# SrTrimAction (XenModel/Actions/SR/SrTrimAction.cs).
 * Calls the "trim" plugin on the host to reclaim freed space from the SR.
 * This is useful for thin-provisioned storage where deleted VDI space needs
 * to be explicitly returned to the underlying storage.
 *
 * From C# implementation:
 * - Finds first attached storage host for the SR
 * - Calls Host.call_plugin with plugin="trim", function="do_trim"
 * - Parses XML response to check for errors
 * - Returns true if trim succeeded, false with error message if failed
 *
 * Note: Trim is only supported on certain SR types (e.g., thin-provisioned SRs)
 */
class XENLIB_EXPORT SrTrimAction : public AsyncOperation
{
    Q_OBJECT

    public:
        /**
         * @brief Trim (reclaim freed space) from an SR
         * @param connection XenServer connection
         * @param sr SR object to trim
         * @param parent Parent object
         */
        explicit SrTrimAction(XenConnection* connection,
                              QSharedPointer<SR> sr,
                              QObject* parent = nullptr);

    protected:
        void run() override;

    private:
        /**
         * @brief Parse trim error from XML response
         * @param xml XML response from trim plugin
         * @return Error message, or empty string if no error
         *
         * Parses XML like:
         * <trim_response>
         *   <key_value_pair><key>errcode</key><value>UnsupportedSRForTrim</value></key_value_pair>
         *   <key_value_pair><key>errmsg</key><value>Trim on [uuid] not supported</value></key_value_pair>
         * </trim_response>
         */
        QString getTrimError(const QString& xml);

        QSharedPointer<SR> m_sr;
};

#endif // SRTRIMACTION_H
