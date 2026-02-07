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

#include "dataarchive.h"

namespace CustomDataGraph
{
    DataArchive::DataArchive(int maxPoints) : m_maxPoints(maxPoints)
    {
    }

    void DataArchive::ClearSets()
    {
        this->m_sets.clear();
    }

    void DataArchive::Load(const QList<QPair<QString, DataSet>>& newSets)
    {
        for (const QPair<QString, DataSet>& pair : newSets)
        {
            this->m_sets.insert(pair.first, this->Limit(pair.second));
        }
    }

    void DataArchive::Set(const QString& key, const DataSet& set)
    {
        this->m_sets.insert(key, this->Limit(set));
    }

    bool DataArchive::Contains(const QString& key) const
    {
        return this->m_sets.contains(key);
    }

    DataSet DataArchive::Get(const QString& key) const
    {
        return this->m_sets.value(key);
    }

    const DataSet* DataArchive::Find(const QString& key) const
    {
        const auto it = this->m_sets.constFind(key);
        if (it == this->m_sets.constEnd())
            return nullptr;

        return &it.value();
    }

    bool DataArchive::InsertPointSortedDescendingByX(const QString& key, const DataPoint& point)
    {
        DataSet& set = this->m_sets[key];
        if (!set.InsertPointSortedDescendingByX(point))
            return false;

        set.TrimToMaxPoints(this->m_maxPoints);
        return true;
    }

    QList<QString> DataArchive::Keys() const
    {
        return this->m_sets.keys();
    }

    int DataArchive::MaxPoints() const
    {
        return this->m_maxPoints;
    }

    void DataArchive::SetMaxPoints(int maxPoints)
    {
        this->m_maxPoints = maxPoints;
    }

    DataSet DataArchive::Limit(const DataSet& input) const
    {
        if (this->m_maxPoints <= 0)
            return input;

        DataSet output;
        const QVector<DataPoint>& points = input.Points();
        const int limit = qMin(points.size(), this->m_maxPoints);

        for (int i = 0; i < limit; ++i)
        {
            output.AddPoint(points.at(i));
        }

        return output;
    }
}
