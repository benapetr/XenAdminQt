#include <QtTest>
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/xenobjecttype.h"
#include "test_helpers.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers: build minimal cache entries to exercise VM methods in isolation
// ─────────────────────────────────────────────────────────────────────────────

static QSharedPointer<VM> makeVm(const QString& ref, const QVariantMap& data)
{
    XenCache* cache = XenCache::GetDummy();
    cache->Update(XenObjectType::VM, ref, data);
    return QSharedPointer<VM>(new VM(nullptr, ref));
}

static QVariantMap normalVm(const QString& powerState = "Halted",
                             const QVariantList& allowedOps = QVariantList{"export"})
{
    QVariantMap d;
    d["power_state"]         = powerState;
    d["is_a_template"]       = false;
    d["is_control_domain"]   = false;
    d["allowed_operations"]  = allowedOps;
    d["name_label"]          = "Test VM";
    return d;
}

class XenLibTests : public QObject
{
    Q_OBJECT

private slots:
    // ── cache round-trip ──────────────────────────────────────────────────────
    void loadCacheFromResource_populatesVmData()
    {
        XenCache* cache = LoadCacheFromEventJson(":/testdata/xenapi.json");
        QVERIFY(cache != nullptr);

        const QStringList vmRefs = cache->GetAllRefs(XenObjectType::VM);
        QVERIFY(!vmRefs.isEmpty());

        QSharedPointer<VM> vm = MakeObjectFromDummyCache<VM>(vmRefs.first());
        QVERIFY(!vm.isNull());
        QVERIFY(!vm->GetName().isEmpty());
    }

    // ── IsXvaExportable ───────────────────────────────────────────────────────

    void isXvaExportable_haltedWithExportOp_returnsTrue()
    {
        auto vm = makeVm("ref-1", normalVm("Halted"));
        QVERIFY(vm->IsXvaExportable());
    }

    void isXvaExportable_runningWithExportOp_returnsTrue()
    {
        // XVA export is allowed from any power state as long as "export" is present.
        auto vm = makeVm("ref-2", normalVm("Running"));
        QVERIFY(vm->IsXvaExportable());
    }

    void isXvaExportable_noExportOp_returnsFalse()
    {
        auto vm = makeVm("ref-3", normalVm("Halted", QVariantList{}));
        QVERIFY(!vm->IsXvaExportable());
    }

    void isXvaExportable_template_returnsFalse()
    {
        QVariantMap d = normalVm("Halted");
        d["is_a_template"] = true;
        auto vm = makeVm("ref-4", d);
        QVERIFY(!vm->IsXvaExportable());
    }

    void isXvaExportable_controlDomain_returnsFalse()
    {
        QVariantMap d = normalVm("Running");
        d["is_control_domain"] = true;
        auto vm = makeVm("ref-5", d);
        QVERIFY(!vm->IsXvaExportable());
    }

    // snapshot: is_a_template=true && power_state=Suspended in XenAPI convention;
    // we store is_a_template=true for snapshot records — it blocks export.
    void isXvaExportable_snapshot_returnsFalse()
    {
        QVariantMap d = normalVm("Suspended");
        d["is_a_snapshot"] = true;
        d["is_a_template"] = false;
        // allowed_operations for a snapshot won't include "export", but we also
        // check is_a_snapshot via IsSnapshot(). Either path blocks export.
        d["allowed_operations"] = QVariantList{"export"};
        auto vm = makeVm("ref-6", d);
        // Snapshots are filtered by IsSnapshot() → IsXvaExportable() returns false.
        QVERIFY(!vm->IsXvaExportable());
    }

    // ── IsOvfExportable ───────────────────────────────────────────────────────

    void isOvfExportable_haltedWithExportOp_returnsTrue()
    {
        auto vm = makeVm("ref-10", normalVm("Halted"));
        QVERIFY(vm->IsOvfExportable());
    }

    void isOvfExportable_suspendedWithExportOp_returnsTrue()
    {
        auto vm = makeVm("ref-11", normalVm("Suspended"));
        QVERIFY(vm->IsOvfExportable());
    }

    void isOvfExportable_running_returnsFalse()
    {
        // Running VMs cannot be captured into OVF (C# IsVmExportable filter).
        auto vm = makeVm("ref-12", normalVm("Running"));
        QVERIFY(!vm->IsOvfExportable());
    }

    void isOvfExportable_paused_returnsFalse()
    {
        auto vm = makeVm("ref-13", normalVm("Paused"));
        QVERIFY(!vm->IsOvfExportable());
    }

    void isOvfExportable_noExportOp_returnsFalse()
    {
        auto vm = makeVm("ref-14", normalVm("Halted", QVariantList{}));
        QVERIFY(!vm->IsOvfExportable());
    }

    void isOvfExportable_template_returnsFalse()
    {
        QVariantMap d = normalVm("Halted");
        d["is_a_template"] = true;
        auto vm = makeVm("ref-15", d);
        QVERIFY(!vm->IsOvfExportable());
    }

    void isOvfExportable_controlDomain_returnsFalse()
    {
        QVariantMap d = normalVm("Halted");
        d["is_control_domain"] = true;
        auto vm = makeVm("ref-16", d);
        QVERIFY(!vm->IsOvfExportable());
    }

    // ── IsRealVM ─────────────────────────────────────────────────────────────

    void isRealVM_normalVm_returnsTrue()
    {
        auto vm = makeVm("ref-20", normalVm());
        QVERIFY(vm->IsRealVM());
    }

    void isRealVM_template_returnsFalse()
    {
        QVariantMap d = normalVm();
        d["is_a_template"] = true;
        auto vm = makeVm("ref-21", d);
        QVERIFY(!vm->IsRealVM());
    }

    void isRealVM_controlDomain_returnsFalse()
    {
        QVariantMap d = normalVm();
        d["is_control_domain"] = true;
        auto vm = makeVm("ref-22", d);
        QVERIFY(!vm->IsRealVM());
    }
};

QTEST_APPLESS_MAIN(XenLibTests)
#include "test_main.moc"
