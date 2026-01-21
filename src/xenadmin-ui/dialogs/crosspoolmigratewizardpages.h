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

#ifndef CROSSPOOLMIGRATEWIZARDPAGES_H
#define CROSSPOOLMIGRATEWIZARDPAGES_H

#include <QWizardPage>

class CrossPoolMigrateWizard;
class QCheckBox;

class DestinationWizardPage : public QWizardPage
{
    Q_OBJECT

    public:
        explicit DestinationWizardPage(QWidget* parent = nullptr);
        void setWizard(CrossPoolMigrateWizard* wizard);
        int nextId() const override;

    private:
        CrossPoolMigrateWizard* wizard_ = nullptr;
};

class StorageWizardPage : public QWizardPage
{
    Q_OBJECT

    public:
        explicit StorageWizardPage(QWidget* parent = nullptr);
        void setWizard(CrossPoolMigrateWizard* wizard);
        int nextId() const override;

    private:
        CrossPoolMigrateWizard* wizard_ = nullptr;
};

class NetworkWizardPage : public QWizardPage
{
    Q_OBJECT

    public:
        explicit NetworkWizardPage(QWidget* parent = nullptr);
        void setWizard(CrossPoolMigrateWizard* wizard);
        int nextId() const override;

    private:
        CrossPoolMigrateWizard* wizard_ = nullptr;
};

class TransferWizardPage : public QWizardPage
{
    Q_OBJECT

    public:
        explicit TransferWizardPage(QWidget* parent = nullptr);
        void setWizard(CrossPoolMigrateWizard* wizard);
        int nextId() const override;

    private:
        CrossPoolMigrateWizard* wizard_ = nullptr;
};

class RbacWizardPage : public QWizardPage
{
    Q_OBJECT

    public:
        explicit RbacWizardPage(QWidget* parent = nullptr);
        void setWizard(CrossPoolMigrateWizard* wizard);
        void setConfirmation(QCheckBox* box);
        int nextId() const override;
        bool isComplete() const override;

    private:
        void onConfirmationToggled(bool);

        QCheckBox* confirmBox_ = nullptr;
        CrossPoolMigrateWizard* wizard_ = nullptr;
};

#endif // CROSSPOOLMIGRATEWIZARDPAGES_H
