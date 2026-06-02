#include <QtTest>
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/xenobjecttype.h"
#include "xenlib/ovf/ovfpackage.h"
#include "test_helpers.h"
#include <QTemporaryFile>
#include <QTextStream>

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

    // ── OvfPackage::XenConfigBySystem — Qt OvfWriter format ──────────────────
    // Format: <xen:Data><xen:Name>key</xen:Name><xen:Value>val</xen:Value></xen:Data>
    // Namespace: http://www.citrix.com/xencenter/2009/xen

    void ovfXenConfig_qtFormat_parsesHvmBootPolicy()
    {
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="myvm">
    <Name>myvm</Name>
    <VirtualHardwareSection>
      <xen:Data ovf:required="false">
        <xen:Name>HVM_boot_policy</xen:Name>
        <xen:Value>BIOS order</xen:Value>
      </xen:Data>
      <xen:Data ovf:required="false">
        <xen:Name>platform</xen:Name>
        <xen:Value>nx=true;acpi=1</xen:Value>
      </xen:Data>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("myvm"));
        QCOMPARE(cfg["myvm"]["HVM_boot_policy"], QString("BIOS order"));
        QCOMPARE(cfg["myvm"]["platform"], QString("nx=true;acpi=1"));
    }

    void ovfXenConfig_qtFormat_pvBootloader()
    {
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="pvvm">
    <Name>pvvm</Name>
    <VirtualHardwareSection>
      <xen:Data ovf:required="false">
        <xen:Name>PV_bootloader</xen:Name>
        <xen:Value>pygrub</xen:Value>
      </xen:Data>
      <xen:Data ovf:required="false">
        <xen:Name>PV_args</xen:Name>
        <xen:Value>quiet splash</xen:Value>
      </xen:Data>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("pvvm"));
        QCOMPARE(cfg["pvvm"]["PV_bootloader"], QString("pygrub"));
        QCOMPARE(cfg["pvvm"]["PV_args"], QString("quiet splash"));
        QVERIFY(!cfg["pvvm"].contains("HVM_boot_policy"));
    }

    // ── OvfPackage::XenConfigBySystem — C# XenAdmin format ───────────────────
    // Format: <citrix:VirtualSystemOtherConfigurationData Name="key"><Value>v</Value>...
    // Namespace: http://schemas.citrix.com/ovf/envelope/1

    void ovfXenConfig_csFormat_parsesBootPolicy()
    {
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:citrix="http://schemas.citrix.com/ovf/envelope/1"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="csvm">
    <Name>csvm</Name>
    <VirtualHardwareSection>
      <citrix:VirtualSystemOtherConfigurationData Name="HVM_boot_policy">
        <Value>BIOS order</Value>
      </citrix:VirtualSystemOtherConfigurationData>
      <citrix:VirtualSystemOtherConfigurationData Name="HVM_boot_params">
        <Value>order=dc</Value>
      </citrix:VirtualSystemOtherConfigurationData>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("csvm"));
        QCOMPARE(cfg["csvm"]["HVM_boot_policy"], QString("BIOS order"));
        QCOMPARE(cfg["csvm"]["HVM_boot_params"], QString("order=dc"));
    }

    void ovfXenConfig_csFormat_recommendationsKeyExcluded()
    {
        // The "recommendations" entry must be skipped by parseXenConfig (handled by parseRecommendations).
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:citrix="http://schemas.citrix.com/ovf/envelope/1"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="vm1">
    <Name>vm1</Name>
    <VirtualHardwareSection>
      <citrix:VirtualSystemOtherConfigurationData Name="recommendations">
        <Value><restrictions><restriction field="vcpus-max" value="8"/></restrictions></Value>
      </citrix:VirtualSystemOtherConfigurationData>
      <citrix:VirtualSystemOtherConfigurationData Name="HVM_boot_policy">
        <Value>BIOS order</Value>
      </citrix:VirtualSystemOtherConfigurationData>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("vm1"));
        QVERIFY(!cfg["vm1"].contains("recommendations"));
        QCOMPARE(cfg["vm1"]["HVM_boot_policy"], QString("BIOS order"));
    }

    // ── OvfPackage::XenConfigBySystem — no xen config section ────────────────

    void ovfXenConfig_noXenSection_returnsEmptyMap()
    {
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="plainvm">
    <Name>plainvm</Name>
    <VirtualHardwareSection/>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        // System is present (from VirtualSystem element) but config map is empty
        QVERIFY(cfg.contains("plainvm"));
        QVERIFY(cfg["plainvm"].isEmpty());
    }

    // ── OvfPackage::XenConfigBySystem — multi-VM package ─────────────────────

    void ovfXenConfig_multipleVms_configsAreSeparate()
    {
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystemCollection ovf:id="myvapp">
    <VirtualSystem ovf:id="vm-hvm">
      <Name>vm-hvm</Name>
      <VirtualHardwareSection>
        <xen:Data ovf:required="false">
          <xen:Name>HVM_boot_policy</xen:Name>
          <xen:Value>BIOS order</xen:Value>
        </xen:Data>
      </VirtualHardwareSection>
    </VirtualSystem>
    <VirtualSystem ovf:id="vm-pv">
      <Name>vm-pv</Name>
      <VirtualHardwareSection>
        <xen:Data ovf:required="false">
          <xen:Name>PV_bootloader</xen:Name>
          <xen:Value>pygrub</xen:Value>
        </xen:Data>
      </VirtualHardwareSection>
    </VirtualSystem>
  </VirtualSystemCollection>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("vm-hvm"));
        QVERIFY(cfg.contains("vm-pv"));
        QCOMPARE(cfg["vm-hvm"]["HVM_boot_policy"], QString("BIOS order"));
        QVERIFY(!cfg["vm-hvm"].contains("PV_bootloader"));
        QCOMPARE(cfg["vm-pv"]["PV_bootloader"], QString("pygrub"));
        QVERIFY(!cfg["vm-pv"].contains("HVM_boot_policy"));
    }

    void ovfXenConfig_emptyValueAllowed()
    {
        // A key with an empty value should still be stored (it may override a default).
        const QString xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Envelope xmlns="http://schemas.dmtf.org/ovf/envelope/1"
          xmlns:xen="http://www.citrix.com/xencenter/2009/xen"
          xmlns:ovf="http://schemas.dmtf.org/ovf/envelope/1">
  <References/>
  <VirtualSystem ovf:id="uefivm">
    <Name>uefivm</Name>
    <VirtualHardwareSection>
      <xen:Data ovf:required="false">
        <xen:Name>HVM_boot_policy</xen:Name>
        <xen:Value></xen:Value>
      </xen:Data>
    </VirtualHardwareSection>
  </VirtualSystem>
</Envelope>)";

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("uefivm"));
        QVERIFY(cfg["uefivm"].contains("HVM_boot_policy"));
        QCOMPARE(cfg["uefivm"]["HVM_boot_policy"], QString(""));
    }
};

QTEST_APPLESS_MAIN(XenLibTests)
#include "test_main.moc"
