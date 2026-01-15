/*
 * Copyright (c) 2025, Petr Bena <petr@bena.rocks>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "wlbrecommendations.h"
#include "../../vm.h"
#include "../../host.h"
#include <QHash>
#include <QDebug>

// Message constants (TODO: Move to Messages class when implemented)
static const QString HOST_MENU_CURRENT_SERVER = "Current server";
static const QString HOST_NOT_LIVE_SHORT = "Host not live";

WlbRecommendations::WlbRecommendations(const QList<QSharedPointer<VM>>& vms, const QHash<QSharedPointer<VM>, QHash<QSharedPointer<Host>, QStringList>>& recommendations)
    : m_vms(vms), m_recommendations(recommendations)
{
    // Check if any recommendations contain errors
    for (auto vmIt = recommendations.constBegin(); vmIt != recommendations.constEnd(); ++vmIt)
    {
        const QHash<QSharedPointer<Host>, QStringList>& hostRecs = vmIt.value();
        for (auto hostIt = hostRecs.constBegin(); hostIt != hostRecs.constEnd(); ++hostIt)
        {
            const QStringList& rec = hostIt.value();
            if (rec.size() > 0 && rec[0].compare("WLB", Qt::CaseInsensitive) != 0)
            {
                this->m_isError = true;
                return;
            }
        }
    }
}

bool WlbRecommendations::IsError() const
{
    return this->m_isError;
}

QSharedPointer<Host> WlbRecommendations::GetOptimalServer(const QSharedPointer<VM>& vm) const
{
    if (!this->m_recommendations.contains(vm))
        return QSharedPointer<Host>();

    const QHash<QSharedPointer<Host>, QStringList>& hostRecs = this->m_recommendations[vm];
    
    QSharedPointer<Host> bestHost;
    double bestStars = -1.0;

    for (auto it = hostRecs.constBegin(); it != hostRecs.constEnd(); ++it)
    {
        QSharedPointer<Host> host = it.key();
        const QStringList& rec = it.value();

        double stars = 0.0;
        if (!parseStarRating(rec, stars))
            continue;

        // Exclude VM's current resident host
        if (vm->GetResidentOnHost() == host.data())
            continue;

        if (stars > bestStars)
        {
            bestStars = stars;
            bestHost = host;
        }
    }

    return bestHost;
}

WlbRecommendation WlbRecommendations::GetStarRating(const QSharedPointer<Host>& host) const
{
    WlbRecommendation result;
    
    double totalStars = 0.0;
    int count = 0;

    for (const QSharedPointer<VM>& vm : this->m_vms)
    {
        if (!this->m_recommendations.contains(vm))
            continue;

        const QHash<QSharedPointer<Host>, QStringList>& hostRecs = this->m_recommendations[vm];
        if (!hostRecs.contains(host))
            continue;

        const QStringList& rec = hostRecs[host];

        // Check if VM is already on this host
        if (vm->GetResidentOnHost() == host.data())
        {
            result.CanRunByVM[vm] = false;
            result.CantRunReasons[vm] = HOST_MENU_CURRENT_SERVER;
            continue;
        }

        // Try to parse as WLB recommendation
        double stars = 0.0;
        if (parseStarRating(rec, stars))
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

bool WlbRecommendations::parseStarRating(const QStringList& rec, double& starRating)
{
    if (rec.size() < 2)
        return false;

    if (rec[0].compare("WLB", Qt::CaseInsensitive) != 0)
        return false;

    bool ok = false;
    starRating = rec[1].toDouble(&ok);
    return ok;
}
