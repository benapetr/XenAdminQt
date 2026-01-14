#ifndef WLBRECOMMENDATION_H
#define WLBRECOMMENDATION_H

#include <QMap>
#include <QString>

class VM;

/**
 * @brief WLB recommendation for a specific host
 * 
 * Contains information about whether VMs can run on a host,
 * the star rating (0.0-5.0), and reasons why VMs cannot run.
 */
class WlbRecommendation
{
public:
    /**
     * @brief Construct an empty recommendation
     */
    WlbRecommendation();

    /**
     * @brief Map of VM -> whether it can run on this host
     */
    QMap<VM*, bool> CanRunByVM;

    /**
     * @brief Average star rating across all VMs (0.0-5.0)
     */
    double StarRating;

    /**
     * @brief Map of VM -> reason why it cannot run (if CanRunByVM[vm] == false)
     */
    QMap<VM*, QString> CantRunReasons;
};

#endif // WLBRECOMMENDATION_H
