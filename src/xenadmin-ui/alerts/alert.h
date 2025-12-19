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

#ifndef ALERT_H
#define ALERT_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QUuid>

class XenConnection;

/**
 * Alert priority levels matching XenAPI message priority
 * C# Reference: XenModel/Alerts/Types/Alert.cs line 349 - AlertPriority enum
 */
enum class AlertPriority
{
    Unknown = 0,  // Lowest priority (default)
    Priority1 = 1, // Data-loss imminent
    Priority2 = 2, // Service-loss imminent
    Priority3 = 3, // Service degraded
    Priority4 = 4, // Service recovered
    Priority5 = 5  // Informational
};

/**
 * Base class for all alerts in the system.
 * C# Reference: XenModel/Alerts/Types/Alert.cs
 * 
 * Alerts represent notifications from XenServer about system events,
 * warnings, and errors. This is an abstract base class that must be
 * subclassed for specific alert types (MessageAlert, etc.)
 */
class Alert : public QObject
{
    Q_OBJECT

public:
    virtual ~Alert() = default;

    // Core alert properties
    QString uuid() const { return this->m_uuid; }
    virtual QString title() const = 0;
    virtual QString description() const = 0;
    virtual AlertPriority priority() const = 0;
    QDateTime timestamp() const { return this->m_timestamp; }
    virtual QString appliesTo() const = 0;
    
    // Additional properties
    virtual QString name() const { return QString(); }
    virtual QString webPageLabel() const { return QString(); }
    virtual QString fixLinkText() const { return QString(); }
    virtual QString helpLinkText() const { return tr("Click here for help"); }
    
    // State management
    virtual bool allowedToDismiss() const { return !this->m_dismissing; }
    virtual bool isDismissed() const { return false; }
    virtual void dismiss() = 0;
    
    bool dismissing() const { return this->m_dismissing; }
    void setDismissing(bool dismissing) { this->m_dismissing = dismissing; }
    
    // Connection tracking
    XenConnection* connection() const { return this->m_connection; }
    QString hostUuid() const { return this->m_hostUuid; }
    
    // Comparison functions for sorting
    static int compareOnDate(const Alert* a1, const Alert* a2);
    static int compareOnPriority(const Alert* a1, const Alert* a2);
    static int compareOnTitle(const Alert* a1, const Alert* a2);
    static int compareOnAppliesTo(const Alert* a1, const Alert* a2);
    static int compareOnDescription(const Alert* a1, const Alert* a2);
    
    // Helper to get priority as string
    static QString priorityToString(AlertPriority priority);

protected:
    explicit Alert(XenConnection* connection = nullptr);
    
    QString m_uuid;
    QDateTime m_timestamp;
    QString m_hostUuid;
    XenConnection* m_connection = nullptr;
    bool m_dismissing = false;
};

// Declare metatype for QVariant storage
Q_DECLARE_METATYPE(Alert*)

// qHash function for AlertPriority enum to allow use in QSet
inline uint qHash(AlertPriority priority, uint seed = 0)
{
    return ::qHash(static_cast<int>(priority), seed);
}

#endif // ALERT_H
