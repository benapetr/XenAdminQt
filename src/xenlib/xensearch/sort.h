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

// sort.h - Sort specification for search results
// C# equivalent: xenadmin/XenModel/XenSearch/Sort.cs

#ifndef SORT_H
#define SORT_H

#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

// Forward declaration
class XenObject;

/*!
 * \brief Sort specification for search results
 *
 * C# equivalent: XenAdmin.XenSearch.Sort (xenadmin/XenModel/XenSearch/Sort.cs)
 *
 * Represents a sort criterion for search results, including the column name
 * and sort direction (ascending/descending).
 *
 * Usage example:
 * \code
 * Sort sort("name", true);  // Sort by name, ascending
 * int result = sort.Compare(obj1, obj2);  // Compare two objects
 * \endcode
 */
class Sort
{
    public:
        /*!
         * \brief Construct a Sort specification
         * \param column Column name to sort by (e.g. "name", "cpu", "memory")
         * \param ascending true for ascending sort, false for descending
         *
         * C# equivalent: Sort constructor (Sort.cs lines 48-53)
         */
        Sort(const QString& column, bool ascending);

        /*!
         * \brief Construct Sort from XML node
         * \param reader XML stream reader positioned at <sort> element
         *
         * C# equivalent: Sort(XmlNode node) (Sort.cs lines 105-108)
         */
        explicit Sort(QXmlStreamReader& reader);

        /*!
         * \brief Default constructor for container use
         */
        Sort();

        /*!
         * \brief Copy constructor
         */
        Sort(const Sort& other);

        /*!
         * \brief Assignment operator
         */
        Sort& operator=(const Sort& other);

        /*!
         * \brief Destructor
         */
        ~Sort();

        /*!
         * \brief Get the column name to sort by
         * \return Column name (e.g. "name", "cpu", "memory")
         *
         * C# equivalent: Column property (Sort.cs lines 45-48)
         */
        QString GetColumn() const { return this->column_; }

        /*!
         * \brief Get the sort direction
         * \return true if ascending, false if descending
         *
         * C# equivalent: Ascending property (Sort.cs lines 50-53)
         */
        bool IsAscending() const { return this->ascending_; }

        /*!
         * \brief Write this Sort to XML
         * \param writer XML stream writer
         *
         * C# equivalent: ToXmlNode (Sort.cs lines 110-119)
         */
        void ToXml(QXmlStreamWriter& writer) const;

        /*!
         * \brief Compare two XenObject instances
         * \param one First object
         * \param other Second object
         * \return Negative if one < other, 0 if equal, positive if one > other
         *
         * C# equivalent: Compare method (Sort.cs lines 121-157)
         *
         * Note: Currently simplified - does not handle custom fields
         */
        int Compare(XenObject* one, XenObject* other) const;

        /*!
         * \brief Check equality with another Sort
         * \param other Other Sort to compare with
         * \return true if column and direction match
         *
         * C# equivalent: Equals override (Sort.cs lines 159-167)
         */
        bool operator==(const Sort& other) const;

        /*!
         * \brief Check inequality with another Sort
         */
        bool operator!=(const Sort& other) const { return !(*this == other); }

    private:
        QString column_;      ///< Column name to sort by
        bool ascending_;      ///< true = ascending, false = descending
};

#endif // SORT_H
