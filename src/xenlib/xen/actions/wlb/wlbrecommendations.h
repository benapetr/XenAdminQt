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

#ifndef WLBRECOMMENDATIONS_H
#define WLBRECOMMENDATIONS_H

#include "wlbrecommendation.h"
#include <QHash>
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
        WlbRecommendations(const QList<QSharedPointer<VM>>& vms, const QHash<QSharedPointer<VM>, QHash<QSharedPointer<Host>, QStringList>>& recommendations);

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
        QSharedPointer<Host> GetOptimalServer(const QSharedPointer<VM>& vm) const;

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
        WlbRecommendation GetStarRating(const QSharedPointer<Host>& host) const;

    private:
        /**
         * @brief Parse WLB star rating from string array
         * @param rec String array from XenAPI
         * @param starRating Output parameter for parsed star value
         * @return true if rec[0]=="WLB" (case insensitive) and rec[1] is valid double
         */
        static bool parseStarRating(const QStringList& rec, double& starRating);

        QList<QSharedPointer<VM>> m_vms;
        QHash<QSharedPointer<VM>, QHash<QSharedPointer<Host>, QStringList>> m_recommendations;
        bool m_isError = false;
};

#endif // WLBRECOMMENDATIONS_H
