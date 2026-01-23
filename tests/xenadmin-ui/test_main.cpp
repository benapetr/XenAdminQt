#include <QtTest>
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"

class XenAdminUiTests : public QObject
{
    Q_OBJECT

private slots:
    void defaultTemplateUsesBooleanValue()
    {
        XenCache* cache = XenCache::GetDummy();
        const QString ref = "OpaqueRef:test-default-template";
        QSharedPointer<VM> vm(new VM(nullptr, ref));

        QVariantMap dataFalse;
        dataFalse.insert("other_config", QVariantMap{{"default_template", "false"}});
        cache->Update("vm", ref, dataFalse);
        QCOMPARE(vm->DefaultTemplate(), false);

        QVariantMap dataTrue;
        dataTrue.insert("other_config", QVariantMap{{"default_template", "true"}});
        cache->Update("vm", ref, dataTrue);
        QCOMPARE(vm->DefaultTemplate(), true);

        QVariantMap dataMissing;
        dataMissing.insert("other_config", QVariantMap{});
        cache->Update("vm", ref, dataMissing);
        QCOMPARE(vm->DefaultTemplate(), false);
    }
};

QTEST_APPLESS_MAIN(XenAdminUiTests)
#include "test_main.moc"
