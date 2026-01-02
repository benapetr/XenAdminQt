#ifndef VUSB_H
#define VUSB_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>

/*!
 * \brief Virtual USB device wrapper class
 * 
 * Represents a virtual USB device attached to a VM.
 * Provides access to USB group, attachment status, and operations.
 * First published in XenServer 7.3.
 */
class VUSB : public XenObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString uuid READ Uuid)
    Q_PROPERTY(QString VM READ VMRef)
    Q_PROPERTY(QString usbGroup READ USBGroupRef)
    Q_PROPERTY(bool currentlyAttached READ CurrentlyAttached)
    
public:
    explicit VUSB(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
    
    QString GetObjectType() const override;
    
    // Basic properties
    QString Uuid() const;
    QStringList AllowedOperations() const;
    QVariantMap CurrentOperations() const;
    QString VMRef() const;
    QString USBGroupRef() const;
    QVariantMap OtherConfig() const;
    bool CurrentlyAttached() const;
    
    // Helper methods
    bool IsAttached() const;
};

#endif // VUSB_H
