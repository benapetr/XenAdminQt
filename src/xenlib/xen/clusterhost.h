#ifndef CLUSTERHOST_H
#define CLUSTERHOST_H

#include "xenobject.h"
#include <QString>
#include <QVariantMap>
#include <QSharedPointer>

// Forward declarations
class Cluster;
class Host;
class PIF;

/**
 * @brief Cluster member metadata
 * First published in XenServer 7.6.
 */
class ClusterHost : public XenObject
{
    public:
        ClusterHost(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
        virtual ~ClusterHost() = default;

        // Property getters (read from cache dynamically)
        QString GetUuid() const { return this->stringProperty("uuid"); }
        QString GetClusterRef() const { return this->stringProperty("cluster"); }
        QString GetHostRef() const { return this->stringProperty("host"); }
        bool GetEnabled() const { return this->boolProperty("enabled"); }
        QString GetPIFRef() const { return this->stringProperty("PIF"); }
        bool GetJoined() const { return this->boolProperty("joined"); }
        QStringList GetAllowedOperations() const { return this->stringListProperty("allowed_operations"); }
        QVariantMap GetCurrentOperations() const { return this->property("current_operations").toMap(); }
        QVariantMap GetOtherConfig() const { return this->property("other_config").toMap(); }

        // Object resolution methods
        QSharedPointer<Cluster> GetCluster() const;
        QSharedPointer<Host> GetHost() const;
        QSharedPointer<PIF> GetPIF() const;
};

#endif // CLUSTERHOST_H
