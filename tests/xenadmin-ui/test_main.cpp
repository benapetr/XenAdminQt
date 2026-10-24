#include <QtTest>
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/network/connection.h"
#include "iconmanager.h"

class XenAdminUiTests : public QObject
{
    Q_OBJECT

private slots:
    void cachedFolderIconDoesNotRecurse()
    {
        XenConnection connection;
        const QString ref = "/test-folder";
        connection.GetCache()->Update(XenObjectType::Folder, ref,
                                      QVariantMap{{"name_label", "test-folder"}});
        const auto folder = connection.GetCache()->ResolveObject(XenObjectType::Folder, ref);
        QVERIFY(folder);

        IconManager& icons = IconManager::instance();
        QVERIFY(!icons.GetIconForObject(folder).isNull());
        QVERIFY(!icons.GetIconForObject(&connection, XenObjectType::Folder, ref).isNull());
        QVERIFY(!icons.GetIconForObject(nullptr, XenObjectType::Folder, QString()).isNull());
    }

    void cachedObjectWithoutIconDoesNotRecurse()
    {
        XenConnection connection;
        const QString ref = "OpaqueRef:test-vif";
        connection.GetCache()->Update(XenObjectType::VIF, ref, QVariantMap{{"device", "0"}});
        const auto vif = connection.GetCache()->ResolveObject(XenObjectType::VIF, ref);
        QVERIFY(vif);

        IconManager& icons = IconManager::instance();
        QVERIFY(icons.GetIconForObject(vif).isNull());
        QVERIFY(icons.GetIconForObject(&connection, XenObjectType::VIF, ref).isNull());
    }

    void defaultTemplateUsesBooleanValue()
    {
        XenCache* cache = XenCache::GetDummy();
        const QString ref = "OpaqueRef:test-default-template";
        QSharedPointer<VM> vm(new VM(nullptr, ref));

        QVariantMap dataFalse;
        dataFalse.insert("other_config", QVariantMap{{"default_template", "false"}});
        cache->Update(XenObjectType::VM, ref, dataFalse);
        QCOMPARE(vm->IsDefaultTemplate(), false);

        QVariantMap dataTrue;
        dataTrue.insert("other_config", QVariantMap{{"default_template", "true"}});
        cache->Update(XenObjectType::VM, ref, dataTrue);
        QCOMPARE(vm->IsDefaultTemplate(), true);

        QVariantMap dataMissing;
        dataMissing.insert("other_config", QVariantMap{});
        cache->Update(XenObjectType::VM, ref, dataMissing);
        QCOMPARE(vm->IsDefaultTemplate(), false);
    }
};

QTEST_MAIN(XenAdminUiTests)
#include "test_main.moc"
