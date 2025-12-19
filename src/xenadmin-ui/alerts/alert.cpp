/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 */

#include "alert.h"
#include "../../xenlib/xen/connection.h"

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
