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

#ifndef PDSECTION_H
#define PDSECTION_H

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <functional>

class Command;

/**
 * @brief PDSection - Collapsible property display section for General Tab
 * 
 * Port of C# XenAdmin.Controls.PDSection which displays key-value pairs
 * in an expandable/collapsible table format. Used extensively in GeneralTabPage
 * to show object properties organized by category (General, Memory, CPU, etc.)
 * 
 * Features:
 * - Expandable/collapsible header with chevron icon
 * - Three-column table: Key, Value, Notes
 * - Support for link cells that execute commands/actions
 * - Context menu with copy functionality
 * - Automatic height adjustment based on content
 * - Selection persistence across rebuilds
 */
class PDSection : public QWidget
{
    Q_OBJECT

    public:
        explicit PDSection(QWidget* parent = nullptr);
        ~PDSection();

        //! Get/set the section title displayed in the header
        QString GetSectionTitle() const;
        void SetSectionTitle(const QString& title);

        //! Check if the section has no data rows
        bool IsEmpty() const;

        //! Check if the section is expanded
        bool IsExpanded() const;

        //! Check if no row is selected
        bool HasNoSelection() const;

        //! Get bounds of selected row (for scrolling)
        QRect GetSelectedRowBounds() const;

        //! Enable/disable focus event handling
        void SetDisableFocusEvent(bool disable);

        //! Enable/disable cell tooltips
        void SetShowCellToolTips(bool show);
        bool GetShowCellToolTips() const;

        //! Expand the section
        void Expand();

        //! Collapse the section
        void Collapse();

        /**
         * @brief Add a simple key-value entry
         * @param key The property name
         * @param value The property value
         * @param contextMenuItems Optional context menu items for this row
         */
        void AddEntry(const QString& key, const QString& value, const QList<QAction*>& contextMenuItems = QList<QAction*>());

        /**
         * @brief Add a key-value entry with custom text color
         * @param key The property name
         * @param value The property value
         * @param fontColor Color for the value text
         * @param contextMenuItems Optional context menu items
         */
        void AddEntry(const QString& key, const QString& value, const QColor& fontColor, const QList<QAction*>& contextMenuItems = QList<QAction*>());

        /**
         * @brief Add an entry with a clickable link in the value column
         * @param key The property name
         * @param value The link text
         * @param command Command to execute when link is clicked
         * @param contextMenuItems Optional context menu items
         */
        void AddEntryLink(const QString& key, const QString& value, Command* command, const QList<QAction*>& contextMenuItems = QList<QAction*>());

        /**
         * @brief Add an entry with a clickable link in the value column
         * @param key The property name
         * @param value The link text
         * @param action Function to execute when link is clicked
         * @param contextMenuItems Optional context menu items
         */
        void AddEntryLink(const QString& key, const QString& value, std::function<void()> action, const QList<QAction*>& contextMenuItems = QList<QAction*>());

        /**
         * @brief Add an entry with value and a clickable note link
         * @param key The property name
         * @param value The property value (text)
         * @param note The link text in notes column
         * @param action Function to execute when note link is clicked
         * @param enabled Whether the entry is enabled
         * @param contextMenuItems Optional context menu items
         */
        void AddEntryWithNoteLink(const QString& key, const QString& value, const QString& note, 
                                  std::function<void()> action, bool enabled = true,
                                  const QList<QAction*>& contextMenuItems = QList<QAction*>());

        /**
         * @brief Add an entry with value and note link, with custom color
         * @param key The property name
         * @param value The property value
         * @param note The link text in notes column
         * @param action Function to execute when note link is clicked
         * @param fontColor Color for the value text
         * @param contextMenuItems Optional context menu items
         */
        void AddEntryWithNoteLink(const QString& key, const QString& value, const QString& note,
                                  std::function<void()> action, const QColor& fontColor,
                                  const QList<QAction*>& contextMenuItems = QList<QAction*>());

        /**
         * @brief Update an existing entry's value by key
         * @param key The key to search for
         * @param newValue The new value to set
         * @param visible Whether the row should be visible
         */
        void UpdateEntryValueWithKey(const QString& key, const QString& newValue, bool visible);

        /**
         * @brief Fix the width of the first column (key column)
         * @param width Width in pixels
         */
        void FixFirstColumnWidth(int width);

        /**
         * @brief Clear all data rows (stores selection for restoration)
         */
        void ClearData();

        /**
         * @brief Restore selection to row from last ClearData() call
         */
        void RestoreSelection();

        /**
         * @brief Pause layout updates (for batch operations)
         */
        void PauseLayout();

        /**
         * @brief Resume layout updates and refresh height
         */
        void StartLayout();

    signals:
        //! Emitted when the table receives focus
        void ContentReceivedFocus(PDSection* section);

        //! Emitted when selection changes in the table
        void ContentChangedSelection(PDSection* section);

        //! Emitted when expanded state changes
        void ExpandedChanged(PDSection* section);

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;
        void focusInEvent(QFocusEvent* event) override;

    private slots:
        void onHeaderDoubleClicked();
        void onChevronClicked();
        void onTableCellClicked(int row, int column);
        void onTableSelectionChanged();
        void onTableContextMenuRequested(const QPoint& pos);
        void onTableCellEntered(int row, int column);
        void onCopyMenuItemTriggered();

    private:
        //! Create the key cell text (adds separator)
        QString createKeyText(const QString& key) const;

        //! Add a row to the table
        void addRow(const QString& keyText, const QString& valueText, const QString& noteText,
                   bool isValueLink, bool isNoteLink, std::function<void()> linkAction,
                   bool enabled, const QList<QAction*>& contextMenuItems, const QColor& fontColor = QColor());

        //! Toggle expanded state
        void toggleExpandedState(bool expand);

        //! Refresh the section height based on content
        void refreshHeight();

        //! Refresh the chevron icon based on state
        void refreshChevron();

        //! Run the command or action associated with a cell
        void runCellCommandOrAction(int row, int column);

        // UI components
        QVBoxLayout* mainLayout_;
        QWidget* headerPanel_;
        QLabel* titleLabel_;
        QPushButton* chevronButton_;
        QTableWidget* tableWidget_;
        QMenu* contextMenu_;
        QAction* copyAction_;

        // State
        bool isExpanded_;
        bool inLayout_;
        bool disableFocusEvent_;
        QString previousSelectionKey_;
        bool chevronHot_;

        // Row data storage for commands/actions
        struct RowData
        {
            bool isValueLink;
            bool isNoteLink;
            std::function<void()> linkAction;
            QList<QAction*> contextMenuItems;
        };
        QMap<int, RowData> rowData_;
};

#endif // PDSECTION_H
