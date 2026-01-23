#include <QtTest>
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "test_helpers.h"

class XenLibTests : public QObject
{
    Q_OBJECT

private slots:
    void loadCacheFromResource_populatesVmData()
    {
        XenCache* cache = LoadCacheFromEventJson(":/testdata/xenapi.json");
        QVERIFY(cache != nullptr);

        const QStringList vmRefs = cache->GetAllRefs("vm");
        QVERIFY(!vmRefs.isEmpty());

        QSharedPointer<VM> vm = MakeObjectFromDummyCache<VM>(vmRefs.first());
        QVERIFY(!vm.isNull());
        QVERIFY(!vm->GetName().isEmpty());
    }
};

QTEST_APPLESS_MAIN(XenLibTests)
#include "test_main.moc"
