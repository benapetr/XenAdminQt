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

#ifndef CUSTOMDATAGRAPH_DATAARCHIVE_H
#define CUSTOMDATAGRAPH_DATAARCHIVE_H

#include "dataset.h"
#include <QMap>
#include <QString>
#include <QList>

namespace CustomDataGraph
{
    class DataArchive
    {
        public:
            explicit DataArchive(int maxPoints = 0);

            void ClearSets();
            void Load(const QList<QPair<QString, DataSet>>& newSets);
            void Set(const QString& key, const DataSet& set);
            bool Contains(const QString& key) const;
            DataSet Get(const QString& key) const;
            const DataSet* Find(const QString& key) const;
            bool InsertPointSortedDescendingByX(const QString& key, const DataPoint& point);
            QList<QString> Keys() const;

            int MaxPoints() const;
            void SetMaxPoints(int maxPoints);

        private:
            QMap<QString, DataSet> m_sets;
            int m_maxPoints;

            DataSet Limit(const DataSet& input) const;
    };
}

#endif // CUSTOMDATAGRAPH_DATAARCHIVE_H
