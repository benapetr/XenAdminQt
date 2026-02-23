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

#include "tableclipboardutils.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <utility>

QString TableClipboardUtils::BuildCsvFromTable(const QTableWidget* table, bool skipEmptyDecorativeColumns)
{
    if (!table)
        return QString();

    QList<int> exportColumns;
    exportColumns.reserve(table->columnCount());
    for (int col = 0; col < table->columnCount(); ++col)
    {
        const QTableWidgetItem* headerItem = table->horizontalHeaderItem(col);
        const QString headerText = headerItem ? headerItem->text().trimmed() : QString();

        bool hasData = false;
        for (int row = 0; row < table->rowCount(); ++row)
        {
            const QTableWidgetItem* item = table->item(row, col);
            if (item && !item->text().trimmed().isEmpty())
            {
                hasData = true;
                break;
            }
        }

        if (!skipEmptyDecorativeColumns || !headerText.isEmpty() || hasData)
            exportColumns.append(col);
    }

    QStringList headers;
    headers.reserve(exportColumns.size());
    for (int col : std::as_const(exportColumns))
    {
        const QTableWidgetItem* headerItem = table->horizontalHeaderItem(col);
        headers.append(headerItem ? headerItem->text() : QString());
    }

    QList<QStringList> rows;
    rows.reserve(table->rowCount());
    for (int row = 0; row < table->rowCount(); ++row)
    {
        QStringList rowValues;
        rowValues.reserve(exportColumns.size());
        for (int col : std::as_const(exportColumns))
        {
            const QTableWidgetItem* item = table->item(row, col);
            rowValues.append(item ? item->text() : QString());
        }
        rows.append(rowValues);
    }

    return BuildCsvDocument(headers, rows);
}

void TableClipboardUtils::CopyTableCsvToClipboard(const QTableWidget* table, bool skipEmptyDecorativeColumns)
{
    const QString csvText = BuildCsvFromTable(table, skipEmptyDecorativeColumns);
    if (QClipboard* clipboard = QGuiApplication::clipboard())
        clipboard->setText(csvText);
}

QString TableClipboardUtils::CsvEscapeField(const QString& value)
{
    QString escaped = value;
    escaped.replace("\r\n", "\n");
    escaped.replace('\r', '\n');
    escaped.replace('"', "\"\"");

    const bool needsQuotes = escaped.contains(',') || escaped.contains('"') || escaped.contains('\n');
    return needsQuotes ? QString("\"%1\"").arg(escaped) : escaped;
}

QString TableClipboardUtils::CsvJoinRow(const QStringList& fields)
{
    QStringList escaped;
    escaped.reserve(fields.size());
    for (const QString& field : fields)
        escaped.append(CsvEscapeField(field));
    return escaped.join(',');
}

QString TableClipboardUtils::BuildCsvDocument(const QStringList& headers, const QList<QStringList>& rows)
{
    QStringList lines;
    lines.reserve(rows.size() + (headers.isEmpty() ? 0 : 1));

    if (!headers.isEmpty())
        lines.append(CsvJoinRow(headers));

    for (const QStringList& row : rows)
        lines.append(CsvJoinRow(row));

    return lines.join('\n');
}

TableClipboardUtils::SortState TableClipboardUtils::CaptureSortState(const QTableWidget* table)
{
    SortState state;
    state.order = Qt::AscendingOrder;

    if (!table)
        return state;

    state.wasSortingEnabled = table->isSortingEnabled();
    const QHeaderView* header = table->horizontalHeader();
    if (!header)
        return state;

    const int sortColumn = header->sortIndicatorSection();
    state.column = sortColumn;
    state.order = header->sortIndicatorOrder();
    state.hasValidSort = sortColumn >= 0 && sortColumn < table->columnCount();
    return state;
}

void TableClipboardUtils::RestoreSortState(QTableWidget* table, const SortState& state, int defaultColumn, Qt::SortOrder defaultOrder)
{
    if (!table)
        return;

    if (!state.wasSortingEnabled && defaultColumn < 0)
    {
        table->setSortingEnabled(false);
        return;
    }

    table->setSortingEnabled(true);

    int sortColumn = state.hasValidSort ? state.column : defaultColumn;
    Qt::SortOrder sortOrder = state.hasValidSort ? state.order : defaultOrder;
    if (sortColumn >= 0 && sortColumn < table->columnCount())
        table->sortItems(sortColumn, sortOrder);
}
