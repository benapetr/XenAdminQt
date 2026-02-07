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

#ifndef CUSTOMDATAGRAPH_DATASOURCEITEM_H
#define CUSTOMDATAGRAPH_DATASOURCEITEM_H

#include <QString>
#include <QColor>

namespace CustomDataGraph
{
    struct DataSourceDescriptor
    {
        QString NameLabel;
        bool Standard = false;
        bool Enabled = true;
        QString Units;
    };

    class DataSourceItem
    {
        public:
            DataSourceItem() = default;

            DataSourceItem(const DataSourceDescriptor& source, const QString& friendlyName,
                           const QColor& color, const QString& id)
                : DataSource(source), FriendlyName(friendlyName), Color(color), Id(id), Enabled(source.Enabled)
            {
            }

            QString GetDataSource() const;

            DataSourceDescriptor DataSource;
            QString FriendlyName;
            QColor Color;
            QString Id;
            bool Enabled = true;
            bool Hidden = false;
            bool ColorChanged = false;

            bool operator==(const DataSourceItem& other) const
            {
                return this->Id == other.Id;
            }
    };
}

#endif // CUSTOMDATAGRAPH_DATASOURCEITEM_H
