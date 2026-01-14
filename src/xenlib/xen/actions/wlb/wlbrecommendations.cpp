#include "wlbrecommendations.h"
#include "../../vm.h"
#include "../../host.h"
#include <QDebug>

// Message constants (TODO: Move to Messages class when implemented)
static const QString HOST_MENU_CURRENT_SERVER = "Current server";
static const QString HOST_NOT_LIVE_SHORT = "Host not live";

WlbRecommendations::WlbRecommendations(
    const QList<VM*>& vms,
    const QMap<VM*, QMap<Host*, QStringList>>& recommendations)
    : vms_(vms)
    , recommendations_(recommendations)
    , isError_(false)
{
    // Check if any recommendations contain errors
    QMapIterator<VM*, QMap<Host*, QStringList>> vmIt(recommendations);
    while (vmIt.hasNext())
    {
        vmIt.next();
        QMapIterator<Host*, QStringList> hostIt(vmIt.value());
        while (hostIt.hasNext())
        {
            hostIt.next();
            const QStringList& rec = hostIt.value();
            if (rec.size() > 0 && rec[0].compare("WLB", Qt::CaseInsensitive) != 0)
            {
                this->isError_ = true;
                return;
            }
        }
    }
}

bool WlbRecommendations::IsError() const
{
    return this->isError_;
}

Host* WlbRecommendations::GetOptimalServer(VM* vm) const
{
    if (!this->recommendations_.contains(vm))
        return nullptr;

    const QMap<Host*, QStringList>& hostRecs = this->recommendations_[vm];
    
    Host* bestHost = nullptr;
    double bestStars = -1.0;

    QMapIterator<Host*, QStringList> it(hostRecs);
    while (it.hasNext())
    {
        it.next();
        Host* host = it.key();
        const QStringList& rec = it.value();

        double stars = 0.0;
        if (!ParseStarRating(rec, stars))
            continue;

        // Exclude VM's current resident host
        if (vm->GetResidentOnHost() == host)
            continue;

        if (stars > bestStars)
        {
            bestStars = stars;
            bestHost = host;
        }
    }

    return bestHost;
}

WlbRecommendation WlbRecommendations::GetStarRating(Host* host) const
{
    WlbRecommendation result;
    
    double totalStars = 0.0;
    int count = 0;

    foreach (VM* vm, this->vms_)
    {
        if (!this->recommendations_.contains(vm))
            continue;

        const QMap<Host*, QStringList>& hostRecs = this->recommendations_[vm];
        if (!hostRecs.contains(host))
            continue;

        const QStringList& rec = hostRecs[host];

        // Check if VM is already on this host
        if (vm->GetResidentOnHost() == host)
        {
            result.CanRunByVM[vm] = false;
            result.CantRunReasons[vm] = HOST_MENU_CURRENT_SERVER;
            continue;
        }

        // Try to parse as WLB recommendation
        double stars = 0.0;
        if (ParseStarRating(rec, stars))
        {
            result.CanRunByVM[vm] = (stars > 0.0);
            totalStars += stars;
            count++;
            
            // If zero stars and reason provided
            if (stars == 0.0 && rec.size() > 2)
            {
                result.CantRunReasons[vm] = rec[2];
            }
        }
        else
        {
            // XenAPI error or host not live
            result.CanRunByVM[vm] = false;
            
            if (rec.size() > 0)
            {
                // TODO: Parse using Failure class when implemented
                // For now, check for common "HOST_NOT_LIVE" error
                if (rec[0].contains("HOST_NOT_LIVE", Qt::CaseInsensitive))
                {
                    result.CantRunReasons[vm] = HOST_NOT_LIVE_SHORT;
                }
                else
                {
                    // Generic error message from rec[0]
                    result.CantRunReasons[vm] = rec[0];
                }
            }
            else
            {
                result.CantRunReasons[vm] = "Unknown error";
            }
        }
    }

    // Calculate average star rating
    if (count > 0)
    {
        result.StarRating = totalStars / count;
    }
    else
    {
        result.StarRating = 0.0;
    }

    return result;
}

bool WlbRecommendations::ParseStarRating(const QStringList& rec, double& starRating)
{
    if (rec.size() < 2)
        return false;

    if (rec[0].compare("WLB", Qt::CaseInsensitive) != 0)
        return false;

    bool ok = false;
    starRating = rec[1].toDouble(&ok);
    return ok;
}
