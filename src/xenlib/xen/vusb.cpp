#include "vusb.h"
#include "network/connection.h"
#include "../xencache.h"

VUSB::VUSB(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString VUSB::GetObjectType() const
{
    return "vusb";
}

QString VUSB::Uuid() const
{
    return this->stringProperty("uuid");
}

QStringList VUSB::AllowedOperations() const
{
    return this->property("allowed_operations").toStringList();
}

QVariantMap VUSB::CurrentOperations() const
{
    return this->property("current_operations").toMap();
}

QString VUSB::VMRef() const
{
    return this->stringProperty("VM");
}

QString VUSB::USBGroupRef() const
{
    return this->stringProperty("USB_group");
}

QVariantMap VUSB::OtherConfig() const
{
    return this->property("other_config").toMap();
}

bool VUSB::CurrentlyAttached() const
{
    return this->boolProperty("currently_attached");
}

// Helper methods
bool VUSB::IsAttached() const
{
    return this->CurrentlyAttached();
}
