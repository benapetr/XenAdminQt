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

#ifndef JSONRPCCLIENT_H
#define JSONRPCCLIENT_H

#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QByteArray>

namespace Xen
{
    /**
     * @brief JSON-RPC 2.0 client for XenServer API
     *
     * This class provides JSON-RPC encoding/decoding to match the C# XenAdmin implementation.
     * XenServer supports both XML-RPC and JSON-RPC, but C# uses JSON-RPC exclusively since
     * commit 8375754a5 (March 2023).
     *
     * Key differences from XML-RPC:
     * - Field names match XenAPI exactly: "class_", "opaqueRef" (not "class", "ref")
     * - Cleaner JSON format vs verbose XML
     * - Better type preservation (JSON native types vs XML string encoding)
     *
     * Reference: xenadmin/XenModel/XenAPI/JsonRpcClient.cs (~15,700 lines)
     */
    class JsonRpcClient
    {
    public:
        JsonRpcClient();
        ~JsonRpcClient();

        /**
         * @brief Build a JSON-RPC 2.0 request
         *
         * @param method The XenAPI method name (e.g., "session.login_with_password", "VM.start")
         * @param params The parameters array (already converted to QVariant)
         * @param requestId Unique request ID for tracking async responses
         * @return JSON-RPC request as UTF-8 encoded JSON
         *
         * Output format:
         * {
         *   "jsonrpc": "2.0",
         *   "method": "VM.start",
         *   "params": ["OpaqueRef:session-id", "OpaqueRef:vm-ref", false],
         *   "id": 1
         * }
         */
        static QByteArray buildJsonRpcCall(const QString& method, const QVariantList& params, int requestId);

        /**
         * @brief Parse a JSON-RPC 2.0 response
         *
         * @param json The raw JSON response from XenServer
         * @return Parsed response data, or null QVariant on error
         *
         * Expected format:
         * {
         *   "jsonrpc": "2.0",
         *   "result": {
         *     "Status": "Success",
         *     "Value": { ... actual data ... }
         *   },
         *   "id": 1
         * }
         *
         * Error format:
         * {
         *   "jsonrpc": "2.0",
         *   "error": {
         *     "code": -32600,
         *     "message": "Invalid Request"
         *   },
         *   "id": 1
         * }
         *
         * Returns the "Value" field on success, or null on error.
         * Sets lastError() on parse failure or error response.
         */
        static QVariant parseJsonRpcResponse(const QByteArray& json);

        /**
         * @brief Get the last error message
         * @return Error message from last parseJsonRpcResponse() failure
         */
        static QString lastError();

    private:
        static QString s_lastError;
    };
} // namespace Xen

#endif // JSONRPCCLIENT_H
