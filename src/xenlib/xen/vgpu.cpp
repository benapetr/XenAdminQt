#include "vgpu.h"
#include "network/connection.h"
#include "../xencache.h"

VGPU::VGPU(XenConnection* connection, const QString& opaqueRef, QObject* parent)
    : XenObject(connection, opaqueRef, parent)
{
}

QString VGPU::GetObjectType() const
{
    return "vgpu";
}

QString VGPU::Uuid() const
{
    return this->stringProperty("uuid");
}

QString VGPU::VMRef() const
{
    return this->stringProperty("VM");
}

QString VGPU::GPUGroupRef() const
{
    return this->stringProperty("GPU_group");
}

QString VGPU::Device() const
{
    return this->stringProperty("device");
}

bool VGPU::CurrentlyAttached() const
{
    return this->boolProperty("currently_attached");
}

QVariantMap VGPU::OtherConfig() const
{
    return this->property("other_config").toMap();
}

// GPU configuration
QString VGPU::TypeRef() const
{
    return this->stringProperty("type");
}

QString VGPU::ResidentOnRef() const
{
    return this->stringProperty("resident_on");
}

QString VGPU::ScheduledToBeResidentOnRef() const
{
    return this->stringProperty("scheduled_to_be_resident_on");
}

QVariantMap VGPU::CompatibilityMetadata() const
{
    return this->property("compatibility_metadata").toMap();
}

QString VGPU::ExtraArgs() const
{
    return this->stringProperty("extra_args");
}

QString VGPU::PCIRef() const
{
    return this->stringProperty("PCI");
}

// Helper methods
bool VGPU::IsAttached() const
{
    return this->CurrentlyAttached();
}

bool VGPU::IsResident() const
{
    QString resident = this->ResidentOnRef();
    return !resident.isEmpty() && resident != "OpaqueRef:NULL";
}

bool VGPU::HasScheduledLocation() const
{
    QString scheduled = this->ScheduledToBeResidentOnRef();
    return !scheduled.isEmpty() && scheduled != "OpaqueRef:NULL";
}
