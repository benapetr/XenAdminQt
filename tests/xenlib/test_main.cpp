#include <QtTest>
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/xenobjecttype.h"
#include "xenlib/ovf/ovfpackage.h"
#include "test_helpers.h"
#include "test_ovfdata.h"
#include "test_transportserver.h"
#include "xenlib/xen/network/connectionworker.h"
#include "xenlib/xen/jsonrpcclient.h"
#include "xenlib/xen/failure.h"
#include "xenlib/xen/session.h"
#include "xenlib/xen/actions/vm/vmstartabstractaction.h"
#include "xenlib/xen/network/connection.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/xen/network/heartbeat.h"
#include "xenlib/xen/actions/vdi/movevirtualdiskaction.h"
#include "xenlib/xen/actions/vdi/creatediskaction.h"
#include "xenlib/xen/actions/vbd/vbdcreateandplugaction.h"
#include "xenlib/xen/actions/vdi/migratevirtualdiskaction.h"
#include "xenlib/xen/actions/vm/vmrebootaction.h"
#include "xenlib/xen/actions/vm/vmshutdownaction.h"
#include "xenlib/xensearch/group.h"
#include "xenlib/xen/network/certificatemanager.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/friendlynames.h"
#include "xenlib/xen/message.h"
#include "xenlib/alerts/messagealert.h"
#include "xenlib/alerts/failedloginattemptalert.h"
#include "xenlib/alerts/leafcoalescealert.h"
#include <QTemporaryFile>
#include <QScopeGuard>
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

class HaTestStartAction : public VMStartAbstractAction
{
public:
    HaTestStartAction(QSharedPointer<VM> vm, WarningDialogHAInvalidConfig warning, bool isStart)
        : VMStartAbstractAction(vm, "Test HA start", warning, nullptr), m_isStart(isStart) {}
    bool IsStart() const override { return this->m_isStart; }
    VMStartAbstractAction* Clone() override { return nullptr; }
    bool performed = false;
protected:
    void run() override { this->startOrResumeVmWithHa(0, 100); }
    void doAction(int, int) override { this->performed = true; }
private:
    bool m_isStart;
};

class XenLibTests : public QObject
{
    Q_OBJECT

private slots:

    void friendlyNamesFormat()
    {
        QCOMPARE(FriendlyNames::Format("Server '{0}' has {1} of {2}", {"h1", "1", "2"}), QString("Server 'h1' has 1 of 2"));
        // Single pass: placeholders inside arguments are not substituted again
        QCOMPARE(FriendlyNames::Format("{0} and {1}", {"{1}", "x"}), QString("{1} and x"));
        // Missing arguments leave the placeholder
        QCOMPARE(FriendlyNames::Format("{0} {1}", {"a"}), QString("a {1}"));
    }

    void messageFriendlyLookups()
    {
        QCOMPARE(Message::FriendlyName("HA_HOST_FAILED"), QString("Server failed"));
        QCOMPARE(Message::FriendlyBody("HA_HOST_FAILED"), QString("Server '{0}' has failed."));
        QCOMPARE(Message::FriendlyAction("PBD_PLUG_FAILED_ON_SERVER_START"), QString("Repair Storage"));
        QVERIFY(Message::FriendlyName("NOT_A_REAL_MESSAGE").isEmpty());
    }

    void failedLoginAttemptAlert()
    {
        XenConnection connection;
        connection.GetCache()->Update(XenObjectType::Pool, "OpaqueRef:pool", QVariantMap{{"uuid", "pool-uuid"}, {"name_label", "MyPool"}});

        auto make = [&](const QString& body)
        {
            return QSharedPointer<XenLib::Alert>(XenLib::MessageAlert::ParseMessage(&connection, QVariantMap{
                {"ref", "OpaqueRef:msg"}, {"name", "FAILED_LOGIN_ATTEMPTS"}, {"cls", "Pool"},
                {"obj_uuid", "pool-uuid"}, {"body", body}, {"priority", 3}, {"timestamp", 0}}));
        };

        auto full = make("<body><total>12</total><unknown>2</unknown>"
                         "<known><number>10</number><date>20260101T10:00:00Z</date><ip>203.0.113.5</ip>"
                         "<username>root</username></known></body>");
        QVERIFY(dynamic_cast<XenLib::FailedLoginAttemptAlert*>(full.data()));
        QCOMPARE(full->GetTitle(), QString("There has been a number of failed attempts to login to 'MyPool' within the past hour."));
        const QStringList lines = full->GetDescription().split('\n');
        QCOMPARE(lines.size(), 4);
        QCOMPARE(lines.at(0), QString("There has been a total of 12 failed login attempts."));
        QCOMPARE(lines.at(1), QString("2 attempts originated from unknown sources."));
        QCOMPARE(lines.at(2), QString("The most common attempts originated from the following sources:"));
        QVERIFY(lines.at(3).startsWith("- IP address: 203.0.113.5, Number of attempts: 10, Last attempt on: "));
        QVERIFY(lines.at(3).endsWith(", Username: root"));

        auto allUnknown = make("<body><unknown>5</unknown></body>");
        QCOMPARE(allUnknown->GetDescription(), QString("All 5 attempts originated from unknown sources."));

        // Unparsable body: description falls back to the generic MessageAlert title (C# base.Title);
        // FriendlyNames has no entry for this type, so that is the raw message name
        auto broken = make("not xml");
        QCOMPARE(broken->GetDescription(), QString("FAILED_LOGIN_ATTEMPTS"));
    }

    void serverCertificateTrustOnFirstUse()
    {
        QFile pem(QFINDTESTDATA("transport-testdata/server-cert.pem"));
        QVERIFY(pem.open(QIODevice::ReadOnly));
        const QSslCertificate cert(pem.readAll());
        QVERIFY(!cert.isNull());

        XenCertificateManager* manager = XenCertificateManager::instance();
        const QString host = "tofu-test.invalid";
        using Prompt = XenCertificateManager::CertificatePrompt;
        const QList<QSslError> selfSigned{QSslError(QSslError::SelfSignedCertificate, cert)};

        // Only a host name mismatch still counts as a trusted chain (shown in the prompt)
        QVERIFY(!XenCertificateManager::IsTrustedChain(selfSigned));
        QVERIFY(XenCertificateManager::IsTrustedChain({QSslError(QSslError::HostNameMismatch, cert)}));

        // Restore the singleton for the other tests (no handler = accept everything)
        const auto restore = qScopeGuard([manager]() {
            manager->SetPromptHandler(nullptr);
            manager->SetKnownServers({});
            manager->SetWarnUnrecognizedCertificate(true);
            manager->SetWarnChangedCertificate(true);
        });
        manager->SetKnownServers({});

        // No handler (C#: no MainWindow): accepted without being remembered
        QVERIFY(manager->ValidateServerCertificate(cert, selfSigned, host));
        QVERIFY(manager->GetKnownServers().isEmpty());

        QList<Prompt> prompts;
        XenCertificateManager::PromptResult answer;
        manager->SetPromptHandler([&](Prompt prompt, const QSslCertificate&, const QString&, bool trusted) {
            prompts.append(prompt);
            Q_UNUSED(trusted)
            return answer;
        });

        // Unknown certificate, rejected: not remembered
        answer = {false, false};
        QVERIFY(!manager->ValidateServerCertificate(cert, selfSigned, host));
        QCOMPARE(prompts, QList<Prompt>{Prompt::Unknown});
        QVERIFY(!manager->GetKnownServers().contains(host));

        // Accepted: remembered by SHA-1, and not asked again
        answer = {true, false};
        QVERIFY(manager->ValidateServerCertificate(cert, selfSigned, host));
        QCOMPARE(manager->GetKnownServers().value(host), XenCertificateManager::CertificateHashString(cert));
        QCOMPARE(manager->GetKnownServers().value(host).length(), 40);
        prompts.clear();
        QVERIFY(manager->ValidateServerCertificate(cert, selfSigned, host));
        QVERIFY(prompts.isEmpty());

        // Different remembered hash: "changed" prompt; rejecting keeps the old hash
        manager->SetKnownServers({{host, "OLDHASH"}});
        answer = {false, false};
        QVERIFY(!manager->ValidateServerCertificate(cert, selfSigned, host));
        QCOMPARE(prompts, QList<Prompt>{Prompt::Changed});
        QCOMPARE(manager->GetKnownServers().value(host), QString("OLDHASH"));

        // Accepting with "Always ignore" replaces the hash and turns off change warnings
        answer = {true, true};
        QVERIFY(manager->ValidateServerCertificate(cert, selfSigned, host));
        QCOMPARE(manager->GetKnownServers().value(host), XenCertificateManager::CertificateHashString(cert));
        QVERIFY(!manager->GetWarnChangedCertificate());

        // Change warnings off: a changed certificate is replaced silently
        prompts.clear();
        manager->SetKnownServers({{host, "OLDHASH"}});
        QVERIFY(manager->ValidateServerCertificate(cert, selfSigned, host));
        QVERIFY(prompts.isEmpty());

        // New-certificate warnings off: an unknown one is added silently
        manager->SetKnownServers({});
        manager->SetWarnUnrecognizedCertificate(false);
        QVERIFY(manager->ValidateServerCertificate(cert, selfSigned, host));
        QVERIFY(prompts.isEmpty());
        QVERIFY(manager->GetKnownServers().contains(host));

        // No SSL errors (C# SslPolicyErrors.None): accepted without any check
        manager->SetKnownServers({});
        manager->SetWarnUnrecognizedCertificate(true);
        QVERIFY(manager->ValidateServerCertificate(cert, {}, host));
        QVERIFY(manager->GetKnownServers().isEmpty());

        // A certificate a known pool reports for one of its hosts (e.g. the member a console
        // connects to) is accepted without a prompt and remembered for that host name
        {
            XenConnection* pool = new XenConnection();
            Xen::ConnectionsManager::instance()->AddConnection(pool);
            const auto removePool = qScopeGuard([pool]() { Xen::ConnectionsManager::instance()->RemoveConnection(pool); });
            QString sha256 = QString::fromLatin1(cert.digest(QCryptographicHash::Sha256).toHex()).toUpper();
            for (int i = sha256.length() - 2; i > 0; i -= 2)
                sha256.insert(i, ':'); // XAPI format AA:BB:...
            pool->GetCache()->Update(XenObjectType::Certificate, "OpaqueRef:cert",
                                     QVariantMap{{"type", "host"}, {"fingerprint_sha256", sha256}});
            QVERIFY(XenCertificateManager::IsKnownPoolCertificate(cert));

            manager->SetKnownServers({});
            manager->SetWarnUnrecognizedCertificate(true);
            prompts.clear();
            QVERIFY(manager->ValidateServerCertificate(cert, selfSigned, "member-by-ip.invalid"));
            QVERIFY(prompts.isEmpty());
            QCOMPARE(manager->GetKnownServers().value("member-by-ip.invalid"), XenCertificateManager::CertificateHashString(cert));
        }
        QVERIFY(!XenCertificateManager::IsKnownPoolCertificate(cert));

        // A missing certificate with errors is rejected
        QVERIFY(!manager->ValidateServerCertificate(QSslCertificate(), selfSigned, host));
    }

    void groupHideMatchesCSharp()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        auto obj = [&](XenObjectType type, const QString& ref) { return cache->ResolveObject(type, ref); };
        const QVariantMap hiddenConfig{{"HideFromXenCenter", "true"}};

        cache->Update(XenObjectType::HostMetrics, "OpaqueRef:m-live", QVariantMap{{"live", true}});
        cache->Update(XenObjectType::HostMetrics, "OpaqueRef:m-dead", QVariantMap{{"live", false}});
        cache->Update(XenObjectType::Host, "OpaqueRef:live", QVariantMap{{"name_label", "live"}, {"metrics", "OpaqueRef:m-live"}});
        cache->Update(XenObjectType::Host, "OpaqueRef:dead", QVariantMap{{"name_label", "dead"}, {"metrics", "OpaqueRef:m-dead"}});

        // VMs
        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{{"name_label", "vm"}, {"power_state", "Halted"}});
        cache->Update(XenObjectType::VM, "OpaqueRef:vm-hidden", QVariantMap{{"name_label", "h"}, {"power_state", "Halted"}, {"other_config", hiddenConfig}});
        cache->Update(XenObjectType::VM, "OpaqueRef:vm-gui", QVariantMap{{"name_label", "__gui__temp"}, {"power_state", "Halted"}});
        cache->Update(XenObjectType::VM, "OpaqueRef:dom0", QVariantMap{{"name_label", "dom0"}, {"is_control_domain", true}});
        cache->Update(XenObjectType::VM, "OpaqueRef:vm-on-dead", QVariantMap{
            {"name_label", "d"}, {"power_state", "Running"}, {"resident_on", "OpaqueRef:dead"}});
        QVERIFY(!XenSearch::Group::Hide(obj(XenObjectType::VM, "OpaqueRef:vm"), false));
        QVERIFY(XenSearch::Group::Hide(obj(XenObjectType::VM, "OpaqueRef:vm-hidden"), false));
        QVERIFY(!XenSearch::Group::Hide(obj(XenObjectType::VM, "OpaqueRef:vm-hidden"), true));
        QVERIFY(XenSearch::Group::Hide(obj(XenObjectType::VM, "OpaqueRef:vm-gui"), true));
        QVERIFY(XenSearch::Group::Hide(obj(XenObjectType::VM, "OpaqueRef:dom0"), true));
        QVERIFY(XenSearch::Group::Hide(obj(XenObjectType::VM, "OpaqueRef:vm-on-dead"), true));

        // vTPM templates are hidden only while a host restricts vTPM
        cache->Update(XenObjectType::VM, "OpaqueRef:vtpm-tmpl", QVariantMap{
            {"name_label", "t"}, {"is_a_template", true}, {"platform", QVariantMap{{"vtpm", "true"}}}});
        QVERIFY(XenSearch::Group::Hide(obj(XenObjectType::VM, "OpaqueRef:vtpm-tmpl"), true)); // restrict_vtpm missing = restricted
        cache->Update(XenObjectType::Host, "OpaqueRef:live", QVariantMap{
            {"name_label", "live"}, {"metrics", "OpaqueRef:m-live"}, {"license_params", QVariantMap{{"restrict_vtpm", "false"}}}});
        cache->Update(XenObjectType::Host, "OpaqueRef:dead", QVariantMap{
            {"name_label", "dead"}, {"metrics", "OpaqueRef:m-dead"}, {"license_params", QVariantMap{{"restrict_vtpm", "false"}}}});
        QVERIFY(!XenSearch::Group::Hide(obj(XenObjectType::VM, "OpaqueRef:vtpm-tmpl"), false));

        // SRs
        cache->Update(XenObjectType::SR, "OpaqueRef:sr", QVariantMap{{"name_label", "sr"}, {"type", "nfs"}, {"shared", true}});
        cache->Update(XenObjectType::SR, "OpaqueRef:sr-hidden", QVariantMap{{"name_label", "h"}, {"type", "nfs"}, {"shared", true}, {"other_config", hiddenConfig}});
        cache->Update(XenObjectType::SR, "OpaqueRef:sr-tmpfs", QVariantMap{{"name_label", "mem"}, {"type", "tmpfs"}});
        cache->Update(XenObjectType::SR, "OpaqueRef:sr-tools", QVariantMap{{"name_label", "tools"}, {"type", "iso"}, {"is_tools_sr", true}});
        cache->Update(XenObjectType::PBD, "OpaqueRef:pbd", QVariantMap{{"host", "OpaqueRef:dead"}, {"SR", "OpaqueRef:sr-local"}});
        cache->Update(XenObjectType::SR, "OpaqueRef:sr-local", QVariantMap{
            {"name_label", "local"}, {"type", "ext"}, {"shared", false}, {"PBDs", QVariantList{"OpaqueRef:pbd"}}});
        QVERIFY(!XenSearch::Group::Hide(obj(XenObjectType::SR, "OpaqueRef:sr"), false));
        QVERIFY(XenSearch::Group::Hide(obj(XenObjectType::SR, "OpaqueRef:sr-hidden"), false));
        QVERIFY(!XenSearch::Group::Hide(obj(XenObjectType::SR, "OpaqueRef:sr-hidden"), true));
        QVERIFY(XenSearch::Group::Hide(obj(XenObjectType::SR, "OpaqueRef:sr-tmpfs"), true));
        QVERIFY(XenSearch::Group::Hide(obj(XenObjectType::SR, "OpaqueRef:sr-tools"), true));
        QVERIFY(XenSearch::Group::Hide(obj(XenObjectType::SR, "OpaqueRef:sr-local"), true)); // home server not live

        // Networks: the guest installer network is shown only with hidden objects
        cache->Update(XenObjectType::Network, "OpaqueRef:himn", QVariantMap{
            {"name_label", "himn"}, {"other_config", QVariantMap{{"is_guest_installer_network", "true"}}}});
        QVERIFY(XenSearch::Group::Hide(obj(XenObjectType::Network, "OpaqueRef:himn"), false));
        QVERIFY(!XenSearch::Group::Hide(obj(XenObjectType::Network, "OpaqueRef:himn"), true));

        // VDI.Show: missing or unmanaged disks are never shown, hidden ones only with hidden objects
        cache->Update(XenObjectType::VDI, "OpaqueRef:vdi", QVariantMap{{"name_label", "d"}});
        cache->Update(XenObjectType::VDI, "OpaqueRef:vdi-missing", QVariantMap{{"name_label", "d"}, {"missing", true}});
        cache->Update(XenObjectType::VDI, "OpaqueRef:vdi-hidden", QVariantMap{{"name_label", "d"}, {"other_config", hiddenConfig}});
        QVERIFY(obj(XenObjectType::VDI, "OpaqueRef:vdi")->Show(false));
        QVERIFY(!obj(XenObjectType::VDI, "OpaqueRef:vdi-missing")->Show(true));
        QVERIFY(!obj(XenObjectType::VDI, "OpaqueRef:vdi-hidden")->Show(false));
        QVERIFY(obj(XenObjectType::VDI, "OpaqueRef:vdi-hidden")->Show(true));

        // Unresolvable objects are hidden
        QVERIFY(XenSearch::Group::Hide(QSharedPointer<XenObject>(), true));
    }

    void vmPowerActionTitles()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::Pool, "OpaqueRef:pool", QVariantMap{{"name_label", "MyPool"}, {"master", "OpaqueRef:h1"}});
        cache->Update(XenObjectType::Host, "OpaqueRef:h1", QVariantMap{{"name_label", "host1"}});
        cache->Update(XenObjectType::VM, "OpaqueRef:running", QVariantMap{
            {"name_label", "vm1"}, {"power_state", "Running"}, {"resident_on", "OpaqueRef:h1"}, {"is_a_template", false}});
        cache->Update(XenObjectType::VM, "OpaqueRef:halted", QVariantMap{
            {"name_label", "vm2"}, {"power_state", "Halted"}, {"affinity", "OpaqueRef:NULL"}, {"is_a_template", false}});
        QCOMPARE(connection.GetName(), QString("MyPool"));

        // Running VM: its home is the server it runs on
        QSharedPointer<VM> running = cache->ResolveObject<VM>("OpaqueRef:running");
        VMCleanReboot reboot(running);
        QCOMPARE(reboot.GetTitle(), QString("Rebooting VM 'vm1' on 'host1'"));
        QCOMPARE(reboot.GetHost(), running->GetHome());
        VMCleanShutdown shutdown(running);
        QCOMPARE(shutdown.GetTitle(), QString("Shutting down VM 'vm1' on 'host1'"));
        VMSuspendAction suspend(running);
        QCOMPARE(suspend.GetTitle(), QString("Suspending VM 'vm1'"));

        // No home server: the connection (pool) name is used
        VMHardShutdown hardShutdown(cache->ResolveObject<VM>("OpaqueRef:halted"));
        QCOMPARE(hardShutdown.GetTitle(), QString("Shutting down VM 'vm2' on 'MyPool'"));
    }

    void vmMaxVIFsAllowed()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();

        // No restriction anywhere: C# default of 7
        cache->Update(XenObjectType::VM, "OpaqueRef:plain", QVariantMap{{"is_a_template", false}});
        QCOMPARE(cache->ResolveObject<VM>("OpaqueRef:plain")->MaxVIFsAllowed(), 7);

        // A template's own restriction applies to the template
        const QString recommendations = "<restrictions><restriction field=\"number-of-vifs\" max=\"16\" /></restrictions>";
        cache->Update(XenObjectType::VM, "OpaqueRef:tmpl", QVariantMap{
            {"is_a_template", true}, {"reference_label", "debian"}, {"recommendations", recommendations}});
        QCOMPARE(cache->ResolveObject<VM>("OpaqueRef:tmpl")->MaxVIFsAllowed(), 16);

        // A VM uses the template with the same reference_label
        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{{"is_a_template", false}, {"reference_label", "debian"}});
        QCOMPARE(cache->ResolveObject<VM>("OpaqueRef:vm")->MaxVIFsAllowed(), 16);
    }

    void leafCoalesceAlert()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::VDI, "OpaqueRef:vdi", QVariantMap{{"uuid", "vdi-uuid"}, {"name_label", "disk0"}, {"VBDs", QVariantList{"OpaqueRef:vbd"}}});
        cache->Update(XenObjectType::VBD, "OpaqueRef:vbd", QVariantMap{{"VM", "OpaqueRef:vm"}, {"VDI", "OpaqueRef:vdi"}});
        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{{"name_label", "vm1"}});

        auto make = [&](const QString& name, const QString& objUuid)
        {
            return QSharedPointer<XenLib::Alert>(XenLib::MessageAlert::ParseMessage(&connection, QVariantMap{
                {"ref", "OpaqueRef:msg"}, {"name", name}, {"cls", "VDI"}, {"obj_uuid", objUuid},
                {"body", ""}, {"priority", 5}, {"timestamp", 0}}));
        };

        auto started = make("LEAF_COALESCE_START_MESSAGE", "vdi-uuid");
        QVERIFY(dynamic_cast<XenLib::LeafCoalesceAlert*>(started.data()));
        QCOMPARE(started->GetTitle(), QString("Data merge for VM vm1 has started."));
        QCOMPARE(started->GetDescription(), QString("Data merge for VDI 'disk0' of VM 'vm1' has started, performance may be reduced."));

        auto completed = make("LEAF_COALESCE_COMPLETED", "vdi-uuid");
        QCOMPARE(completed->GetDescription(), QString("Data merge for VDI 'disk0' of VM 'vm1' has completed."));

        // VDI not in the cache: its uuid is used and the VM is unknown
        auto failed = make("LEAF_COALESCE_FAILED", "gone-uuid");
        QCOMPARE(failed->GetTitle(), QString("Data merge for VM Unknown has failed."));
        QCOMPARE(failed->GetDescription(), QString("Data merge for VDI 'gone-uuid' of VM 'Unknown' has failed."));
    }

    void messageAlertText()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::Pool, "OpaqueRef:pool", QVariantMap{{"uuid", "pool-uuid"}, {"name_label", "MyPool"}, {"master", "OpaqueRef:h1"}});
        cache->Update(XenObjectType::Host, "OpaqueRef:h1", QVariantMap{{"uuid", "host-uuid-1"}, {"name_label", "host1"}});
        cache->Update(XenObjectType::Host, "OpaqueRef:h2", QVariantMap{{"uuid", "host-uuid-2"}, {"name_label", "host2"}});

        auto make = [&](const QString& name, const QString& cls, const QString& objUuid, const QString& body)
        {
            return QSharedPointer<XenLib::MessageAlert>(new XenLib::MessageAlert(&connection, QVariantMap{
                {"ref", "OpaqueRef:msg"}, {"name", name}, {"cls", cls}, {"obj_uuid", objUuid},
                {"body", body}, {"priority", 2}, {"timestamp", 0}}));
        };

        // Host in a pool: title is prefixed with the host name, body names the host
        auto hostFailed = make("HA_HOST_FAILED", "Host", "host-uuid-1", "raw body");
        QCOMPARE(hostFailed->GetTitle(), QString("host1: Server failed"));
        QCOMPARE(hostFailed->GetDescription(), QString("Server 'host1' has failed."));
        QCOMPARE(hostFailed->AppliesTo(), QString("MyPool"));

        // Pool message: no name prefix, failover tolerance variant chosen from the body
        auto dropIn = make("HA_POOL_DROP_IN_PLAN_EXISTS_FOR", "Pool", "pool-uuid", "1");
        QCOMPARE(dropIn->GetTitle(), Message::FriendlyName("HA_POOL_DROP_IN_PLAN_EXISTS_FOR"));
        QCOMPARE(dropIn->GetDescription(),
                 FriendlyNames::Format(Message::FriendlyBody("ha_pool_drop_in_plan_exists_for-1"), {"MyPool", "1"}));

        // Multipath alert with one unhealthy host
        const QString multipathBody = "Unhealthy paths:\n"
            "[20090511T16:29:22Z] host=host-uuid-2; host-name=x; pbd=p; scsi_id=s; current=1; max=2\n";
        auto multipath = make("MULTIPATH_PERIODIC_ALERT", "Host", "host-uuid-1", multipathBody);
        QCOMPARE(multipath->GetDescription(),
                 FriendlyNames::Format(Message::FriendlyBody("MULTIPATH_PERIODIC_ALERT"), {"host2", "1", "2"}));

        // Unknown message type and unresolved object: raw name and body
        auto unknown = make("SOMETHING_NEW", "VM", "missing-uuid", "raw body");
        QCOMPARE(unknown->GetTitle(), QString("SOMETHING_NEW"));
        QCOMPARE(unknown->GetDescription(), QString("raw body"));
    }
    void attachDiskEligibility_data()
    {
        QTest::addColumn<QVariantMap>("properties");
        QTest::addColumn<QVariantList>("attachments");
        QTest::addColumn<bool>("showHidden");
        QTest::addColumn<bool>("eligible");
        QTest::addColumn<bool>("readOnly");
        const QVariantList activeRW{QVariantMap{{"mode", "RW"}, {"currently_attached", true}}};
        const QVariantList activeRO{QVariantMap{{"mode", "RO"}, {"currently_attached", true}}};
        const QVariantList detachedRW{QVariantMap{{"mode", "RW"}, {"currently_attached", false}}};
        QTest::newRow("unused-user-disk") << QVariantMap{} << QVariantList{} << false << true << false;
        QTest::newRow("unmanaged-base-disk") << QVariantMap{{"managed", false}} << QVariantList{} << false << false << false;
        QTest::newRow("base-disk-hidden-even-with-show-hidden") << QVariantMap{{"managed", false}} << QVariantList{} << true << false << false;
        QTest::newRow("missing-disk") << QVariantMap{{"missing", true}} << QVariantList{} << true << false << false;
        const QVariantMap hidden{{"other_config", QVariantMap{{"HideFromXenCenter", "true"}}}};
        QTest::newRow("hidden-disk") << hidden << QVariantList{} << false << false << false;
        QTest::newRow("show-hidden-disk") << hidden << QVariantList{} << true << true << false;
        for (const QString& type : {QString("crashdump"), QString("ephemeral"), QString("suspend")})
            QTest::newRow(qPrintable(type)) << QVariantMap{{"type", type}} << QVariantList{} << false << false << false;
        QTest::newRow("read-only-disk") << QVariantMap{{"read_only", true}} << QVariantList{} << false << true << true;
        QTest::newRow("active-writer") << QVariantMap{} << activeRW << false << false << false;
        QTest::newRow("active-reader") << QVariantMap{} << activeRO << false << true << true;
        QTest::newRow("detached-writer") << QVariantMap{} << detachedRW << false << true << true;
        QTest::newRow("sharable-active-writer") << QVariantMap{{"sharable", true}} << activeRW << false << true << false;
        QTest::newRow("managed-snapshot-disk") << QVariantMap{{"is_a_snapshot", true}} << detachedRW << false << true << true;
    }

    void attachDiskEligibility()
    {
        QFETCH(QVariantMap, properties);
        QFETCH(QVariantList, attachments);
        QFETCH(bool, showHidden);
        QFETCH(bool, eligible);
        QFETCH(bool, readOnly);
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        QStringList refs;
        for (int i = 0; i < attachments.size(); ++i)
        {
            const QString ref = QString("OpaqueRef:attach-vbd-%1").arg(i);
            cache->Update(XenObjectType::VBD, ref, attachments.at(i).toMap());
            refs.append(ref);
        }
        QVariantMap record{{"type", "user"}, {"managed", true}, {"VBDs", refs}};
        for (auto it = properties.cbegin(); it != properties.cend(); ++it)
            record[it.key()] = it.value();
        cache->Update(XenObjectType::VDI, "OpaqueRef:attach-vdi", record);
        const auto vdi = cache->ResolveObject<VDI>("OpaqueRef:attach-vdi");
        bool forceReadOnly = false;
        QCOMPARE(vdi->CanAttach(showHidden, &forceReadOnly), eligible);
        if (eligible)
            QCOMPARE(forceReadOnly, readOnly);
    }

    void attachStorageVisibilityAndAccessibility()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::Host, "OpaqueRef:attach-host", QVariantMap{{"name_label", "Host"}});
        cache->Update(XenObjectType::Host, "OpaqueRef:other-host", QVariantMap{{"name_label", "Other"}});
        QVariantMap pbd{{"host", "OpaqueRef:attach-host"}, {"currently_attached", true}};
        cache->Update(XenObjectType::PBD, "OpaqueRef:attach-pbd", pbd);
        QVariantMap record{{"name_label", "Storage"}, {"type", "lvm"},
                           {"PBDs", QStringList{"OpaqueRef:attach-pbd"}}};
        cache->Update(XenObjectType::SR, "OpaqueRef:attach-sr", record);
        const auto sr = cache->ResolveObject<SR>("OpaqueRef:attach-sr");
        const auto host = cache->ResolveObject<Host>("OpaqueRef:attach-host");
        QVERIFY(sr->IsVisible(false));
        QVERIFY(sr->CanBeSeenFrom(host));
        QVERIFY(!sr->CanBeSeenFrom(cache->ResolveObject<Host>("OpaqueRef:other-host")));
        pbd["currently_attached"] = false;
        cache->Update(XenObjectType::PBD, "OpaqueRef:attach-pbd", pbd);
        QVERIFY(!sr->CanBeSeenFrom(host));
        record["introduced_by"] = "OpaqueRef:dr-task";
        cache->Update(XenObjectType::SR, "OpaqueRef:attach-sr", record);
        QVERIFY(!sr->IsVisible(false));
        QVERIFY(sr->IsVisible(true));
        record["name_label"] = "__gui__temporary";
        cache->Update(XenObjectType::SR, "OpaqueRef:attach-sr", record);
        QVERIFY(!sr->IsVisible(true));
        record["name_label"] = "Memory";
        record["type"] = "tmpfs";
        cache->Update(XenObjectType::SR, "OpaqueRef:attach-sr", record);
        QVERIFY(!sr->IsVisible(true));
    }

    void templateClassificationPrecedence_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("builtIn");
        QTest::addColumn<bool>("isTemplate");
        QTest::addColumn<bool>("snapshot");
        QTest::addColumn<bool>("scheduled");
        QTest::addColumn<int>("expected");
        using Type = VM::VmTemplateType;
        QTest::newRow("named-custom-template") << QString("Windows Server backup") << false << true << false << false << int(Type::Custom);
        QTest::newRow("server-before-windows") << QString("Windows Server 2022") << true << true << false << false << int(Type::WindowsServer);
        QTest::newRow("legacy-before-server") << QString("Legacy Windows Server") << true << true << false << false << int(Type::LegacyWindows);
        QTest::newRow("yinhe-before-kylin") << QString("Yinhe Kylin") << true << true << false << false << int(Type::YinheKylin);
        QTest::newRow("unknown-built-in") << QString("Other install media") << true << true << false << false << int(Type::Misc);
        QTest::newRow("snapshot-before-os") << QString("Windows Server snapshot") << true << true << true << false << int(Type::Snapshot);
        QTest::newRow("scheduled-before-snapshot") << QString("Scheduled snapshot") << false << true << true << true << int(Type::SnapshotFromVmpp);
        QTest::newRow("regular-vm") << QString("Ubuntu") << true << false << false << false << int(Type::NoTemplate);
    }

    void templateClassificationPrecedence()
    {
        QFETCH(QString, name);
        QFETCH(bool, builtIn);
        QFETCH(bool, isTemplate);
        QFETCH(bool, snapshot);
        QFETCH(bool, scheduled);
        QFETCH(int, expected);
        QVariantMap data{{"name_label", name}, {"is_a_template", isTemplate},
                         {"is_a_snapshot", snapshot}, {"is_snapshot_from_vmpp", scheduled}};
        if (builtIn)
            data["other_config"] = QVariantMap{{"default_template", "false"}};
        const auto vm = makeVm("template-classification", data);
        QCOMPARE(int(vm->TemplateType()), expected);
    }

    void diskDeviceSelection_data()
    {
        QTest::addColumn<bool>("create");
        QTest::addColumn<int>("devices");
        QTest::addColumn<bool>("existingBootable");
        QTest::addColumn<bool>("plugFails");
        QTest::newRow("create-fills-gap-before-dvd") << true << int(TransportTestServer::DiskDevices) << true << false;
        QTest::newRow("attach-fills-gap-before-dvd") << false << int(TransportTestServer::DiskDevices) << true << false;
        QTest::newRow("create-full-no-orphan-vdi") << true << int(TransportTestServer::EmptyDevices) << true << false;
        QTest::newRow("attach-full") << false << int(TransportTestServer::EmptyDevices) << true << false;
        QTest::newRow("first-created-disk-bootable") << true << int(TransportTestServer::BootDevice) << false << false;
        QTest::newRow("keep-existing-bootable-disk") << true << int(TransportTestServer::BootDevice) << true << false;
        QTest::newRow("first-attached-disk-bootable") << false << int(TransportTestServer::BootDevice) << false << false;
        QTest::newRow("propagate-hotplug-error") << true << int(TransportTestServer::DiskDevices) << true << true;
    }

    void diskDeviceSelection()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        QFETCH(bool, create);
        QFETCH(int, devices);
        QFETCH(bool, existingBootable);
        QFETCH(bool, plugFails);
        const bool full = devices == TransportTestServer::EmptyDevices;
        QVector<TransportTestServer::Reply> replies{TransportTestServer::Login, TransportTestServer::Close,
                                                   TransportTestServer::Reply(devices)};
        if (!full)
        {
            if (create)
                replies.append(TransportTestServer::CreatedVDI);
            replies.append(TransportTestServer::CreatedVBD);
            replies.append(plugFails ? TransportTestServer::PlugAllowed : TransportTestServer::Close);
            replies.append(plugFails ? TransportTestServer::ApiFailure : TransportTestServer::RunningVM);
        }
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"), replies);
        const int port = server.startListening();
        QVERIFY(port > 0);
        XenConnection connection;
        QVERIFY(connection.ConnectToHost("127.0.0.1", port, "test", "test"));
        QTRY_VERIFY_WITH_TIMEOUT(connection.IsTransportConnected(), 5000);
        XenAPI::Session session(&connection);
        QVERIFY(session.Login("test", "test"));
        session.SetOwnsSessionToken(false);
        connection.SetSession(&session);
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::VM, "OpaqueRef:disk-vm",
                      QVariantMap{{"name_label", "Disk VM"}, {"power_state", "Running"},
                                  {"VBDs", QStringList{"OpaqueRef:disk", "OpaqueRef:dvd"}}});
        cache->Update(XenObjectType::VBD, "OpaqueRef:disk",
                      QVariantMap{{"VM", "OpaqueRef:disk-vm"}, {"userdevice", devices == TransportTestServer::BootDevice ? "2" : "0"},
                                  {"type", "Disk"}, {"bootable", existingBootable}});
        cache->Update(XenObjectType::VBD, "OpaqueRef:dvd",
                      QVariantMap{{"VM", "OpaqueRef:disk-vm"}, {"userdevice", "3"}, {"type", "CD"}});
        const auto vm = cache->ResolveObject<VM>("OpaqueRef:disk-vm");
        QVariantMap vbd{{"VM", vm->OpaqueRef()}, {"VDI", "OpaqueRef:existing-vdi"},
                        {"type", "Disk"}, {"empty", false}, {"mode", "RW"}};
        CreateDiskAction createAction(QVariantMap{{"name_label", "New disk"}}, vbd, vm);
        VbdCreateAndPlugAction attachAction(vm, vbd, "Existing disk");
        QSignalSpy createInstructions(&createAction, &CreateDiskAction::showUserInstruction);
        QSignalSpy attachInstructions(&attachAction, &VbdCreateAndPlugAction::showUserInstruction);
        AsyncOperation* action = create ? static_cast<AsyncOperation*>(&createAction) : &attachAction;
        action->RunSync(&session);
        QCOMPARE(action->HasError(), full || plugFails);
        QCOMPARE(create ? createInstructions.count() : attachInstructions.count(), !full && !plugFails ? 1 : 0);
        QStringList methods;
        for (const QJsonObject& request : server.recordedRequests())
        {
            const QString method = request.value("method").toString();
            methods.append(method);
            if (method == "VBD.create")
            {
                const QJsonObject record = request.value("params").toArray().at(1).toObject();
                QCOMPARE(record.value("userdevice").toString(), devices == TransportTestServer::BootDevice ? QString("0") : QString("1"));
                QCOMPARE(record.value("bootable").toBool(), devices == TransportTestServer::BootDevice && (!create || !existingBootable));
            }
        }
        QCOMPARE(methods.count("VM.get_allowed_VBD_devices"), 1);
        QCOMPARE(methods.count("VDI.create"), create && !full ? 1 : 0);
        QCOMPARE(methods.count("VBD.create"), full ? 0 : 1);
        QCOMPARE(methods.count("VBD.async_plug"), plugFails ? 1 : 0);
        if (create && !full)
            QVERIFY(methods.indexOf("VM.get_allowed_VBD_devices") < methods.indexOf("VDI.create"));
        connection.SetSession(nullptr);
    }

    void protectedVmStart_data()
    {
        QTest::addColumn<bool>("haEnabled");
        QTest::addColumn<QString>("priority");
        QTest::addColumn<int>("agilityReply");
        QTest::addColumn<bool>("accept");
        QTest::addColumn<bool>("clearFails");
        QTest::addColumn<bool>("isStart");
        QTest::newRow("confirm-start") << true << QString("restart") << int(TransportTestServer::AgilityFailure) << true << false << true;
        QTest::newRow("confirm-resume") << true << QString("restart") << int(TransportTestServer::AgilityFailure) << true << false << false;
        QTest::newRow("cancel") << true << QString("restart") << int(TransportTestServer::AgilityFailure) << false << false << true;
        QTest::newRow("clear-fails") << true << QString("restart") << int(TransportTestServer::AgilityFailure) << true << true << true;
        QTest::newRow("agile") << true << QString("restart") << int(TransportTestServer::Close) << true << false << true;
        QTest::newRow("transport-error-is-not-non-agility") << true << QString("restart") << int(TransportTestServer::Truncated) << true << false << true;
        QTest::newRow("permission-error-is-not-non-agility") << true << QString("restart") << int(TransportTestServer::PermissionFailure) << true << false << true;
        QTest::newRow("ha-disabled") << false << QString("restart") << int(TransportTestServer::AgilityFailure) << true << false << true;
        QTest::newRow("best-effort") << true << QString("best-effort") << int(TransportTestServer::AgilityFailure) << true << false << true;
    }

    void protectedVmStart()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        QFETCH(bool, haEnabled);
        QFETCH(QString, priority);
        QFETCH(int, agilityReply);
        QFETCH(bool, accept);
        QFETCH(bool, clearFails);
        QFETCH(bool, isStart);
        const bool checksAgility = haEnabled && priority == "restart";
        const bool prompts = checksAgility && agilityReply == TransportTestServer::AgilityFailure;
        QVector<TransportTestServer::Reply> replies{TransportTestServer::Login, TransportTestServer::Close};
        if (checksAgility)
            replies.append(TransportTestServer::Reply(agilityReply));
        if (prompts && accept)
            replies.append(clearFails ? TransportTestServer::ApiFailure : TransportTestServer::Close);
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"), replies);
        const int port = server.startListening();
        QVERIFY(port > 0);
        XenConnection connection;
        QVERIFY(connection.ConnectToHost("127.0.0.1", port, "test", "test"));
        QTRY_VERIFY_WITH_TIMEOUT(connection.IsTransportConnected(), 5000);
        XenAPI::Session session(&connection);
        QVERIFY(session.Login("test", "test"));
        session.SetOwnsSessionToken(false);
        connection.SetSession(&session);
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::Pool, "OpaqueRef:test-pool", QVariantMap{{"ha_enabled", haEnabled}});
        cache->Update(XenObjectType::VM, "OpaqueRef:test-vm",
                      QVariantMap{{"name_label", "HA VM"}, {"power_state", isStart ? "Halted" : "Suspended"},
                                  {"ha_restart_priority", priority}});
        const auto vm = cache->ResolveObject<VM>("OpaqueRef:test-vm");
        bool warned = false;
        bool correctMode = false;
        HaTestStartAction action(vm, [&](QSharedPointer<VM> warnedVm, bool start) {
            warned = true;
            correctMode = warnedVm == vm && start == isStart;
            if (!accept)
                throw VMStartAbstractAction::CancelledByUser();
        }, isStart);
        action.RunSync(&session);
        QCOMPARE(warned, prompts);
        if (prompts)
            QVERIFY(correctMode);
        const bool cancelled = prompts && !accept;
        const bool failed = (checksAgility && agilityReply != TransportTestServer::Close && !prompts) || (prompts && accept && clearFails);
        QCOMPARE(action.GetState(), cancelled ? AsyncOperation::Cancelled : failed ? AsyncOperation::Failed : AsyncOperation::Completed);
        QCOMPARE(action.performed, !cancelled && !failed);

        int clears = 0;
        for (const QJsonObject& request : server.recordedRequests())
        {
            if (request.value("method").toString() == "VM.set_ha_restart_priority")
            {
                ++clears;
                const QJsonArray params = request.value("params").toArray();
                QCOMPARE(params.at(0).toString(), session.GetSessionID());
                QCOMPARE(params.at(1).toString(), vm->OpaqueRef());
                QCOMPARE(params.at(2).toString(), QString());
            }
        }
        QCOMPARE(clears, prompts && accept ? 1 : 0);
    }

    void jsonRpcStrictValidation()
    {
        using Xen::JsonRpcClient;
        QVERIFY(JsonRpcClient::parseJsonRpcResponseOrThrow(R"({"jsonrpc":"2.0","result":null,"id":1})").isNull());
        QCOMPARE(JsonRpcClient::parseJsonRpcResponseOrThrow(R"({"jsonrpc":"2.0","result":false,"id":1})").toBool(), false);
        for (const QByteArray& invalid : {QByteArray(), QByteArray("not JSON"), QByteArray("{}"),
             QByteArray(R"({"jsonrpc":"2.0","id":1})")})
        {
            QVERIFY_EXCEPTION_THROWN(JsonRpcClient::parseJsonRpcResponseOrThrow(invalid), std::runtime_error);
        }
        try
        {
            JsonRpcClient::parseJsonRpcResponseOrThrow(R"({"jsonrpc":"2.0","error":{"code":1,"message":"HOST_NOT_ENOUGH_FREE_MEMORY","data":["1024","512"]},"id":1})");
            QFAIL("API rejection was accepted");
        } catch (const Failure& failure)
        {
            QCOMPARE(failure.errorCode(), QString("HOST_NOT_ENOUGH_FREE_MEMORY"));
            QCOMPARE(failure.errorDescription().size(), 3);
            QCOMPARE(QString::fromUtf8(failure.what()), failure.message());
        }
    }

    void transportReconnectsBeforeNextRequest_data()
    {
        QTest::addColumn<int>("reply");
        QTest::newRow("announced-close") << int(TransportTestServer::Close);
        QTest::newRow("idle-peer-close") << int(TransportTestServer::PeerClose);
        QTest::newRow("chunked-close") << int(TransportTestServer::Chunked);
    }

    void transportReconnectsBeforeNextRequest()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        QFETCH(int, reply);
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"),
                                   {TransportTestServer::Reply(reply), TransportTestServer::Close});
        const int port = server.startListening();
        QVERIFY(port > 0);
        Xen::ConnectionWorker worker("127.0.0.1", port);
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(worker.IsTransportConnected(), 5000);
        const QByteArray request = Xen::JsonRpcClient::buildJsonRpcCall("VM.assert_can_boot_here", {}, 1);
        for (int i = 0; i < 2; ++i)
        {
            const int id = worker.QueueRequest(request, false);
            const QByteArray response = worker.WaitForResponse(id, 5000);
            QVERIFY(Xen::JsonRpcClient::parseJsonRpcResponseOrThrow(response).isNull());
            // Let the peer's close arrive before the next request.
            QTest::qWait(50);
        }
        QCOMPARE(server.requests(), 2);
        QCOMPARE(server.connections(), 2);
    }

    void transportFailureDoesNotReplayMutation_data()
    {
        QTest::addColumn<int>("reply");
        QTest::newRow("truncated-body") << int(TransportTestServer::Truncated);
        QTest::newRow("invalid-json") << int(TransportTestServer::Malformed);
        QTest::newRow("http-error") << int(TransportTestServer::HttpError);
    }

    void transportFailureDoesNotReplayMutation()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        QFETCH(int, reply);
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"),
                                   {TransportTestServer::Reply(reply), TransportTestServer::Close});
        const int port = server.startListening();
        QVERIFY(port > 0);
        Xen::ConnectionWorker worker("127.0.0.1", port);
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(worker.IsTransportConnected(), 5000);
        const QByteArray mutation = Xen::JsonRpcClient::buildJsonRpcCall("VM.start", {}, 1);
        const QByteArray failed = worker.WaitForResponse(worker.QueueRequest(mutation, false), 5000);
        const QJsonObject error = QJsonDocument::fromJson(failed).object().value("error").toObject();
        QCOMPARE(error.value("message").toString(), QString("TRANSPORT_ERROR"));
        QVERIFY(error.value("data").toArray().first().toString().contains("outcome is unknown"));
        QVERIFY_EXCEPTION_THROWN(Xen::JsonRpcClient::parseJsonRpcResponseOrThrow(failed), Failure);
        QVERIFY(!worker.IsTransportConnected());
        QCOMPARE(server.requests(), 1);

        const QByteArray read = Xen::JsonRpcClient::buildJsonRpcCall("VM.get_record", {}, 2);
        const QByteArray response = worker.WaitForResponse(worker.QueueRequest(read, false), 5000);
        QVERIFY(Xen::JsonRpcClient::parseJsonRpcResponseOrThrow(response).isNull());
        QCOMPARE(server.requests(), 2);
        QCOMPARE(server.connections(), 2);
    }

    void transportPreservesApiFailure()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"),
                                   {TransportTestServer::ApiFailure, TransportTestServer::Close});
        const int port = server.startListening();
        QVERIFY(port > 0);
        Xen::ConnectionWorker worker("127.0.0.1", port);
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(worker.IsTransportConnected(), 5000);
        const QByteArray request = Xen::JsonRpcClient::buildJsonRpcCall("VM.assert_can_boot_here", {}, 1);
        const QByteArray response = worker.WaitForResponse(worker.QueueRequest(request, false), 5000);
        QCOMPARE(QJsonDocument::fromJson(response).object().value("error").toObject().value("message").toString(),
                 QString("HOST_NOT_ENOUGH_FREE_MEMORY"));
        QVERIFY(worker.IsTransportConnected());
        const QByteArray next = worker.WaitForResponse(worker.QueueRequest(request, false), 5000);
        QVERIFY(Xen::JsonRpcClient::parseJsonRpcResponseOrThrow(next).isNull());
        QCOMPARE(server.connections(), 1);
    }

    void timedOutQueuedRequestIsNeverSent()
    {
        if (!QSslSocket::supportsSsl())
            QSKIP("TLS backend unavailable");
        TransportTestServer server(QFINDTESTDATA("transport-testdata/server-cert.pem"),
                                   QFINDTESTDATA("transport-testdata/server-key.pem"), {TransportTestServer::Close});
        const int port = server.startListening();
        QVERIFY(port > 0);
        Xen::ConnectionWorker worker("127.0.0.1", port);
        const QByteArray request = Xen::JsonRpcClient::buildJsonRpcCall("VM.start", {}, 1);
        const QByteArray cancelled = worker.WaitForResponse(worker.QueueRequest(request, false), 1);
        QVERIFY(cancelled.contains("Request was not sent"));
        worker.start();
        QTRY_VERIFY_WITH_TIMEOUT(worker.IsTransportConnected(), 5000);
        const QByteArray response = worker.WaitForResponse(worker.QueueRequest(request, false), 5000);
        QVERIFY(Xen::JsonRpcClient::parseJsonRpcResponseOrThrow(response).isNull());
        QCOMPARE(server.requests(), 1);
    }

    void connectionManagerWaitsForLogicalReadiness()
    {
        XenConnection connection;
        auto* manager = Xen::ConnectionsManager::instance();
        manager->AddConnection(&connection);
        QSignalSpy ready(manager, &Xen::ConnectionsManager::connectionStateChanged);

        // Simulate transport establishment without login/cache completion.
        const bool established = QMetaObject::invokeMethod(&connection, "onWorkerEstablished", Qt::DirectConnection);
        const int beforeLogin = ready.count();
        connection.ConnectionResult(true, QString());
        const int afterLogin = ready.count();
        connection.EndConnect();
        manager->RemoveConnection(&connection);

        QVERIFY(established);
        QCOMPARE(beforeLogin, 0);
        QCOMPARE(afterLogin, 1);
    }

    void heartbeatReportsTwoConsecutiveSessionFailures()
    {
        XenConnection connection;
        QVERIFY(QMetaObject::invokeMethod(&connection, "onWorkerEstablished", Qt::DirectConnection));
        // A connected transport without a logged-in session cannot create a probe.
        XenHeartbeat heartbeat(&connection, 15000);
        QSignalSpy lost(&heartbeat, &XenHeartbeat::connectionLost);
        heartbeat.start();
        QVERIFY(heartbeat.isRunning());

        QVERIFY(QMetaObject::invokeMethod(&heartbeat, "performHeartbeat", Qt::DirectConnection));
        QCOMPARE(lost.count(), 0);
        QVERIFY(heartbeat.isRunning());
        QVERIFY(QMetaObject::invokeMethod(&heartbeat, "performHeartbeat", Qt::DirectConnection));
        QCOMPARE(lost.count(), 1);
        QVERIFY(!heartbeat.isRunning());
    }

    void diskActionsUseCachedMetadataWithoutSession()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        const QString vdiRef = "OpaqueRef:metadata-vdi";
        const QString sourceSrRef = "OpaqueRef:metadata-source";
        const QString targetSrRef = "OpaqueRef:metadata-target";
        cache->Update(XenObjectType::SR, sourceSrRef, QVariantMap{{"name_label", "Source SR"}});
        cache->Update(XenObjectType::SR, targetSrRef, QVariantMap{{"name_label", "Target SR"}});
        cache->Update(XenObjectType::VDI, vdiRef,
                      QVariantMap{{"name_label", "Cached disk"}, {"SR", sourceSrRef}, {"type", "suspend"}});
        QVERIFY(!connection.GetSession());

        MoveVirtualDiskAction move(&connection, vdiRef, targetSrRef);
        MigrateVirtualDiskAction migrate(&connection, vdiRef, targetSrRef);
        for (const AsyncOperation* action : {static_cast<AsyncOperation*>(&move), static_cast<AsyncOperation*>(&migrate)})
        {
            QVERIFY(action->GetDescription().contains("Cached disk"));
            QVERIFY(action->GetDescription().contains("Source SR"));
            QVERIFY(action->GetDescription().contains("Target SR"));
        }
        QVERIFY(move.GetApiMethodsToRoleCheck().contains("vm.set_suspend_VDI"));
    }

    void diskActionMetadataFallsBackToReferences()
    {
        MoveVirtualDiskAction move(nullptr, "missing-vdi", "missing-sr");
        MigrateVirtualDiskAction migrate(nullptr, "missing-vdi", "missing-sr");
        for (const AsyncOperation* action : {static_cast<AsyncOperation*>(&move), static_cast<AsyncOperation*>(&migrate)})
        {
            QVERIFY(action->GetDescription().contains("missing-vdi"));
            QVERIFY(action->GetDescription().contains("missing-sr"));
        }
    }

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
        const QString xml = QString::fromUtf8(kOvfXml_ovfXenConfig_qtFormat_parsesHvmBootPolicy);

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("myvm"));
        QCOMPARE(cfg["myvm"]["HVM_boot_policy"], QString("BIOS order"));
        QCOMPARE(cfg["myvm"]["platform"], QString("nx=true;acpi=1"));
    }

    void ovfXenConfig_qtFormat_pvBootloader()
    {
        const QString xml = QString::fromUtf8(kOvfXml_ovfXenConfig_qtFormat_pvBootloader);

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
        const QString xml = QString::fromUtf8(kOvfXml_ovfXenConfig_csFormat_parsesBootPolicy);

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
        const QString xml = QString::fromUtf8(kOvfXml_ovfXenConfig_csFormat_recommendationsKeyExcluded);

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
        const QString xml = QString::fromUtf8(kOvfXml_ovfXenConfig_noXenSection_returnsEmptyMap);

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
        const QString xml = QString::fromUtf8(kOvfXml_ovfXenConfig_multipleVms_configsAreSeparate);

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
        const QString xml = QString::fromUtf8(kOvfXml_ovfXenConfig_emptyValueAllowed);

        OvfPackage pkg = OvfPackage::FromXml(xml);
        QVERIFY(pkg.IsValid());
        const auto cfg = pkg.XenConfigBySystem();
        QVERIFY(cfg.contains("uefivm"));
        QVERIFY(cfg["uefivm"].contains("HVM_boot_policy"));
        QCOMPARE(cfg["uefivm"]["HVM_boot_policy"], QString(""));
    }
};

QTEST_GUILESS_MAIN(XenLibTests)
#include "test_main.moc"
