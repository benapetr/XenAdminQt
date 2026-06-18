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

#ifndef XENAPI_VTPM_H
#define XENAPI_VTPM_H

#include "../../xenlib_global.h"
#include <QString>

namespace XenAPI
{
    class Session;

    /**
     * @brief Static XenAPI bindings for the VTPM class.
     *
     * C# equivalent: XenModel/XenAPI/VTPM.cs
     *
     * vTPM support requires XenServer 22.26.0+ (XCP-ng 8.3+). Callers should
     * catch std::runtime_error and treat failure as a non-fatal warning on
     * older hosts.
     */
    class XENLIB_EXPORT VTPM
    {
        public:
            VTPM()  = delete;
            ~VTPM() = delete;

            /**
             * @brief Create a virtual TPM and attach it to a VM.
             *
             * C# equivalent: VTPM.create(session_ref, vm_ref, is_unique)
             *
             * @param session   Active XenServer session.
             * @param vmRef     OpaqueRef of the VM to attach to.
             * @param isUnique  When true, only one vTPM is allowed per VM
             *                  (XenServer enforces uniqueness server-side).
             * @return OpaqueRef of the newly created VTPM record.
             * @throws std::runtime_error on API failure or not-connected.
             */
            static QString create(Session* session, const QString& vmRef, bool isUnique = false);

            /**
             * @brief Destroy a virtual TPM record.
             *
             * C# equivalent: VTPM.destroy(session_ref, vtpm_ref)
             *
             * @param session  Active XenServer session.
             * @param vtpmRef  OpaqueRef of the VTPM to destroy.
             * @throws std::runtime_error on API failure or not-connected.
             */
            static void destroy(Session* session, const QString& vtpmRef);
    };

} // namespace XenAPI

#endif // XENAPI_VTPM_H
