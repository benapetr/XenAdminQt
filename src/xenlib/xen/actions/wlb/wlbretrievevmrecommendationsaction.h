#ifndef WLBRETRIEVEVMRECOMMENDATIONSACTION_H
#define WLBRETRIEVEVMRECOMMENDATIONSACTION_H

#include "../../asyncoperation.h"
#include <QMap>
#include <QStringList>

class VM;
class Host;
class XenConnection;

/**
 * @brief Action to retrieve WLB (Workload Balancing) recommendations for VMs
 * 
 * This action calls the XenAPI VM.retrieve_wlb_recommendations method for each VM
 * to get placement recommendations from the WLB server. The recommendations include
 * star ratings (0.0-5.0) and reasons why VMs can or cannot run on specific hosts.
 */
class WlbRetrieveVmRecommendationsAction : public AsyncOperation
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new WLB retrieve recommendations action
     * @param connection The XenServer connection
     * @param vms List of VMs to retrieve recommendations for
     * @param parent Optional parent QObject
     */
    explicit WlbRetrieveVmRecommendationsAction(XenConnection* connection, 
                                                 const QList<VM*>& vms,
                                                 QObject* parent = nullptr);

    /**
 * @brief Get the WLB recommendations
 * @return Map of VM -> (Map of Host -> recommendation string array)
 * 
 * String array format: ["WLB", "star_rating"] or ["WLB", "0.0", "reason"] or [error_code, detail, detail]
 */
QMap<VM*, QMap<Host*, QStringList>> GetRecommendations() const;

protected:
    /**
     * @brief Execute the action - retrieve WLB recommendations for each VM
     */
    void run() override;

private:
    QList<VM*> vms_;
    QMap<VM*, QMap<Host*, QStringList>> recommendations_;
};

#endif // WLBRETRIEVEVMRECOMMENDATIONSACTION_H
