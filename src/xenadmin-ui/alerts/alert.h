/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
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
