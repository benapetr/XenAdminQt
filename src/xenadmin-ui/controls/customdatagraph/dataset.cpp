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

#include "dataset.h"
#include <algorithm>

namespace CustomDataGraph
{
    void DataSet::Clear()
    {
        this->m_points.clear();
    }

    void DataSet::AddPoint(const DataPoint& point)
    {
        this->m_points.append(point);
    }

    void DataSet::AddPoint(qint64 x, double y)
    {
        this->m_points.append(DataPoint(x, y));
    }

    void DataSet::SortDescendingByX()
    {
        std::sort(this->m_points.begin(), this->m_points.end(), [](const DataPoint& a, const DataPoint& b)
        {
            return a.X > b.X;
        });
    }

    bool DataSet::InsertPointSortedDescendingByX(const DataPoint& point)
    {
        if (this->m_points.isEmpty())
        {
            this->m_points.append(point);
            return true;
        }

        if (point.X > this->m_points.first().X)
        {
            this->m_points.prepend(point);
            return true;
        }

        if (point.X < this->m_points.last().X)
        {
            this->m_points.append(point);
            return true;
        }

        int left = 0;
        int right = this->m_points.size() - 1;
        while (left <= right)
        {
            const int mid = left + ((right - left) / 2);
            const qint64 midX = this->m_points.at(mid).X;

            if (midX == point.X)
                return false;

            if (midX < point.X)
                right = mid - 1;
            else
                left = mid + 1;
        }

        this->m_points.insert(left, point);
        return true;
    }

    void DataSet::TrimToMaxPoints(int maxPoints)
    {
        if (maxPoints <= 0 || this->m_points.size() <= maxPoints)
            return;

        this->m_points.resize(maxPoints);
    }

    const QVector<DataPoint>& DataSet::Points() const
    {
        return this->m_points;
    }

    DataRange DataSet::RangeY() const
    {
        if (this->m_points.isEmpty())
            return DataRange();

        double minVal = this->m_points.first().Y;
        double maxVal = this->m_points.first().Y;

        for (const DataPoint& point : this->m_points)
        {
            if (point.Y < minVal)
                minVal = point.Y;
            if (point.Y > maxVal)
                maxVal = point.Y;
        }

        return DataRange(minVal, maxVal);
    }
}
