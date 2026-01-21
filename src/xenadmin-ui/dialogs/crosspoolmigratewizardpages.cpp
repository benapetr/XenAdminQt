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

#include "crosspoolmigratewizardpages.h"
#include "crosspoolmigratewizard.h"
#include <QCheckBox>

DestinationWizardPage::DestinationWizardPage(QWidget* parent)
    : QWizardPage(parent)
{
}

void DestinationWizardPage::setWizard(CrossPoolMigrateWizard* wizard)
{
    wizard_ = wizard;
}

int DestinationWizardPage::nextId() const
{
    if (wizard_ && wizard_->requiresRbacWarning())
        return CrossPoolMigrateWizard::Page_RbacWarning;
    return CrossPoolMigrateWizard::Page_Storage;
}

StorageWizardPage::StorageWizardPage(QWidget* parent)
    : QWizardPage(parent)
{
}

void StorageWizardPage::setWizard(CrossPoolMigrateWizard* wizard)
{
    wizard_ = wizard;
}

int StorageWizardPage::nextId() const
{
    if (!wizard_)
        return CrossPoolMigrateWizard::Page_Finish;
    if (wizard_->shouldShowNetworkPage())
        return CrossPoolMigrateWizard::Page_Network;
    if (wizard_->shouldShowTransferNetworkPage())
        return CrossPoolMigrateWizard::Page_TransferNetwork;
    return CrossPoolMigrateWizard::Page_Finish;
}

NetworkWizardPage::NetworkWizardPage(QWidget* parent)
    : QWizardPage(parent)
{
}

void NetworkWizardPage::setWizard(CrossPoolMigrateWizard* wizard)
{
    wizard_ = wizard;
}

int NetworkWizardPage::nextId() const
{
    if (wizard_ && wizard_->shouldShowTransferNetworkPage())
        return CrossPoolMigrateWizard::Page_TransferNetwork;
    return CrossPoolMigrateWizard::Page_Finish;
}

TransferWizardPage::TransferWizardPage(QWidget* parent)
    : QWizardPage(parent)
{
}

void TransferWizardPage::setWizard(CrossPoolMigrateWizard* wizard)
{
    wizard_ = wizard;
}

int TransferWizardPage::nextId() const
{
    return CrossPoolMigrateWizard::Page_Finish;
}

RbacWizardPage::RbacWizardPage(QWidget* parent)
    : QWizardPage(parent)
{
    setFinalPage(false);
}

void RbacWizardPage::setWizard(CrossPoolMigrateWizard* wizard)
{
    wizard_ = wizard;
}

void RbacWizardPage::setConfirmation(QCheckBox* box)
{
    confirmBox_ = box;
    if (confirmBox_)
        connect(confirmBox_, &QCheckBox::toggled, this, &RbacWizardPage::onConfirmationToggled);
}

int RbacWizardPage::nextId() const
{
    if (wizard_ && wizard_->isIntraPoolCopySelected())
        return CrossPoolMigrateWizard::Page_IntraPoolCopy;
    return CrossPoolMigrateWizard::Page_Storage;
}

bool RbacWizardPage::isComplete() const
{
    return confirmBox_ ? confirmBox_->isChecked() : true;
}

void RbacWizardPage::onConfirmationToggled(bool)
{
    emit completeChanged();
}
