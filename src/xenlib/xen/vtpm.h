#ifndef VTPM_H
#define VTPM_H

#include "xenobject.h"
#include <QString>
#include <QStringList>
#include <QVariantMap>

/*!
 * \brief Virtual TPM (Trusted Platform Module) device wrapper class
 * 
 * Represents a virtual TPM device attached to a VM.
 * Provides access to TPM configuration, persistence backend, and security properties.
 * Experimental feature first published in XenServer 22.26.0.
 */
class VTPM : public XenObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString uuid READ Uuid)
    Q_PROPERTY(QString VM READ VMRef)
    Q_PROPERTY(QString backend READ BackendRef)
    Q_PROPERTY(QString persistenceBackend READ PersistenceBackend)
    Q_PROPERTY(bool isUnique READ IsUnique)
    Q_PROPERTY(bool isProtected READ IsProtected)
    
public:
    explicit VTPM(XenConnection* connection, const QString& opaqueRef, QObject* parent = nullptr);
    
    QString GetObjectType() const override;
    
    // Basic properties
    QString Uuid() const;
    QStringList AllowedOperations() const;
    QVariantMap CurrentOperations() const;
    QString VMRef() const;
    QString BackendRef() const;
    QString PersistenceBackend() const;
    bool IsUnique() const;
    bool IsProtected() const;
    
    // Helper methods
    bool IsSecure() const;
};

#endif // VTPM_H
