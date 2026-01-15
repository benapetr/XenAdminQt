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

#ifndef WLBRECOMMENDATION_H
#define WLBRECOMMENDATION_H

#include <QHash>
#include <QSharedPointer>
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
        QHash<QSharedPointer<VM>, bool> CanRunByVM;

        /**
         * @brief Average star rating across all VMs (0.0-5.0)
         */
        double StarRating;

        /**
         * @brief Map of VM -> reason why it cannot run (if CanRunByVM[vm] == false)
         */
        QHash<QSharedPointer<VM>, QString> CantRunReasons;
};

#endif // WLBRECOMMENDATION_H
