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

#include "alert.h"
#include "../../xenlib/xen/network/connection.h"

Alert::Alert(XenConnection* connection)
    : QObject(nullptr)
    , m_connection(connection)
    , m_dismissing(false)
{
    this->m_uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    this->m_timestamp = QDateTime::currentDateTime();
}

// C# Reference: Alert.cs line 272 - CompareOnDate
int Alert::compareOnDate(const Alert* a1, const Alert* a2)
{
    if (!a1 || !a2)
        return 0;
    
    int result = a1->timestamp() < a2->timestamp() ? -1 : 
                 a1->timestamp() > a2->timestamp() ? 1 : 0;
    
    if (result == 0)
        result = QString::compare(a1->name(), a2->name(), Qt::CaseInsensitive);
    if (result == 0)
        result = QString::compare(a1->uuid(), a2->uuid());
    
    return result;
}

// C# Reference: Alert.cs line 283 - CompareOnPriority
int Alert::compareOnPriority(const Alert* a1, const Alert* a2)
{
    if (!a1 || !a2)
        return 0;
    
    int p1 = static_cast<int>(a1->priority());
    int p2 = static_cast<int>(a2->priority());
    
    // Unknown (0) is the lowest priority
    if (p1 < p2)
        return p1 == 0 ? 1 : -1;
    if (p1 > p2)
        return p2 == 0 ? -1 : 1;
    
    return QString::compare(a1->uuid(), a2->uuid());
}

// C# Reference: Alert.cs line 295 - CompareOnTitle
int Alert::compareOnTitle(const Alert* a1, const Alert* a2)
{
    if (!a1 || !a2)
        return 0;
    
    int result = QString::compare(a1->title(), a2->title(), Qt::CaseInsensitive);
    if (result == 0)
        result = QString::compare(a1->name(), a2->name(), Qt::CaseInsensitive);
    if (result == 0)
        result = QString::compare(a1->uuid(), a2->uuid());
    
    return result;
}

// C# Reference: Alert.cs line 305 - CompareOnAppliesTo
int Alert::compareOnAppliesTo(const Alert* a1, const Alert* a2)
{
    if (!a1 || !a2)
        return 0;
    
    int result = QString::compare(a1->appliesTo(), a2->appliesTo(), Qt::CaseInsensitive);
    if (result == 0)
        result = QString::compare(a1->name(), a2->name(), Qt::CaseInsensitive);
    if (result == 0)
        result = QString::compare(a1->uuid(), a2->uuid());
    
    return result;
}

// C# Reference: Alert.cs line 316 - CompareOnDescription
int Alert::compareOnDescription(const Alert* a1, const Alert* a2)
{
    if (!a1 || !a2)
        return 0;
    
    int result = QString::compare(a1->description(), a2->description(), Qt::CaseInsensitive);
    if (result == 0)
        result = QString::compare(a1->name(), a2->name(), Qt::CaseInsensitive);
    if (result == 0)
        result = QString::compare(a1->uuid(), a2->uuid());
    
    return result;
}

// C# Reference: Alert.cs line 369 - GetString extension
QString Alert::priorityToString(AlertPriority priority)
{
    switch (priority)
    {
        case AlertPriority::Priority1:
            return "1";
        case AlertPriority::Priority2:
            return "2";
        case AlertPriority::Priority3:
            return "3";
        case AlertPriority::Priority4:
            return "4";
        case AlertPriority::Priority5:
            return "5";
        default:
            return QObject::tr("Unknown");
    }
}
