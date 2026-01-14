#ifndef WLBRECOMMENDATIONS_H
#define WLBRECOMMENDATIONS_H

#include "wlbrecommendation.h"
#include <QMap>
#include <QList>
#include <QStringList>

class VM;
class Host;

/**
 * @brief Wrapper for WLB recommendations with analysis methods
 * 
 * Analyzes WLB recommendation data from the XenAPI to provide:
 * - Optimal server selection (highest star rating)
 * - Per-host star ratings and eligibility
 * - Error handling for common failure scenarios
 */
class WlbRecommendations
{
public:
    /**
     * @brief Construct WLB recommendations wrapper
     * @param vms List of VMs being analyzed
     * @param recommendations Map of VM -> (Map of Host -> string array)
     * 
     * String array format:
     * - ["WLB", "star_rating"] - Success with star rating
     * - ["WLB", "0.0", "reason"] - Zero rating with reason
     * - [error_code, detail, detail] - XenAPI error
     */
    WlbRecommendations(const QList<VM*>& vms,
                       const QMap<VM*, QMap<Host*, QStringList>>& recommendations);

    /**
     * @brief Check if the WLB API call failed
     * @return true if any recommendations contain errors
     */
    bool IsError() const;

    /**
     * @brief Get the optimal (highest-rated) server for a VM
     * @param vm The VM to find optimal placement for
     * @return Host with highest star rating (excluding VM's current resident host), or nullptr
     */
    Host* GetOptimalServer(VM* vm) const;

    /**
     * @brief Get star rating and eligibility for a specific host
     * @param host The host to analyze
     * @return WlbRecommendation containing average stars, CanRunByVM, and CantRunReasons
     * 
     * Star rating is the average across all VMs.
     * Three error scenarios are handled:
     * 1. VM already on host: "Current server", CanRun=false
     * 2. XenAPI failure: Parsed error message, CanRun=false
     * 3. Host not live: "Host not live", CanRun=false
     */
    WlbRecommendation GetStarRating(Host* host) const;

private:
    /**
     * @brief Parse WLB star rating from string array
     * @param rec String array from XenAPI
     * @param starRating Output parameter for parsed star value
     * @return true if rec[0]=="WLB" (case insensitive) and rec[1] is valid double
     */
    static bool ParseStarRating(const QStringList& rec, double& starRating);

    QList<VM*> vms_;
    QMap<VM*, QMap<Host*, QStringList>> recommendations_;
    bool isError_;
};

#endif // WLBRECOMMENDATIONS_H
