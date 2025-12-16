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

#ifndef SRCREATEACTION_H
#define SRCREATEACTION_H

#include "../../asyncoperation.h"
#include <QVariantMap>

class Host;

/**
 * @brief SrCreateAction - Create a new Storage Repository
 *
 * Qt equivalent of C# SrCreateAction (XenModel/Actions/SR/SrCreateAction.cs).
 * Creates a new SR using SR.create(), handles secret creation for passwords,
 * verifies PBD attachment, and optionally sets as default SR if it's the first
 * shared non-ISO SR in the pool.
 *
 * From C# implementation:
 * - Creates secrets for passwords (CIFS, iSCSI, etc.) before SR.create
 * - Calls SR.create() with device config and SM config
 * - Verifies all PBDs are attached after creation
 * - If PBD plug fails, attempts manual plug and rolls back on failure
 * - Destroys password secrets after creation (PBDs duplicate them)
 * - Sets auto-scan other_config based on content type
 * - Sets as default SR if first shared non-ISO SR
 */
class XENLIB_EXPORT SrCreateAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Create a new Storage Repository
     * @param connection XenServer connection
     * @param host Host to create SR on (coordinator host for shared SRs)
     * @param srName SR name
     * @param srDescription SR description
     * @param srType SR type (e.g., "nfs", "lvmoiscsi", "ext", "iso")
     * @param srContentType Content type (e.g., "user", "iso")
     * @param deviceConfig Device configuration map (server, serverpath, target, etc.)
     * @param smConfig SM-specific configuration map (optional)
     * @param parent Parent object
     */
    explicit SrCreateAction(XenConnection* connection,
                            Host* host,
                            const QString& srName,
                            const QString& srDescription,
                            const QString& srType,
                            const QString& srContentType,
                            const QVariantMap& deviceConfig,
                            const QVariantMap& smConfig = QVariantMap(),
                            QObject* parent = nullptr);

protected:
    void run() override;

private:
    /**
     * @brief Create a secret for password fields
     * @param key The key name (e.g., "password", "cifspassword", "chappassword")
     * @param value The password value
     * @return Secret UUID
     *
     * Removes the key from device config and adds "<key>_secret" entry with UUID
     */
    QString createSecret(const QString& key, const QString& value);

    /**
     * @brief Attempt to forget an SR that failed to completely plug
     * @param srRef SR reference
     *
     * Unplugs all PBDs and forgets the SR. Never throws exceptions.
     */
    void forgetFailedSR(const QString& srRef);

    /**
     * @brief Check if this is the first shared non-ISO SR in the pool
     * @return true if no other shared non-ISO SRs exist
     *
     * Used to determine whether to set this SR as the default SR.
     * Queries the cache for all existing SRs.
     */
    bool isFirstSharedNonISOSR() const;

    Host* m_host;
    QString m_srName;
    QString m_srDescription;
    QString m_srType;
    QString m_srContentType;
    bool m_srIsShared;
    QVariantMap m_deviceConfig;
    QVariantMap m_smConfig;
    QString m_createdSecretUuid; // Track secret for cleanup
};

#endif // SRCREATEACTION_H
