#include "wlbretrievevmrecommendationsaction.h"
#include "../../network/connection.h"
#include "../../vm.h"
#include "../../host.h"
#include "../../pool.h"
#include "../../session.h"
#include "../../xenapi/xenapi_VM.h"
#include "../../../xencache.h"
#include <QDebug>

WlbRetrieveVmRecommendationsAction::WlbRetrieveVmRecommendationsAction(
    XenConnection* connection,
    const QList<VM*>& vms,
    QObject* parent)
    : AsyncOperation(connection, "Retrieving WLB VM recommendations", "", parent)
    , vms_(vms)
{
    // Add required API role
    AddApiMethodToRoleCheck("vm.retrieve_wlb_recommendations");
}

QMap<VM*, QMap<Host*, QStringList>> WlbRetrieveVmRecommendationsAction::GetRecommendations() const
{
    return this->recommendations_;
}

void WlbRetrieveVmRecommendationsAction::run()
{
    XenAPI::Session* session = GetSession();
    if (!session || !session->IsLoggedIn())
    {
        setError("Not connected to XenServer");
        return;
    }

    // Check if WLB is enabled on the connection
    // TODO: Implement Helpers::WlbEnabled() check
    // For now we'll attempt the call and handle errors
    bool wlbEnabled = false;
    
    // Get pool to check WLB configuration
    QSharedPointer<Pool> pool = GetConnection()->GetCache()->GetPool();
    if (pool && pool->IsValid())
    {
        QVariantMap poolData = pool->GetData();
        QString wlbUrl = poolData.value("wlb_url").toString();
        wlbEnabled = poolData.value("wlb_enabled").toBool() && !wlbUrl.isEmpty();
    }

    if (!wlbEnabled)
    {
        qDebug() << "WLB is not enabled on this connection";
        return;
    }

    try
    {
        foreach (VM* vm, this->vms_)
        {
            if (!vm || !vm->IsValid())
                continue;

            QString vmRef = vm->OpaqueRef();
            if (vmRef.isEmpty())
                continue;

            SetDescription(QString("Retrieving WLB recommendations for VM '%1'").arg(vm->GetName()));

            // Call XenAPI to retrieve recommendations
            QMap<QString, QStringList> hostRecommendations = 
                XenAPI::VM::retrieve_wlb_recommendations(session, vmRef);

            // Convert host refs to Host* objects
            QMap<Host*, QStringList> recommendations;
            QMapIterator<QString, QStringList> it(hostRecommendations);
            while (it.hasNext())
            {
                it.next();
                QSharedPointer<Host> host = GetConnection()->GetCache()->ResolveObject<Host>("host", it.key());
                if (host && host->IsValid())
                {
                    recommendations[host.data()] = it.value();
                }
            }

            this->recommendations_[vm] = recommendations;
        }
    }
    catch (const std::exception& e)
    {
        setError(QString("Failed to retrieve WLB recommendations: %1").arg(e.what()));
    }
}
