#include "clusterhost.h"
#include "../xencache.h"
#include "cluster.h"
#include "host.h"
#include "pif.h"

ClusterHost::ClusterHost(XenConnection* connection, const QString& opaqueRef, QObject* parent) : XenObject(connection, opaqueRef, parent)
{
}

QSharedPointer<Cluster> ClusterHost::GetCluster() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<Cluster>();
    
    XenCache* cache = connection->GetCache();
    
    QString clusterRef = this->GetClusterRef();
    if (clusterRef.isEmpty() || clusterRef == "OpaqueRef:NULL")
        return QSharedPointer<Cluster>();
    
    return cache->ResolveObject<Cluster>(clusterRef);
}

QSharedPointer<Host> ClusterHost::GetHost() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<Host>();
    
    XenCache* cache = connection->GetCache();
    
    QString hostRef = this->GetHostRef();
    if (hostRef.isEmpty() || hostRef == "OpaqueRef:NULL")
        return QSharedPointer<Host>();
    
    return cache->ResolveObject<Host>(XenObjectType::Host, hostRef);
}

QSharedPointer<PIF> ClusterHost::GetPIF() const
{
    XenConnection* connection = this->GetConnection();
    if (!connection)
        return QSharedPointer<PIF>();
    
    XenCache* cache = connection->GetCache();
    
    QString pifRef = this->GetPIFRef();
    if (pifRef.isEmpty() || pifRef == "OpaqueRef:NULL")
        return QSharedPointer<PIF>();
    
    return cache->ResolveObject<PIF>(pifRef);
}
