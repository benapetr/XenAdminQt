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

#ifndef XENAPI_TASK_H
#define XENAPI_TASK_H

#include <QString>

namespace XenAPI
{
    class Session;
    
    /// <summary>
    /// Static methods for XenAPI Task operations (XAPI object: task)
    /// A long-running asynchronous task
    /// </summary>
    class Task
    {
    private:
        Task() {} // Private constructor - this is a static-only class
        
    public:
        /// <summary>
        /// Create a new task object
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_label">short label for the new task</param>
        /// <param name="_description">longer description for the new task</param>
        /// <returns>Reference to the created task</returns>
        static QString Create(Session* session, const QString& label, const QString& description);
        
        /// <summary>
        /// Destroy the task object
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_task">The opaque_ref of the given task</param>
        static void Destroy(Session* session, const QString& taskRef);
        
        /// <summary>
        /// Request that a task be cancelled
        /// </summary>
        /// <param name="session">The session</param>
        /// <param name="_task">The opaque_ref of the given task</param>
        static void Cancel(Session* session, const QString& taskRef);
    };
}

#endif // XENAPI_TASK_H
