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

#ifndef XENAPI_GPU_GROUP_H
#define XENAPI_GPU_GROUP_H

#include "../../xenlib_global.h"
#include <QString>

namespace XenAPI
{
    class Session;

    enum class AllocationAlgorithm
    {
        Unknown,
        BreadthFirst,
        DepthFirst
    };

    XENLIB_EXPORT QString AllocationAlgorithmToWireValue(AllocationAlgorithm algorithm);
    XENLIB_EXPORT AllocationAlgorithm AllocationAlgorithmFromWireValue(const QString& value);

    class XENLIB_EXPORT GPU_group
    {
        private:
            GPU_group() = delete;

        public:
            static void set_allocation_algorithm(Session* session, const QString& gpuGroupRef, AllocationAlgorithm algorithm);
            static QString async_set_allocation_algorithm(Session* session, const QString& gpuGroupRef, AllocationAlgorithm algorithm);
    };
} // namespace XenAPI

#endif // XENAPI_GPU_GROUP_H
