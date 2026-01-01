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

// ieditpage.h - Base interface for property editor pages
// Matches C# XenAdmin.SettingsPanels.IEditPage interface
#ifndef IEDITPAGE_H
#define IEDITPAGE_H

#include <QWidget>
#include <QString>
#include <QIcon>
#include <QVariantMap>

// Forward declarations
class AsyncOperation;

/**
 * @brief IEditPage - Interface for property editor pages
 *
 * Qt equivalent of C# XenAdmin.SettingsPanels.IEditPage
 *
 * This interface defines the contract for property editor pages used in
 * PropertiesDialog (Host, VM, Pool, SR, etc.). Each page displays and edits
 * properties of a XenServer object.
 *
 * Key concepts from C# implementation:
 *
 * 1. **Two-phase save pattern**:
 *    a) Simple field changes: Edit object properties directly via setters
 *    b) Complex operations: Return AsyncOperation for API calls
 *
 * 2. **Object cloning**: Pages work with cloned object data (xenObjectCopy)
 *    to allow cancel without affecting the original cached data.
 *
 * 3. **Validation**: Pages validate their fields before save is allowed.
 *
 * 4. **Change tracking**: Pages track whether any fields have been modified.
 *
 * Architecture pattern (matching C# PropertiesDialog.btnOK_Click):
 * ```
 * // When user clicks OK:
 * foreach (IEditPage page in pages) {
 *     if (!page->isValidToSave()) {
 *         // Show validation messages, abort save
 *         return;
 *     }
 * }
 *
 * // Collect actions from all pages:
 * QList<AsyncOperation*> actions;
 * foreach (IEditPage page in pages) {
 *     if (page->hasChanged()) {
 *         AsyncOperation* action = page->saveSettings();
 *         if (action) {
 *             actions.append(action);
 *         }
 *     }
 * }
 *
 * // Execute all actions:
 * MultipleOperation* multiOp = new MultipleOperation(connection, actions);
 * multiOp->runAsync();
 * ```
 *
 * C# source: XenAdmin/SettingsPanels/IEditPage.cs
 */

class XenConnection; // Forward declaration

class IEditPage : public QWidget
{
    Q_OBJECT

    public:
        explicit IEditPage(QWidget* parent = nullptr)
            : QWidget(parent), m_connection(nullptr)
        {}
        virtual ~IEditPage() = default;

        /**
         * @brief Set the XenConnection for this page
         * Called by VerticallyTabbedDialog before setXenObjects
         */
        void setConnection(XenConnection* connection)
        {
            m_connection = connection;
        }

        /**
         * @brief Get the XenConnection for creating actions
         */
        XenConnection* connection() const
        {
            return m_connection;
        }

        // IVerticalTab interface (for display in VerticalTabWidget)
        // C# equivalent: Implements VerticalTabs.IVerticalTab

        /**
         * @brief Main text for the tab
         * @return Tab title (e.g. "General", "CPU and Memory", "Storage")
         *
         * C# equivalent: string Text { get; }
         */
        virtual QString text() const = 0;

        /**
         * @brief Sub text for the tab (optional detail line)
         * @return Secondary text or empty string
         *
         * C# equivalent: string SubText { get; }
         */
        virtual QString subText() const = 0;

        /**
         * @brief Icon for the tab
         * @return QIcon for tab display
         *
         * C# equivalent: Image Image { get; }
         */
        virtual QIcon image() const = 0;

        // IEditPage interface
        // C# source: XenAdmin/SettingsPanels/IEditPage.cs

        /**
         * @brief Set the objects being edited
         * @param objectRef OpaqueRef of the original object (from cache)
         * @param objectType Type string ("vm", "host", "pool", "sr", etc.)
         * @param objectDataBefore Original object data (for comparison and reference)
         * @param objectDataCopy Cloned object data (page edits this copy)
         *
         * Pages work with TWO copies of object data:
         * - **objectDataBefore**: The original cached data, read-only, for reference
         * - **objectDataCopy**: A clone that page can modify (not committed until save)
         *
         * Most pages only need objectDataCopy, but some operations (like actions)
         * need both to know what changed.
         *
         * C# equivalent: void SetXenObjects(IXenObject orig, IXenObject clone)
         *
         * Example implementation:
         * ```cpp
         * void GeneralEditPage::setXenObjects(const QString& objectRef,
         *                                      const QString& objectType,
         *                                      const QVariantMap& objectDataBefore,
         *                                      const QVariantMap& objectDataCopy)
         * {
         *     m_objectRef = objectRef;
         *     m_objectType = objectType;
         *     m_objectDataBefore = objectDataBefore;
         *     m_objectDataCopy = objectDataCopy;
         *
         *     // Load data into UI fields:
         *     ui->nameLineEdit->setText(objectDataCopy["name_label"].toString());
         *     ui->descriptionTextEdit->setPlainText(objectDataCopy["name_description"].toString());
         * }
         * ```
         */
        virtual void setXenObjects(const QString& objectRef,
                                   const QString& objectType,
                                   const QVariantMap& objectDataBefore,
                                   const QVariantMap& objectDataCopy) = 0;

        /**
         * @brief Save settings from this page
         * @return AsyncOperation* if complex API calls needed, nullptr if only simple edits
         *
         * This is the core save method. It does TWO things:
         *
         * 1. **Edit objectDataCopy directly** for simple field changes:
         *    - Name, description, basic properties
         *    - These changes are NOT committed to server yet
         *    - They're just stored in the local objectDataCopy map
         *
         * 2. **Return AsyncOperation** for complex operations:
         *    - Folder/tag changes → GeneralEditPageAction
         *    - Network configuration → ChangeNetworkingAction
         *    - HA settings → EnableHAAction
         *    - Any operation requiring multiple API calls
         *
         * The PropertiesDialog collects all actions from all pages, then:
         * 1. Commits simple changes (by setting object properties on server)
         * 2. Runs all returned actions sequentially
         *
         * C# equivalent: AsyncAction SaveSettings()
         *
         * Example (matching C# GeneralEditPage.SaveSettings()):
         * ```cpp
         * AsyncOperation* GeneralEditPage::saveSettings()
         * {
         *     // Simple edits to objectDataCopy (NOT API calls yet):
         *     if (m_nameChanged) {
         *         m_objectDataCopy["name_label"] = ui->nameLineEdit->text();
         *     }
         *     if (m_descriptionChanged) {
         *         m_objectDataCopy["name_description"] = ui->descriptionTextEdit->toPlainText();
         *     }
         *
         *     // Complex operation requiring action:
         *     if (m_folderChanged || m_tagsChanged) {
         *         return new GeneralEditPageAction(
         *             xenLib()->getConnection(),
         *             m_objectRef,
         *             m_objectType,
         *             m_oldFolder,
         *             m_newFolder,
         *             m_oldTags,
         *             m_newTags
         *         );
         *     }
         *
         *     return nullptr;  // No action needed, just simple edits
         * }
         * ```
         */
        virtual AsyncOperation* saveSettings() = 0;

        /**
         * @brief Check if all fields are valid for saving
         * @return true if page can be saved, false if validation errors exist
         *
         * Called before save to ensure all fields pass validation.
         * If false, save is aborted and showLocalValidationMessages() is called.
         *
         * C# equivalent: bool ValidToSave { get; }
         *
         * Example:
         * ```cpp
         * bool GeneralEditPage::isValidToSave() const
         * {
         *     if (ui->nameLineEdit->text().trimmed().isEmpty()) {
         *         return false;  // Name is required
         *     }
         *     return true;
         * }
         * ```
         */
        virtual bool isValidToSave() const = 0;

        /**
         * @brief Show validation error messages to user
         *
         * Called when isValidToSave() returns false.
         * Should display balloon tooltips or error labels near invalid fields.
         *
         * C# equivalent: void ShowLocalValidationMessages()
         */
        virtual void showLocalValidationMessages() = 0;

        /**
         * @brief Hide validation error messages
         *
         * Called when page is deselected or validation passes.
         * Should hide any balloon tooltips or error labels.
         *
         * C# equivalent: void HideLocalValidationMessages()
         */
        virtual void hideLocalValidationMessages() = 0;

        /**
         * @brief Clean up resources (unregister listeners, dispose tooltips, etc.)
         *
         * Called when dialog is closing.
         * Should disconnect signals, delete tooltips, etc.
         *
         * C# equivalent: void Cleanup()
         */
        virtual void cleanup() = 0;

        /**
         * @brief Check if any fields have been modified
         * @return true if page has unsaved changes, false otherwise
         *
         * Used to determine if page needs to be saved.
         * If no pages have changed, OK button just closes dialog without API calls.
         *
         * C# equivalent: bool HasChanged { get; }
         *
         * Example:
         * ```cpp
         * bool GeneralEditPage::hasChanged() const
         * {
         *     return m_nameChanged || m_descriptionChanged ||
         *            m_folderChanged || m_tagsChanged;
         * }
         * ```
         */
        virtual bool hasChanged() const = 0;

        /**
         * @brief Get the modified object data copy after saveSettings() is called
         * @return QVariantMap with modifications made by the page
         *
         * This allows the dialog to retrieve changes made by the page to its local
         * m_objectDataCopy so they can be applied via applySimpleChanges().
         *
         * Called by VerticallyTabbedDialog::collectActions() after saveSettings().
         *
         * Default implementation returns empty map (no simple changes to apply).
         * Override in pages that modify objectDataCopy directly (like GeneralEditPage).
         */
        virtual QVariantMap getModifiedObjectData() const
        {
            return QVariantMap(); // Default: no simple changes
        }

    signals:
        /**
         * @brief Emitted when page finishes building/populating
         *
         * Used by PropertiesDialog to know when to refresh tab display
         * after async data loading completes.
         *
         * C# equivalent: Not explicitly in interface, but pages fire
         * events that PropertiesDialog listens to
         */
        void populated();

    protected:
        XenConnection* m_connection; // Accessible to subclasses for creating actions
};

#endif // IEDITPAGE_H
