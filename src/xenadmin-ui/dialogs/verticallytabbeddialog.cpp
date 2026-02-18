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

// verticallytabbeddialog.cpp - Base class for properties dialogs with vertical tabs
#include "verticallytabbeddialog.h"
#include "ui_verticallytabbeddialog.h"
#include "actionprogressdialog.h"
#include "xenlib/xen/actions/general/savechangesaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xencache.h"
#include "xenlib/operations/multipleaction.h"
#include "xenlib/xen/asyncoperation.h"
#include "xenlib/xen/xenobject.h"
#include <QPushButton>
#include <QDebug>

VerticallyTabbedDialog::VerticallyTabbedDialog(QSharedPointer<XenObject> object, QWidget* parent) : QDialog(parent), ui(new Ui::VerticallyTabbedDialog)
{
    if (!object.isNull())
    {
        this->m_objectRef = object->OpaqueRef();
        this->m_objectType = object->GetObjectType();
        this->m_object = object;
    }

    this->ui->setupUi(this);

    // Connect vertical tabs selection changed
    connect(this->ui->verticalTabs, &VerticalTabWidget::currentRowChanged, this, &VerticallyTabbedDialog::onVerticalTabsCurrentChanged);

    // Connect button box signals
    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &VerticallyTabbedDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &VerticallyTabbedDialog::reject);
    
    // Connect Apply button to save changes without closing dialog
    QPushButton* applyButton = this->ui->buttonBox->button(QDialogButtonBox::Apply);
    if (applyButton)
    {
        connect(applyButton, &QPushButton::clicked, this, &VerticallyTabbedDialog::onApplyClicked);
    }

    // Load object data from XenServer
    this->loadObjectData();

    XenConnection* conn = this->m_object ? this->m_object->GetConnection() : nullptr;
    XenCache* cache = conn ? conn->GetCache() : nullptr;
    if (cache)
    {
        connect(cache, &XenCache::objectChanged, this, &VerticallyTabbedDialog::onCacheObjectChanged, Qt::UniqueConnection);
    }

    // NOTE: Subclass must call build() in its constructor after this base constructor
    // Cannot call pure virtual from base constructor in C++
}

VerticallyTabbedDialog::~VerticallyTabbedDialog()
{
    delete this->ui;
}

void VerticallyTabbedDialog::loadObjectData()
{
    // Load object data from XenCache
    if (!this->m_object || !this->m_object->GetConnection())
    {
        qWarning() << "VerticallyTabbedDialog: No connection available";
        this->m_objectDataBefore = QVariantMap();
        this->m_objectDataCopy = QVariantMap();
        return;
    }
    
    // Resolve object data from cache (matches C# clone pattern)
    QVariantMap objectData = this->m_object->GetData();
    
    if (objectData.isEmpty())
    {
        qWarning() << "VerticallyTabbedDialog: Failed to load data for"
                   << XenObject::TypeToString(this->m_objectType) << this->m_objectRef;
        this->m_objectDataBefore = QVariantMap();
        this->m_objectDataCopy = QVariantMap();
        return;
    }

    // Create two copies: _xenObjectBefore (for cancel) and _xenObjectCopy (for editing)
    // C# equivalent: _xenObjectBefore = xenObject.Locked ? xenObject : xenObject.Clone(false);
    //                _xenObjectCopy = xenObject.Locked ? xenObject : xenObject.Clone(false);
    this->m_objectDataBefore = objectData;  // Immutable reference copy
    this->m_objectDataCopy = objectData;    // Working copy for editing
    
    qDebug() << "VerticallyTabbedDialog: Loaded data for"
             << XenObject::TypeToString(this->m_objectType) << this->m_objectRef
             << "- name_label:" << objectData.value("name_label").toString();
}

void VerticallyTabbedDialog::refreshPagesFromCurrentData()
{
    this->loadObjectData();
    for (IEditPage* page : this->m_pages)
    {
        page->SetXenObject(this->m_object, this->m_objectDataBefore, this->m_objectDataCopy);
        this->ui->verticalTabs->UpdateTabSubText(page, page->GetSubText());
    }
}

void VerticallyTabbedDialog::showTab(IEditPage* page)
{
    if (!page || !this->m_object)
        return;

    this->m_pages.append(page);

    // Add to vertical tabs
    this->ui->verticalTabs->AddTab(page->GetImage(), page->GetText(), page->GetSubText(), page);

    // Add to content panel (stacked widget)
    this->ui->ContentPanel->addWidget(page);

    // Set connection so pages can create actions
    page->SetConnection(this->m_object->GetConnection());

    // Initialize page with object data
    // C# equivalent: editPage.SetXenObject(_xenObjectBefore, _xenObjectCopy);
    page->SetXenObject(this->m_object, this->m_objectDataBefore, this->m_objectDataCopy);
    this->ui->verticalTabs->UpdateTabSubText(page, page->GetSubText());

    // Connect populated signal to refresh tabs
    connect(page, &IEditPage::populated, [this, page]()
    {
        this->ui->verticalTabs->UpdateTabSubText(page, page->GetSubText());
    });
}

void VerticallyTabbedDialog::selectPage(IEditPage* page)
{
    int index = this->m_pages.indexOf(page);
    if (index >= 0)
    {
        this->ui->verticalTabs->setCurrentRow(index);
    }
}

void VerticallyTabbedDialog::accept()
{
    // C# equivalent: btnOK_Click()
    
    // Perform save - if successful, close dialog
    if (this->performSave(true)) // true = close dialog on success
    {
        QDialog::accept();
    }
}

void VerticallyTabbedDialog::onApplyClicked()
{
    // Apply changes without closing dialog
    // C# equivalent: btnApply_Click()
    
    this->performSave(false); // false = keep dialog open
}

bool VerticallyTabbedDialog::performSave(bool closeOnSuccess)
{
    if (!this->m_object)
        return false;

    // Shared save logic for both OK and Apply buttons
    
    // Step 1: Validate all pages
    // C# code: foreach (IEditPage editPage in verticalTabs.Items)
    //              if (!editPage.ValidToSave) { ... }
    for (IEditPage* page : this->m_pages)
    {
        if (!page->IsValidToSave())
        {
            this->selectPage(page);
            page->ShowLocalValidationMessages();
            return false; // Abort save
        }
    }

    // Step 2: Check if any changes exist
    // C# code: if (!changedPageExists) { Close(); return; }
    bool hasChanges = false;
    for (IEditPage* page : this->m_pages)
    {
        if (page->HasChanged())
        {
            hasChanges = true;
            break;
        }
    }

    if (!hasChanges)
    {
        // No changes - close if requested (OK button), otherwise do nothing (Apply button)
        return closeOnSuccess;
    }

    // Step 3: Collect actions from all pages
    // C# equivalent: List<AsyncAction> actions = SaveSettings();
    QList<AsyncOperation*> actions = this->collectActions();

    // Step 4: Insert SaveChangesAction first (C# parity)
    // C# code:
    //   actions.Insert(index, new SaveChangesAction(_xenObjectCopy, true, _xenObjectBefore));
    //   _action = new MultipleAction(connection, title, desc, completedDesc, actions);
    //   using (var dialog = new ActionProgressDialog(_action, ProgressBarStyle.Marquee))
    //   {
    //       _action.RunAsync();
    //       dialog.ShowDialog(this);
    //   }

    AsyncOperation* saveChangesAction = new SaveChangesAction(this->m_object,
                                                              this->m_objectDataBefore,
                                                              this->m_objectDataCopy,
                                                              true,
                                                              nullptr);
    saveChangesAction->setParent(nullptr);
    actions.prepend(saveChangesAction);

    // Create progress dialog - it will own the MultipleOperation and all sub-actions
    QString objName = this->m_object->GetName();
    
    // Now create the MultipleAction with progressDialog as parent
    // This ensures proper ownership: dialog owns operation, operation owns sub-actions
    MultipleAction* multiOp = new MultipleAction(
        this->m_object->GetConnection(),
        tr("Update Properties - %1").arg(objName),
        tr("Updating properties..."),
        tr("Properties updated"),
        actions,
        true,  // suppressHistory
        false, // showSubOperationDetails
        false, // stopOnFirstException
        nullptr); // Progress dialog will own the operation

    ActionProgressDialog* progressDialog = new ActionProgressDialog(multiOp, this);
    progressDialog->setWindowTitle(tr("Update Properties - %1").arg(objName));
    
    // Re-parent the operation to the progress dialog for proper cleanup
    multiOp->setParent(progressDialog);

    // Connect completion handler
    bool saveSucceeded = false;
    connect(multiOp, &MultipleAction::completed, this, [this, multiOp, progressDialog, closeOnSuccess, &saveSucceeded]()
    {
        if (multiOp->IsCompleted() && !multiOp->HasError())
        {
            progressDialog->accept();
            saveSucceeded = true;
            
            if (closeOnSuccess)
            {
                // OK path closes in VerticallyTabbedDialog::accept() after performSave() returns true.
            }
            else
            {
                // Apply button - keep baseline and wait for cache objectChanged.
                this->m_objectDataBefore = this->m_objectDataCopy;
                this->m_waitingForCacheSync = true;
                for (IEditPage* page : this->m_pages)
                    this->ui->verticalTabs->UpdateTabSubText(page, page->GetSubText());
            }
        }
        else
        {
            // Error is shown by OperationProgressDialog
            progressDialog->reject();
            // Don't close properties dialog on error - let user retry
        }
    });

    // Show progress dialog modally. ActionProgressDialog starts the operation
    // in showEvent(), so we must not call RunAsync() here.
    progressDialog->exec(); // Blocks until operation completes
    
    // Dialog and operation are automatically cleaned up when dialog closes
    return saveSucceeded;
}

void VerticallyTabbedDialog::onCacheObjectChanged(XenConnection* connection, const QString& type, const QString& ref)
{
    if (!this->m_waitingForCacheSync || !this->m_object)
        return;

    XenConnection* expectedConnection = this->m_object->GetConnection();
    if (!expectedConnection || connection != expectedConnection)
        return;

    const XenObjectType changedType = XenCache::TypeFromString(type);
    if (changedType != this->m_objectType || ref != this->m_objectRef)
        return;

    for (IEditPage* page : this->m_pages)
    {
        if (page->HasChanged())
        {
            qDebug() << "VerticallyTabbedDialog: cache sync detected but local edits exist, skipping auto-refresh";
            return;
        }
    }

    qDebug() << "VerticallyTabbedDialog: cache sync detected for edited object, refreshing pages";
    this->m_waitingForCacheSync = false;
    this->refreshPagesFromCurrentData();
}

void VerticallyTabbedDialog::reject()
{
    // C# equivalent: btnCancel_Click()

    // Cleanup pages
    for (IEditPage* page : m_pages)
    {
        page->Cleanup();
    }

    QDialog::reject();
}

void VerticallyTabbedDialog::closeEvent(QCloseEvent* event)
{
    // C# equivalent: OnFormClosing()

    // Cleanup pages
    for (IEditPage* page : this->m_pages)
    {
        page->Cleanup();
    }

    QDialog::closeEvent(event);
}

QList<AsyncOperation*> VerticallyTabbedDialog::collectActions()
{
    // C# equivalent: SaveSettings() method
    // C# code:
    //   foreach (IEditPage editPage in verticalTabs.Items) {
    //       if (!editPage.HasChanged) continue;
    //       AsyncAction action = editPage.SaveSettings();
    //       if (action == null) continue;
    //       actions.Add(action);
    //   }

    QList<AsyncOperation*> actions;

    for (IEditPage* page : this->m_pages)
    {
        if (!page->HasChanged())
        {
            continue;
        }

        // Call SaveSettings() - this updates the page's local m_objectDataCopy
        AsyncOperation* action = page->SaveSettings();
        
        // Copy the modified data back from the page to the dialog's copy
        // so SaveChangesAction sees the latest simple-field state.
        QVariantMap pageData = page->GetModifiedObjectData();
        
        // Merge simple fields that pages modify directly
        if (pageData.contains("name_label"))
        {
            this->m_objectDataCopy["name_label"] = pageData.value("name_label");
        }
        if (pageData.contains("name_description"))
        {
            this->m_objectDataCopy["name_description"] = pageData.value("name_description");
        }
        
        // Merge other_config
        if (pageData.contains("other_config"))
        {
            QVariantMap pageOtherConfig = pageData.value("other_config").toMap();
            QVariantMap dialogOtherConfig = this->m_objectDataCopy.value("other_config").toMap();
            
            // Copy all keys from page to dialog (page has priority)
            for (auto it = pageOtherConfig.constBegin(); it != pageOtherConfig.constEnd(); ++it)
            {
                dialogOtherConfig[it.key()] = it.value();
            }
            this->m_objectDataCopy["other_config"] = dialogOtherConfig;
        }

        if (pageData.contains("logging"))
        {
            this->m_objectDataCopy["logging"] = pageData.value("logging").toMap();
        }

        if (pageData.contains("VCPUs_params"))
        {
            this->m_objectDataCopy["VCPUs_params"] = pageData.value("VCPUs_params");
        }

        if (pageData.contains("platform"))
        {
            this->m_objectDataCopy["platform"] = pageData.value("platform");
        }

        if (pageData.contains("HVM_shadow_multiplier"))
        {
            this->m_objectDataCopy["HVM_shadow_multiplier"] = pageData.value("HVM_shadow_multiplier");
        }
        
        if (action)
        {
            // IMPORTANT: Remove parent from action so it won't be destroyed when this dialog closes
            // The action will be owned by MultipleOperation, which is owned by OperationProgressDialog
            action->setParent(nullptr);
            actions.append(action);
            qDebug() << "VerticallyTabbedDialog: Collected action from page:" << page->GetText();
        }
    }

    return actions;
}
void VerticallyTabbedDialog::onVerticalTabsCurrentChanged(int index)
{
    if (index < 0 || index >= this->m_pages.size())
        return;

    IEditPage* page = this->m_pages[index];

    // Update header to match selected page
    this->ui->TabImage->setPixmap(page->GetImage().pixmap(32, 32));
    this->ui->TabTitle->setText(page->GetText());

    // Show selected page in content panel
    this->ui->ContentPanel->setCurrentWidget(page);

    // Hide validation messages on other pages
    for (IEditPage* otherPage : this->m_pages)
    {
        if (otherPage != page)
        {
            otherPage->HideLocalValidationMessages();
        }
    }
}

// Collect actions from all changed pages
