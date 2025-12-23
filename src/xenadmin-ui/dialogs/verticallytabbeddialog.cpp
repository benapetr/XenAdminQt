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
#include "operationprogressdialog.h"
#include "../mainwindow.h"
#include "../../xenlib/xenlib.h"
#include "../../xenlib/xen/network/connection.h"
#include "../../xenlib/xen/session.h"
#include "../../xenlib/xen/xenapi/xenapi_VM.h"
#include "../../xenlib/xen/xenapi/xenapi_Host.h"
#include "../../xenlib/xen/xenapi/xenapi_Pool.h"
#include "../../xenlib/xen/xenapi/xenapi_SR.h"
#include "../../xenlib/xen/xenapi/xenapi_Network.h"
#include <QtGlobal>
#include "../../xenlib/xencache.h"
#include "../../xenlib/operations/multipleoperation.h"
#include "../../xenlib/xen/asyncoperation.h"
#include <QMessageBox>
#include <QPushButton>
#include <QDebug>

VerticallyTabbedDialog::VerticallyTabbedDialog(XenConnection* connection,
                                               const QString& objectRef,
                                               const QString& objectType,
                                               QWidget* parent)
    : QDialog(parent), ui(new Ui::VerticallyTabbedDialog), m_connection(connection), m_objectRef(objectRef), m_objectType(objectType.toLower())
{
    this->ui->setupUi(this);

    // Connect vertical tabs selection changed
    connect(this->ui->verticalTabs, &VerticalTabWidget::currentRowChanged,
            this, &VerticallyTabbedDialog::onVerticalTabsCurrentChanged);

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
    if (!this->m_connection || !this->m_connection->getCache())
    {
        qWarning() << "VerticallyTabbedDialog: No connection or cache available";
        this->m_objectDataBefore = QVariantMap();
        this->m_objectDataCopy = QVariantMap();
        return;
    }

    XenCache* cache = this->m_connection->getCache();
    
    // Resolve object data from cache (matches C# clone pattern)
    QVariantMap objectData = cache->ResolveObjectData(this->m_objectType, this->m_objectRef);
    
    if (objectData.isEmpty())
    {
        qWarning() << "VerticallyTabbedDialog: Failed to load data for" 
                   << this->m_objectType << this->m_objectRef;
        this->m_objectDataBefore = QVariantMap();
        this->m_objectDataCopy = QVariantMap();
        return;
    }

    // Create two copies: _xenObjectBefore (for cancel) and _xenObjectCopy (for editing)
    // C# equivalent: _xenObjectBefore = xenObject.Locked ? xenObject : xenObject.Clone(false);
    //                _xenObjectCopy = xenObject.Locked ? xenObject : xenObject.Clone(false);
    this->m_objectDataBefore = objectData;  // Immutable reference copy
    this->m_objectDataCopy = objectData;    // Working copy for editing
    
    qDebug() << "VerticallyTabbedDialog: Loaded data for" << this->m_objectType << this->m_objectRef
             << "- name_label:" << objectData.value("name_label").toString();
}

void VerticallyTabbedDialog::showTab(IEditPage* page)
{
    if (!page)
        return;

    this->m_pages.append(page);

    // Add to vertical tabs
    this->ui->verticalTabs->addTab(page->image(), page->text(), page->subText(), page);

    // Add to content panel (stacked widget)
    this->ui->ContentPanel->addWidget(page);

    // Set connection so pages can create actions
    page->setConnection(this->m_connection);

    // Initialize page with object data
    // C# equivalent: editPage.SetXenObjects(_xenObjectBefore, _xenObjectCopy);
    page->setXenObjects(this->m_objectRef, this->m_objectType, this->m_objectDataBefore, this->m_objectDataCopy);

    // Connect populated signal to refresh tabs
    connect(page, &IEditPage::populated, [this]()
    {
        this->ui->verticalTabs->viewport()->update();
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
    // Shared save logic for both OK and Apply buttons
    
    // Step 1: Validate all pages
    // C# code: foreach (IEditPage editPage in verticalTabs.Items)
    //              if (!editPage.ValidToSave) { ... }
    for (IEditPage* page : this->m_pages)
    {
        if (!page->isValidToSave())
        {
            this->selectPage(page);
            page->showLocalValidationMessages();
            return false; // Abort save
        }
    }

    // Step 2: Check if any changes exist
    // C# code: if (!changedPageExists) { Close(); return; }
    bool hasChanges = false;
    for (IEditPage* page : this->m_pages)
    {
        if (page->hasChanged())
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

    if (actions.isEmpty())
    {
        // No complex actions, just simple property changes
        // Apply them and optionally close
        this->applySimpleChanges();
        
        // After successful save, reload data so Apply can be used again
        if (!closeOnSuccess)
        {
            this->loadObjectData();
            // Refresh all pages with new data
            for (IEditPage* page : this->m_pages)
            {
                page->setXenObjects(this->m_objectRef, this->m_objectType, 
                                   this->m_objectDataBefore, this->m_objectDataCopy);
            }
        }
        
        return true;
    }

    // Step 4: Create OperationProgressDialog to own the MultipleOperation
    // C# code:
    //   actions.Insert(index, new SaveChangesAction(_xenObjectCopy, true, _xenObjectBefore));
    //   _action = new MultipleAction(connection, title, desc, completedDesc, actions);
    //   using (var dialog = new ActionProgressDialog(_action, ProgressBarStyle.Marquee))
    //   {
    //       _action.RunAsync();
    //       dialog.ShowDialog(this);
    //   }

    // First apply simple changes (equivalent to SaveChangesAction)
    this->applySimpleChanges();

    // Create progress dialog - it will own the MultipleOperation and all sub-actions
    QString objName = this->m_objectDataBefore.value("name_label", this->m_objectRef).toString();
    
    // Now create the MultipleOperation with progressDialog as parent
    // This ensures proper ownership: dialog owns operation, operation owns sub-actions
    MultipleOperation* multiOp = new MultipleOperation(
        this->m_connection,
        tr("Update Properties - %1").arg(objName),
        tr("Updating properties..."),
        tr("Properties updated"),
        actions,
        true,  // suppressHistory
        false, // showSubOperationDetails
        false, // stopOnFirstException
        nullptr); // Progress dialog will own the operation

    OperationProgressDialog* progressDialog = new OperationProgressDialog(multiOp, this);
    progressDialog->setWindowTitle(tr("Update Properties - %1").arg(objName));
    
    // Re-parent the operation to the progress dialog for proper cleanup
    multiOp->setParent(progressDialog);

    // Connect completion handler
    bool saveSucceeded = false;
    connect(multiOp, &MultipleOperation::completed, this, [this, multiOp, progressDialog, closeOnSuccess, &saveSucceeded]()
    {
        if (multiOp->isCompleted() && !multiOp->hasError())
        {
            progressDialog->accept();
            saveSucceeded = true;
            
            if (closeOnSuccess)
            {
                // OK button - close the properties dialog
                QDialog::accept();
            }
            else
            {
                // Apply button - reload data and refresh pages
                this->loadObjectData();
                for (IEditPage* page : this->m_pages)
                {
                    page->setXenObjects(this->m_objectRef, this->m_objectType,
                                       this->m_objectDataBefore, this->m_objectDataCopy);
                }
            }
        }
        else
        {
            // Error is shown by OperationProgressDialog
            progressDialog->reject();
            // Don't close properties dialog on error - let user retry
        }
    });

    // Run the operation and show progress dialog modally
    multiOp->runAsync();
    progressDialog->exec(); // Blocks until operation completes
    
    // Dialog and operation are automatically cleaned up when dialog closes
    return saveSucceeded;
}

void VerticallyTabbedDialog::reject()
{
    // C# equivalent: btnCancel_Click()

    // Cleanup pages
    for (IEditPage* page : m_pages)
    {
        page->cleanup();
    }

    QDialog::reject();
}

void VerticallyTabbedDialog::closeEvent(QCloseEvent* event)
{
    // C# equivalent: OnFormClosing()

    // Cleanup pages
    for (IEditPage* page : this->m_pages)
    {
        page->cleanup();
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
        if (!page->hasChanged())
        {
            continue;
        }

        // Call saveSettings() - this updates the page's local m_objectDataCopy
        AsyncOperation* action = page->saveSettings();
        
        // Copy the modified data back from the page to the dialog's copy
        // This ensures applySimpleChanges() sees the modifications
        QVariantMap pageData = page->getModifiedObjectData();
        
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
            qDebug() << "VerticallyTabbedDialog: Collected action from page:" << page->text();
        }
    }

    return actions;
}

void VerticallyTabbedDialog::applySimpleChanges()
{
    // Apply simple property changes that pages made directly to objectDataCopy
    // C# equivalent: SaveChangesAction.Run() which calls xenObject.SaveChanges(Session, beforeObject)
    
    // Pages like GeneralEditPage modify objectDataCopy directly for simple fields:
    // - name_label
    // - name_description  
    // - other_config entries (like iscsi_iqn)
    //
    // This method detects those changes and makes synchronous XenAPI calls to apply them.
    // Complex operations (folder, tags, etc.) are handled by dedicated Actions.
    
    // Get API through MainWindow (dialog parent)
    MainWindow* mainWin = qobject_cast<MainWindow*>(this->parentWidget());
    if (!mainWin || !mainWin->xenLib())
    {
        qWarning() << "VerticallyTabbedDialog::applySimpleChanges: Cannot access XenLib";
        return;
    }

    XenSession* session = nullptr;
    if (mainWin->xenLib()->getConnection())
    {
        session = mainWin->xenLib()->getConnection()->getSession();
    }

    if (!session || !session->isLoggedIn())
    {
        qWarning() << "VerticallyTabbedDialog::applySimpleChanges: No session";
        return;
    }

    bool hasChanges = false;
    
    // 1. Check if name_label changed
    QString oldName = this->m_objectDataBefore.value("name_label").toString();
    QString newName = this->m_objectDataCopy.value("name_label").toString();
    if (oldName != newName && !newName.isEmpty())
    {
        qDebug() << "VerticallyTabbedDialog: Applying name_label change:" << oldName << "->" << newName;
        
        bool success = false;
        if (this->m_objectType == "vm")
        {
            try
            {
                XenAPI::VM::set_name_label(session, this->m_objectRef, newName);
                success = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set VM name_label:" << ex.what();
            }
        }
        else if (this->m_objectType == "host")
        {
            try
            {
                XenAPI::Host::set_name_label(session, this->m_objectRef, newName);
                success = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set Host name_label:" << ex.what();
            }
        }
        else if (this->m_objectType == "pool")
        {
            try
            {
                XenAPI::Pool::set_name_label(session, this->m_objectRef, newName);
                success = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set Pool name_label:" << ex.what();
            }
        }
        else if (this->m_objectType == "sr")
        {
            try
            {
                XenAPI::SR::set_name_label(session, this->m_objectRef, newName);
                success = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set SR name_label:" << ex.what();
            }
        }
        else if (this->m_objectType == "network")
        {
            try
            {
                XenAPI::Network::set_name_label(session, this->m_objectRef, newName);
                success = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set Network name_label:" << ex.what();
            }
        }
        
        if (success)
        {
            hasChanges = true;
        }
        else
        {
            qWarning() << "Failed to set name_label for" << this->m_objectType;
        }
    }
    
    // 2. Check if name_description changed
    QString oldDesc = this->m_objectDataBefore.value("name_description").toString();
    QString newDesc = this->m_objectDataCopy.value("name_description").toString();
    if (oldDesc != newDesc)
    {
        qDebug() << "VerticallyTabbedDialog: Applying name_description change";
        
        bool success = false;
        if (this->m_objectType == "vm")
        {
            try
            {
                XenAPI::VM::set_name_description(session, this->m_objectRef, newDesc);
                success = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set VM name_description:" << ex.what();
            }
        }
        else if (this->m_objectType == "host")
        {
            try
            {
                XenAPI::Host::set_name_description(session, this->m_objectRef, newDesc);
                success = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set Host name_description:" << ex.what();
            }
        }
        else if (this->m_objectType == "pool")
        {
            try
            {
                XenAPI::Pool::set_name_description(session, this->m_objectRef, newDesc);
                success = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set Pool name_description:" << ex.what();
            }
        }
        else if (this->m_objectType == "sr")
        {
            try
            {
                XenAPI::SR::set_name_description(session, this->m_objectRef, newDesc);
                success = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set SR name_description:" << ex.what();
            }
        }
        else if (this->m_objectType == "network")
        {
            try
            {
                XenAPI::Network::set_name_description(session, this->m_objectRef, newDesc);
                success = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set Network name_description:" << ex.what();
            }
        }
        
        if (success)
        {
            hasChanges = true;
        }
        else
        {
            qWarning() << "Failed to set name_description for" << this->m_objectType;
        }
    }
    
    // 3. Check if other_config changed (for things like iscsi_iqn)
    QVariantMap oldOtherConfig = this->m_objectDataBefore.value("other_config").toMap();
    QVariantMap newOtherConfig = this->m_objectDataCopy.value("other_config").toMap();

    if (this->m_objectType == "vm")
    {
        if (oldOtherConfig != newOtherConfig)
        {
            qDebug() << "VerticallyTabbedDialog: Applying VM other_config changes";
            try
            {
                XenAPI::VM::set_other_config(session, this->m_objectRef, newOtherConfig);
                hasChanges = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set VM other_config:" << ex.what();
            }
        }

        QVariantMap oldVcpusParams = this->m_objectDataBefore.value("VCPUs_params").toMap();
        QVariantMap newVcpusParams = this->m_objectDataCopy.value("VCPUs_params").toMap();
        if (oldVcpusParams != newVcpusParams)
        {
            qDebug() << "VerticallyTabbedDialog: Applying VM VCPUs_params changes";
            try
            {
                XenAPI::VM::set_VCPUs_params(session, this->m_objectRef, newVcpusParams);
                hasChanges = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set VM VCPUs_params:" << ex.what();
            }
        }

        QVariantMap oldPlatform = this->m_objectDataBefore.value("platform").toMap();
        QVariantMap newPlatform = this->m_objectDataCopy.value("platform").toMap();
        if (oldPlatform != newPlatform)
        {
            qDebug() << "VerticallyTabbedDialog: Applying VM platform changes";
            try
            {
                XenAPI::VM::set_platform(session, this->m_objectRef, newPlatform);
                hasChanges = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set VM platform:" << ex.what();
            }
        }
    }

    // 4. Check if HVM_shadow_multiplier changed (VM only)
    if (this->m_objectType == "vm" && this->m_objectDataCopy.contains("HVM_shadow_multiplier"))
    {
        double oldMultiplier = this->m_objectDataBefore.value("HVM_shadow_multiplier").toDouble();
        double newMultiplier = this->m_objectDataCopy.value("HVM_shadow_multiplier").toDouble();
        if (qAbs(oldMultiplier - newMultiplier) > 0.0001)
        {
            qDebug() << "VerticallyTabbedDialog: Applying HVM_shadow_multiplier change:"
                     << oldMultiplier << "->" << newMultiplier;
            try
            {
                XenAPI::VM::set_HVM_shadow_multiplier(session, this->m_objectRef, newMultiplier);
                hasChanges = true;
            }
            catch (const std::exception& ex)
            {
                qWarning() << "Failed to set HVM_shadow_multiplier:" << ex.what();
            }
        }
    }
    else
    {
        // Find keys that changed - store keys in QList first to avoid dangling iterators
        QList<QString> oldKeys = oldOtherConfig.keys();
        QList<QString> newKeys = newOtherConfig.keys();
        QSet<QString> allKeys(oldKeys.begin(), oldKeys.end());
        allKeys.unite(QSet<QString>(newKeys.begin(), newKeys.end()));

        for (const QString& key : allKeys)
        {
            QString oldValue = oldOtherConfig.value(key).toString();
            QString newValue = newOtherConfig.value(key).toString();

            if (oldValue != newValue)
            {
                qDebug() << "VerticallyTabbedDialog: Applying other_config change:" << key
                         << ":" << oldValue << "->" << newValue;

                bool success = false;

                if (newValue.isEmpty() && oldOtherConfig.contains(key))
                {
                    // Remove the key - would need remove_from_other_config API
                    // For now, just skip removal (set to empty string instead)
                    qWarning() << "Skipping removal of other_config key" << key
                               << "(remove_from_other_config not implemented)";
                }
                else
                {
                    // Add or update the key
                    if (this->m_objectType == "host")
                    {
                        QVariantMap hostData = this->m_objectDataCopy;
                        QVariantMap otherConfig = hostData.value("other_config").toMap();
                        if (newValue.isEmpty())
                            otherConfig.remove(key);
                        else
                            otherConfig[key] = newValue;

                        try
                        {
                            XenAPI::Host::set_other_config(session, this->m_objectRef, otherConfig);
                            success = true;
                        }
                        catch (const std::exception& ex)
                        {
                            qWarning() << "Failed to set Host other_config:" << ex.what();
                        }
                    }
                    // Pool, SR, Network also have other_config but we haven't implemented helpers yet
                    else
                    {
                        qWarning() << "other_config updates not implemented for" << this->m_objectType;
                    }

                    if (success)
                    {
                        hasChanges = true;
                    }
                }
            }
        }
    }
    
    if (hasChanges)
    {
        qDebug() << "VerticallyTabbedDialog::applySimpleChanges: Changes applied successfully";
    }
    else
    {
        qDebug() << "VerticallyTabbedDialog::applySimpleChanges: No simple changes detected";
    }
}


void VerticallyTabbedDialog::onVerticalTabsCurrentChanged(int index)
{
    if (index < 0 || index >= this->m_pages.size())
        return;

    IEditPage* page = this->m_pages[index];

    // Update header to match selected page
    this->ui->TabImage->setPixmap(page->image().pixmap(32, 32));
    this->ui->TabTitle->setText(page->text());

    // Show selected page in content panel
    this->ui->ContentPanel->setCurrentWidget(page);

    // Hide validation messages on other pages
    for (IEditPage* otherPage : this->m_pages)
    {
        if (otherPage != page)
        {
            otherPage->hideLocalValidationMessages();
        }
    }
}

// Collect actions from all changed pages
