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

#ifndef TABLECLIPBOARDUTILS_H
#define TABLECLIPBOARDUTILS_H

#include <QList>
#include <QString>
#include <QStringList>
#include <Qt>

class QTableWidget;

class TableClipboardUtils
{
    public:
        struct SortState
        {
            int column = -1;
            Qt::SortOrder order;
            bool wasSortingEnabled = false;
            bool hasValidSort = false;
        };

        /**
         * @brief Build CSV text from a table widget.
         * @param table Source table.
         * @param skipEmptyDecorativeColumns Skip columns that have empty headers and no visible text values.
         */
        static QString BuildCsvFromTable(const QTableWidget* table, bool skipEmptyDecorativeColumns = true);

        /**
         * @brief Build CSV text and copy it into the system clipboard.
         * @param table Source table.
         * @param skipEmptyDecorativeColumns Skip columns that have empty headers and no visible text values.
         */
        static void CopyTableCsvToClipboard(const QTableWidget* table, bool skipEmptyDecorativeColumns = true);

        /**
         * @brief Escapes one value for RFC4180-style CSV output.
         */
        static QString CsvEscapeField(const QString& value);

        /**
         * @brief Builds one CSV row from fields.
         */
        static QString CsvJoinRow(const QStringList& fields);

        /**
         * @brief Builds a CSV document from optional headers and row data.
         */
        static QString BuildCsvDocument(const QStringList& headers, const QList<QStringList>& rows);

        /**
         * @brief Captures current sort settings so they can be restored after a table rebuild.
         */
        static SortState CaptureSortState(const QTableWidget* table);

        /**
         * @brief Restores sorting after table rebuild and reapplies either previous or default sort.
         */
        static void RestoreSortState(QTableWidget* table, const SortState& state,
                                     int defaultColumn = -1, Qt::SortOrder defaultOrder = Qt::AscendingOrder);
};

#endif // TABLECLIPBOARDUTILS_H
