#include "vmss.h"
#include "network/connection.h"
#include "../xencache.h"

VMSS::VMSS(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString VMSS::GetObjectType() const
{
    return "vmss";
}

QString VMSS::Uuid() const
{
    return this->stringProperty("uuid");
}

QString VMSS::NameLabel() const
{
    return this->stringProperty("name_label");
}

QString VMSS::NameDescription() const
{
    return this->stringProperty("name_description");
}

bool VMSS::Enabled() const
{
    return this->boolProperty("enabled");
}

QString VMSS::Type() const
{
    return this->stringProperty("type");
}

qlonglong VMSS::RetainedSnapshots() const
{
    return this->longProperty("retained_snapshots");
}

QString VMSS::Frequency() const
{
    return this->stringProperty("frequency");
}

QVariantMap VMSS::Schedule() const
{
    return this->property("schedule").toMap();
}

QDateTime VMSS::LastRunTime() const
{
    QVariant timeVariant = this->property("last_run_time");
    if (timeVariant.canConvert<QDateTime>())
        return timeVariant.toDateTime();
    
    // Try parsing as string if not already a QDateTime
    QString timeStr = timeVariant.toString();
    if (!timeStr.isEmpty())
    {
        QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
        if (dt.isValid())
            return dt;
    }
    
    // Return epoch time if parsing fails
    return QDateTime::fromSecsSinceEpoch(0);
}

QStringList VMSS::VMRefs() const
{
    return this->property("VMs").toStringList();
}

// Helper methods
bool VMSS::IsEnabled() const
{
    return this->Enabled();
}

int VMSS::VMCount() const
{
    return this->VMRefs().count();
}
