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

#ifndef BLOB_H
#define BLOB_H

#include "xenobject.h"
#include <QDateTime>

/**
 * @brief Blob - A placeholder for a binary blob
 *
 * Qt equivalent of C# XenAPI.Blob class. Represents a binary blob.
 * First published in XenServer 5.0.
 *
 * Key properties:
 * - uuid: Unique identifier
 * - name_label, name_description: Human-readable name and description
 * - size: Size of the blob in bytes
 * - pubblic: True if blob is publicly accessible
 * - last_updated: Timestamp of last modification
 * - mime_type: MIME type of the blob
 */
class XENLIB_EXPORT Blob : public XenObject
{
    Q_OBJECT

    public:
        explicit Blob(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        ~Blob() override = default;

        XenObjectType GetObjectType() const override { return XenObjectType::Blob; }

        // Property accessors (read from cache)
        qint64 Size() const;
        bool IsPublic() const;
        QDateTime LastUpdated() const;
        QString MimeType() const;
};

#endif // BLOB_H
