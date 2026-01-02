#include "vtpm.h"
#include "network/connection.h"
#include "../xencache.h"

VTPM::VTPM(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString VTPM::GetObjectType() const
{
    return "vtpm";
}

QString VTPM::Uuid() const
{
    return this->stringProperty("uuid");
}

QStringList VTPM::AllowedOperations() const
{
    return this->property("allowed_operations").toStringList();
}

QVariantMap VTPM::CurrentOperations() const
{
    return this->property("current_operations").toMap();
}

QString VTPM::VMRef() const
{
    return this->stringProperty("VM");
}

QString VTPM::BackendRef() const
{
    return this->stringProperty("backend");
}

QString VTPM::PersistenceBackend() const
{
    return this->stringProperty("persistence_backend");
}

bool VTPM::IsUnique() const
{
    return this->boolProperty("is_unique");
}

bool VTPM::IsProtected() const
{
    return this->boolProperty("is_protected");
}

// Helper methods
bool VTPM::IsSecure() const
{
    return this->IsProtected() && this->IsUnique();
}
