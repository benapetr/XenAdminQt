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

#include "datakey.h"
#include "archivemaintainer.h"
#include "palette.h"
#include <QRegularExpression>
#include <QPainter>
#include <QSet>
#include <QFontMetrics>

namespace CustomDataGraph
{
    namespace
    {
        QString toFriendlyName(const QString& id)
        {
            const int idx = id.lastIndexOf(':');
            const QString metric = idx >= 0 ? id.mid(idx + 1) : id;

            static const QRegularExpression cpuRx(QStringLiteral("^cpu(\\d+)$"));
            static const QRegularExpression pifRx(QStringLiteral("^pif_(.+)_(rx|tx)$"));
            static const QRegularExpression vifRx(QStringLiteral("^vif_(.+)_(rx|tx)$"));
            static const QRegularExpression vbdRx(QStringLiteral("^vbd_(.+)_(read|write)$"));

            QRegularExpressionMatch m = cpuRx.match(metric);
            if (m.hasMatch())
                return QStringLiteral("CPU %1").arg(m.captured(1));

            if (metric == QStringLiteral("memory_free_kib") || metric == QStringLiteral("memory_internal_free"))
                return QStringLiteral("Used Memory");
            if (metric == QStringLiteral("memory_total_kib") || metric == QStringLiteral("memory"))
                return QStringLiteral("Total Memory");

            m = pifRx.match(metric);
            if (m.hasMatch())
                return QStringLiteral("%1 %2").arg(m.captured(1), m.captured(2) == QStringLiteral("rx") ? QStringLiteral("Receive") : QStringLiteral("Send"));

            m = vifRx.match(metric);
            if (m.hasMatch())
                return QStringLiteral("VIF %1 %2").arg(m.captured(1), m.captured(2) == QStringLiteral("rx") ? QStringLiteral("Receive") : QStringLiteral("Send"));

            m = vbdRx.match(metric);
            if (m.hasMatch())
                return QStringLiteral("VBD %1 %2").arg(m.captured(1), m.captured(2).compare(QStringLiteral("read"), Qt::CaseInsensitive) == 0 ? QStringLiteral("Read") : QStringLiteral("Write"));

            return metric;
        }
    }

    DataKey::DataKey(QWidget* parent) : QWidget(parent)
    {
    }

    void DataKey::SetArchiveMaintainer(ArchiveMaintainer* archiveMaintainer)
    {
        this->m_archiveMaintainer = archiveMaintainer;
    }

    void DataKey::SetDataSourceUUIDsToShow(const QList<QString>& dataSourceUuids)
    {
        this->m_dataSourceUUIDsToShow.clear();
        QSet<QString> seen;
        for (const QString& id : dataSourceUuids)
        {
            if (seen.contains(id))
                continue;
            seen.insert(id);
            this->m_dataSourceUUIDsToShow.append(id);
        }
    }

    void DataKey::SetFriendlyNames(const QMap<QString, QString>& friendlyNames)
    {
        this->m_friendlyNames = friendlyNames;
    }

    void DataKey::UpdateItems()
    {
        this->update();
    }

    void DataKey::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.fillRect(this->rect(), this->palette().base());
        painter.setRenderHint(QPainter::Antialiasing);

        const int left = 6;
        const int marker = 10;
        const int rowHeight = 20;
        int y = 6;
        const QFontMetrics fm = painter.fontMetrics();

        for (const QString& id : this->m_dataSourceUUIDsToShow)
        {
            painter.fillRect(QRect(left, y + 4, marker, marker), Palette::GetColour(id));
            painter.setPen(this->palette().text().color());

            const QString friendly = this->m_friendlyNames.value(id, toFriendlyName(id));
            const QString text = fm.elidedText(friendly, Qt::ElideRight, this->width() - (left + marker + 10));
            painter.drawText(left + marker + 6, y + rowHeight - 4, text);

            y += rowHeight;
            if (y + rowHeight > this->height())
                break;
        }
    }
}
