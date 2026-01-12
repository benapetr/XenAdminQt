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

#ifndef CDCHANGER_H
#define CDCHANGER_H

#include "isodropdownbox.h"
#include <QSharedPointer>

class VM;
class VBD;
class XenCache;

/**
 * @brief CD/DVD drive changer widget that extends IsoDropDownBox
 * 
 * This widget provides CD/DVD drive selection and ISO mounting functionality.
 * It monitors VBD (Virtual Block Device) state changes and automatically updates
 * the selected ISO when the underlying device changes.
 * 
 * Ported from C# XenAdmin.Controls.CDChanger
 */
class CDChanger : public IsoDropDownBox
{
    Q_OBJECT

    public:
        explicit CDChanger(QWidget* parent = nullptr);
        ~CDChanger() override;

        /**
         * @brief Get the current VBD (CD/DVD drive) being managed
         */
        QSharedPointer<VBD> GetDrive() const { return this->cdrom_; }
        
        /**
         * @brief Set the VBD (CD/DVD drive) to manage
         * @param vbd The VBD object representing the CD/DVD drive
         */
        void SetDrive(QSharedPointer<VBD> vbd);

        /**
         * @brief Get the VM this CD changer is associated with
         */
        QSharedPointer<VM> GetVM() const { return this->vm_; }

        /**
         * @brief Set the VM this CD changer is associated with
         * @param vm The VM object
         */
        void SetVM(QSharedPointer<VM> vm);

        /**
         * @brief Change the CD/DVD in the drive
         * @param vdiRef The VDI reference of the ISO to mount (empty string to eject)
         */
        void ChangeCD(const QString& vdiRef);

        /**
         * @brief Deregister all event listeners
         */
        void DeregisterEvents();

    protected:
        /**
         * @brief Handle selection change in the combo box
         * @param index The new selected index
         */
        void onCurrentIndexChanged(int index);

    private:
        void updateSelectedCd();
        void connectVbdSignals();
        void disconnectVbdSignals();
        void onVbdPropertyChanged();

        QSharedPointer<VBD> cdrom_;
        QSharedPointer<VM> vm_;
        bool changing_;
};

#endif // CDCHANGER_H
