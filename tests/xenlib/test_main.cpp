#include <QtTest>
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/vdi.h"
#include "xenlib/xen/sr.h"
#include "xenlib/xen/host.h"
#include "xenlib/xen/role.h"
#include "xenlib/utils/vncdes.h"
#include "xenlib/alerts/alertmanager.h"
#include "xenlib/xen/actions/wlb/wlbrecommendations.h"
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
#include "xenlib/xensearch/queries.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/xensearch/groupingtag.h"
#include "xenlib/xensearch/searchmarshalling.h"
#include "xenlib/xensearch/searchmanager.h"
#include "xenlib/customfields/customfieldsmanager.h"
#include "xenlib/xen/actions/vm/installpvtoolsaction.h"
#include "xenlib/xen/vdi.h"
#include <QSignalSpy>
#include "xenlib/xensearch/search.h"
#include "xenlib/xensearch/query.h"
#include "xenlib/xensearch/queryscope.h"
#include "xenlib/xen/network/certificatemanager.h"
#include "xenlib/xen/network.h"
#include "xenlib/xen/friendlynames.h"
#include "xenlib/xen/message.h"
#include "xenlib/alerts/messagealert.h"
#include "xenlib/alerts/failedloginattemptalert.h"
#include "xenlib/alerts/leafcoalescealert.h"
#include "xenlib/utils/filelogger.h"
#include "xenlib/xen/hiddenobjects.h"
#include "xenlib/xen/vmappliance.h"
#include "xenlib/vmhelpers.h"
#include <QTemporaryDir>
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

    void fileLoggerBatchesFiltersAndRotates()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileLogger logger;
        const QMessageLogContext context;
        auto readLog = [](const QString& path)
        {
            QFile file(path);
            return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
        };

        logger.Configure(true, false, FileLogger::MinimumTotalBytes, dir.path(), "Test");
        const QString logPath = logger.GetLogFilePath();
        QCOMPARE(logPath, QDir(dir.path()).filePath("Test.log"));

        // Debug is dropped unless verbose; other messages wait in memory until a flush
        logger.Write(QtDebugMsg, context, "debug line");
        logger.Write(QtInfoMsg, context, "info line");
        logger.Write(QtWarningMsg, context, "CONNECT Cookie: session_id=OpaqueRef:secret-session");
        QVERIFY(!QFileInfo::exists(logPath));
        logger.Flush();
        QString text = readLog(logPath);
        QVERIFY(text.contains("INFO  [") && text.contains("] - info line"));
        QVERIFY(!text.contains("debug line"));
        QVERIFY(text.contains("session_id=<hidden>"));
        QVERIFY(!text.contains("secret-session"));

        // Errors are written straight away
        logger.Write(QtCriticalMsg, context, "error line");
        QVERIFY(readLog(logPath).contains("ERROR [") );

        logger.Configure(true, true, FileLogger::MinimumTotalBytes, dir.path(), "Test");
        logger.Write(QtDebugMsg, context, "verbose debug line");
        logger.Flush();
        QVERIFY(readLog(logPath).contains("DEBUG [") );

        // Turned off: nothing more reaches the disk
        const qint64 sizeBefore = QFileInfo(logPath).size();
        logger.Configure(false, true, FileLogger::MinimumTotalBytes, dir.path(), "Test");
        logger.Write(QtCriticalMsg, context, "not written");
        logger.Flush();
        QCOMPARE(QFileInfo(logPath).size(), sizeBefore);

        // Writing three times the cap keeps FileCount files, each within its share of the cap
        logger.Configure(true, false, FileLogger::MinimumTotalBytes, dir.path(), "Test");
        const QString filler(1000, QLatin1Char('x'));
        for (int i = 0; i < 3 * 1024; ++i)
            logger.Write(QtWarningMsg, context, filler);
        logger.Flush();
        qint64 total = 0;
        for (const QString& path : logger.GetLogFilePaths())
        {
            QVERIFY2(QFileInfo::exists(path), qPrintable(path));
            QVERIFY(QFileInfo(path).size() <= logger.GetMaxFileBytes());
            total += QFileInfo(path).size();
        }
        QVERIFY(total <= FileLogger::MinimumTotalBytes);
        QVERIFY(!QFileInfo::exists(logPath + "." + QString::number(FileLogger::FileCount)));
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

        // XAPI before 1.295: a member without a certificate record is accepted by its
        // address, without a prompt and without being remembered
        {
            XenConnection* pool = new XenConnection();
            Xen::ConnectionsManager::instance()->AddConnection(pool);
            const auto removePool = qScopeGuard([pool]() { Xen::ConnectionsManager::instance()->RemoveConnection(pool); });
            pool->GetCache()->Update(XenObjectType::Host, "OpaqueRef:member", QVariantMap{{"address", "10.0.0.2"}});

            manager->SetKnownServers({});
            prompts.clear();
            QVERIFY(manager->ValidateServerCertificate(cert, selfSigned, "10.0.0.2"));
            QVERIFY(prompts.isEmpty());
            QVERIFY(manager->GetKnownServers().isEmpty());

            // Once the pool records a certificate for that host, a different one is not trusted
            pool->GetCache()->Update(XenObjectType::Certificate, "OpaqueRef:other",
                                     QVariantMap{{"type", "host"}, {"host", "OpaqueRef:member"}, {"fingerprint_sha256", "AA:BB"}});
            QVERIFY(!XenCertificateManager::IsUnrecordedPoolMember("10.0.0.2"));
            QVERIFY(!XenCertificateManager::IsUnrecordedPoolMember("10.0.0.3"));
        }

        // A missing certificate with errors is rejected
        QVERIFY(!manager->ValidateServerCertificate(QSslCertificate(), selfSigned, host));
    }

    void hiddenObjectsKeepObjectsOutOfTheTree()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::VM, "OpaqueRef:new-vm", QVariantMap{{"name_label", "new"}, {"power_state", "Halted"}});
        QSharedPointer<XenObject> vm = cache->ResolveObject(XenObjectType::VM, "OpaqueRef:new-vm");

        QSignalSpy changed(HiddenObjects::instance(), &HiddenObjects::Changed);
        QVERIFY(!HiddenObjects::IsHidden("OpaqueRef:new-vm"));
        QVERIFY(!XenSearch::Group::Hide(vm, false));

        HiddenObjects::Hide("OpaqueRef:new-vm");
        QVERIFY(HiddenObjects::IsHidden("OpaqueRef:new-vm"));
        // Hidden by a running operation even when the user shows hidden objects
        QVERIFY(XenSearch::Group::Hide(vm, true));

        HiddenObjects::Show("OpaqueRef:new-vm");
        QVERIFY(!HiddenObjects::IsHidden("OpaqueRef:new-vm"));
        QVERIFY(!XenSearch::Group::Hide(vm, false));
        QCOMPARE(changed.count(), 2);
    }

    void defaultVmNameSkipsTakenAndHiddenNames()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        QCOMPARE(VMHelpers::MakeHiddenName("Debian"), QString("__gui__Debian"));

        // C# Helpers.DefaultVMName always numbers, starting at 1
        QCOMPARE(VMHelpers::DefaultVMName("Debian", &connection), QString("Debian (1)"));

        cache->Update(XenObjectType::VM, "OpaqueRef:a", QVariantMap{{"name_label", "Debian (1)"}});
        // A VM still being created under the next name holds it too
        cache->Update(XenObjectType::VM, "OpaqueRef:b", QVariantMap{{"name_label", "__gui__Debian (2)"}});
        QCOMPARE(VMHelpers::DefaultVMName("Debian", &connection), QString("Debian (3)"));
    }

    void generalTabModelHelpersMatchCSharp()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();

        // Unknown coordinator: C# version helpers answer true
        QVERIFY(Host::XapiEqualOrGreater(&connection, "23.11.0"));
        QVERIFY(Host::NaplesOrGreater(&connection));

        cache->Update(XenObjectType::Host, "OpaqueRef:h", QVariantMap{
            {"uuid", "host-uuid"},
            {"software_version", QVariantMap{{"xapi", "22.27.0"}, {"platform_version", "2.9.0"}}},
            {"other_config", QVariantMap{{"multipathed", "true"}, {"mpath-boot", "[1, 2]"}}}});
        cache->Update(XenObjectType::Pool, "OpaqueRef:p", QVariantMap{{"master", "OpaqueRef:h"}});
        QVERIFY(Host::XapiEqualOrGreater(&connection, "22.26.0"));
        QVERIFY(!Host::XapiEqualOrGreater(&connection, "23.11.0"));
        QVERIFY(!Host::NaplesOrGreater(&connection));

        QSharedPointer<Host> host = cache->ResolveObject<Host>("OpaqueRef:h");
        int current = -1, max = -1;
        QVERIFY(host->GetBootPathCounts(current, max));
        QCOMPARE(current, 1);
        QCOMPARE(max, 2);
        cache->Update(XenObjectType::Host, "OpaqueRef:h2", QVariantMap{{"other_config", QVariantMap{{"mpath-boot", "[2, 2]"}}}});
        QVERIFY(!cache->ResolveObject<Host>("OpaqueRef:h2")->GetBootPathCounts(current, max)); // not multipathed

        // Boot mode
        cache->Update(XenObjectType::VM, "OpaqueRef:uefi", QVariantMap{
            {"HVM_boot_params", QVariantMap{{"firmware", " UEFI "}}}, {"platform", QVariantMap{{"secureboot", "Auto"}}}});
        QSharedPointer<VM> uefi = cache->ResolveObject<VM>("OpaqueRef:uefi");
        QVERIFY(uefi->IsDefaultBootModeUefi());
        QCOMPARE(uefi->GetSecureBootMode(), QString("auto"));

        // Read caching: per resident host in the VDI's sm_config; the last reason in C# order wins
        cache->Update(XenObjectType::VDI, "OpaqueRef:cached", QVariantMap{{"name_label", "cached"},
            {"sm_config", QVariantMap{{"read-caching-enabled-on-host-uuid", "true"}}}});
        cache->Update(XenObjectType::VDI, "OpaqueRef:srtype", QVariantMap{
            {"sm_config", QVariantMap{{"read-caching-reason-host-uuid", "SR_NOT_SUPPORTED"}}}});
        cache->Update(XenObjectType::VDI, "OpaqueRef:noro", QVariantMap{
            {"sm_config", QVariantMap{{"read-caching-reason-host-uuid", "NO_RO_IMAGE"}}}});
        cache->Update(XenObjectType::VBD, "OpaqueRef:b1", QVariantMap{{"VDI", "OpaqueRef:cached"}, {"currently_attached", true}});
        cache->Update(XenObjectType::VBD, "OpaqueRef:b2", QVariantMap{{"VDI", "OpaqueRef:srtype"}, {"currently_attached", true}});
        cache->Update(XenObjectType::VBD, "OpaqueRef:b3", QVariantMap{{"VDI", "OpaqueRef:noro"}, {"currently_attached", true}});
        cache->Update(XenObjectType::VBD, "OpaqueRef:b4", QVariantMap{{"VDI", "OpaqueRef:cached"}, {"currently_attached", false}});

        cache->Update(XenObjectType::VM, "OpaqueRef:rc", QVariantMap{
            {"resident_on", "OpaqueRef:h"}, {"VBDs", QVariantList{"OpaqueRef:b1", "OpaqueRef:b4"}}});
        QSharedPointer<VM> cachingVm = cache->ResolveObject<VM>("OpaqueRef:rc");
        QVERIFY(cachingVm->ReadCachingEnabled());
        QCOMPARE(cachingVm->ReadCachingVDIs().size(), 1);

        cache->Update(XenObjectType::VM, "OpaqueRef:norc", QVariantMap{
            {"resident_on", "OpaqueRef:h"}, {"VBDs", QVariantList{"OpaqueRef:b2", "OpaqueRef:b3", "OpaqueRef:b4"}}});
        QSharedPointer<VM> plainVm = cache->ResolveObject<VM>("OpaqueRef:norc");
        QVERIFY(!plainVm->ReadCachingEnabled());
        QCOMPARE(plainVm->ReadCachingDisabledReason(),
                 QString("This VM does not have any read-only disks or disks with a read-only parent"));
    }

    void roleListsComeFromSubroles()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::Role, "OpaqueRef:p-destroy", QVariantMap{{"name_label", "message.destroy"}, {"subroles", QVariantList()}});
        cache->Update(XenObjectType::Role, "OpaqueRef:p-console", QVariantMap{{"name_label", "http/connect_console"}, {"subroles", QVariantList()}});
        cache->Update(XenObjectType::Role, "OpaqueRef:admin", QVariantMap{{"name_label", "pool-admin"},
                                                                         {"subroles", QVariantList{"OpaqueRef:p-destroy", "OpaqueRef:p-console"}}});
        cache->Update(XenObjectType::Role, "OpaqueRef:ro", QVariantMap{{"name_label", "read-only"},
                                                                      {"subroles", QVariantList{"OpaqueRef:p-console"}}});

        // C# Role.ValidRoleList, case-insensitive like the permission names
        QList<QSharedPointer<Role>> roles = Role::ValidRoles(&connection, "Message.destroy");
        QCOMPARE(roles.size(), 1);
        QCOMPARE(roles.first()->OpaqueRef(), QString("OpaqueRef:admin"));
        QCOMPARE(roles.first()->FriendlyName(), QString("Pool Admin"));
        QCOMPARE(Role::ValidRoles(&connection, "http/connect_console").size(), 2);
        QVERIFY(Role::ValidRoles(&connection, "vm.destroy").isEmpty());

        // Without a session (e.g. disconnected) nothing is refused
        QVERIFY(Role::SessionCanPerform(&connection, "Message.destroy"));
    }

    void wlbReasonsAreReadable()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::Host, "OpaqueRef:a", QVariantMap{{"name_label", "a"}});
        cache->Update(XenObjectType::Host, "OpaqueRef:b", QVariantMap{{"name_label", "b"}});
        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{{"name_label", "vm"}, {"resident_on", "OpaqueRef:a"}});
        QSharedPointer<VM> vm = cache->ResolveObject<VM>(XenObjectType::VM, "OpaqueRef:vm");
        QSharedPointer<Host> b = cache->ResolveObject<Host>(XenObjectType::Host, "OpaqueRef:b");
        QSharedPointer<Host> c(new Host(&connection, "OpaqueRef:c"));

        QHash<QSharedPointer<VM>, QHash<QSharedPointer<Host>, QStringList>> recommendations;
        recommendations[vm][b] = QStringList{"HOST_NOT_ENOUGH_FREE_MEMORY", "1024", "512"};
        WlbRecommendations wlb({vm}, recommendations);

        // A XenAPI error is shown as its friendly text (C# new Failure(rec).ShortMessage)
        const QString reason = wlb.GetStarRating(b).CantRunReasons.value(vm);
        QVERIFY(!reason.isEmpty());
        QVERIFY(!reason.contains("HOST_NOT_ENOUGH_FREE_MEMORY"));

        // No recommendation for a host: it is not live
        const WlbRecommendation none = wlb.GetStarRating(c);
        QVERIFY(!none.CanRunByVM.value(vm, true));
        QCOMPARE(none.CantRunReasons.value(vm), QString("Host not live"));
    }

    void alertManagerAllowsCallbacksFromItsSignals()
    {
        // Receivers call back into the manager (the alert list rebuilds from it); with the
        // lock held while emitting, as before, every one of these calls deadlocked
        XenLib::AlertManager* manager = XenLib::AlertManager::instance();
        manager->ClearAllAlerts();
        XenConnection connection;
        auto makeAlert = [&connection](const QString& ref)
        {
            return new XenLib::MessageAlert(&connection, QVariantMap{{"opaque_ref", ref}, {"name", "TEST"}, {"uuid", ref},
                                                                     {"priority", 3}, {"timestamp", 0}});
        };

        int callbacks = 0;
        QObject receiver;
        QObject::connect(manager, &XenLib::AlertManager::collectionChanged, &receiver, [&]()
        {
            callbacks++;
            manager->NonDismissingAlertCount();
            manager->GetAllAlerts();
        });
        QObject::connect(manager, &XenLib::AlertManager::alertRemoved, &receiver, [&](XenLib::Alert*)
        {
            manager->FindAlert(QStringLiteral("none"));
        });

        manager->AddAlerts({makeAlert("OpaqueRef:a"), makeAlert("OpaqueRef:b"), makeAlert("OpaqueRef:c")});
        QCOMPARE(manager->AlertCount(), 3);

        manager->RemoveAlert(manager->GetAllAlerts().first());
        QCOMPARE(manager->AlertCount(), 2);

        // The predicate may use the manager too
        manager->RemoveAlerts([manager](XenLib::Alert* alert)
        {
            return manager->FindAlert(alert->GetUUID()) == alert;
        });
        QCOMPARE(manager->AlertCount(), 0);
        QVERIFY(callbacks >= 3);

        manager->ClearAllAlerts();
    }

    void vncDesMatchesStandardVectors()
    {
        // FIPS 46 / textbook vectors
        QCOMPARE(VncDes::EncryptBlock(QByteArray::fromHex("133457799BBCDFF1"), QByteArray::fromHex("0123456789ABCDEF")).toHex().toUpper(),
                 QByteArray("85E813540F0AB405"));
        QCOMPARE(VncDes::EncryptBlock(QByteArray::fromHex("0000000000000000"), QByteArray::fromHex("0000000000000000")).toHex().toUpper(),
                 QByteArray("8CA64DE9C1B123A7"));

        // VNC: each key byte bit-reversed, only the first 8 characters count, ECB over 16 bytes
        const QByteArray challenge = QByteArray::fromHex("0123456789ABCDEF0123456789ABCDEF");
        const QByteArray response = VncDes::ChallengeResponse(challenge, QString::fromLatin1("\xC8\x2C\xEA\x9E\xD9\x3D\xFB\x8F"));
        QCOMPARE(response.size(), 16);
        QCOMPARE(response.left(8).toHex().toUpper(), QByteArray("85E813540F0AB405")); // reversed 13 34 57 79 9B BC DF F1
        QCOMPARE(response.mid(8), response.left(8));
        QCOMPARE(VncDes::ChallengeResponse(challenge, "password"), VncDes::ChallengeResponse(challenge, "password-and-more"));
        QVERIFY(VncDes::ChallengeResponse(challenge, "password") != VncDes::ChallengeResponse(challenge, "Password"));
    }

    void hostTellsDom0FromDriverDomains()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        // The driver domain comes first: dom0 must not be "the first control domain"
        cache->Update(XenObjectType::VM, "OpaqueRef:driver", QVariantMap{{"is_control_domain", true}, {"domid", 3}});
        cache->Update(XenObjectType::VM, "OpaqueRef:dom0", QVariantMap{{"is_control_domain", true}, {"domid", 0}});
        cache->Update(XenObjectType::VM, "OpaqueRef:guest", QVariantMap{{"is_control_domain", false}, {"domid", 5}});
        QVariantMap hostData{{"resident_VMs", QVariantList{"OpaqueRef:driver", "OpaqueRef:dom0", "OpaqueRef:guest"}}};
        cache->Update(XenObjectType::Host, "OpaqueRef:host", hostData);
        QSharedPointer<Host> host = cache->ResolveObject<Host>(XenObjectType::Host, "OpaqueRef:host");
        QVERIFY(host);

        QCOMPARE(host->ControlDomainZero()->OpaqueRef(), QString("OpaqueRef:dom0"));
        QCOMPARE(host->OtherControlDomains().size(), 1);
        QCOMPARE(host->OtherControlDomains().first()->OpaqueRef(), QString("OpaqueRef:driver"));

        // control_domain, when the host reports it, decides
        hostData["control_domain"] = "OpaqueRef:dom0";
        cache->Update(XenObjectType::Host, "OpaqueRef:host", hostData);
        QCOMPARE(host->ControlDomainZero()->OpaqueRef(), QString("OpaqueRef:dom0"));
        QCOMPARE(host->OtherControlDomains().first()->OpaqueRef(), QString("OpaqueRef:driver"));
    }

    void groupingTagIsFreedWithTheLastReference()
    {
        TagsGrouping grouping(nullptr);
        QVariant itemData = GroupingTag::ToVariant(new GroupingTag(&grouping, QVariant(), QVariant("Tags")));
        QWeakPointer<GroupingTag> watch = itemData.value<QSharedPointer<GroupingTag>>();
        QVERIFY(GroupingTag::FromVariant(itemData));
        QCOMPARE(GroupingTag::FromVariant(itemData)->getGroup().toString(), QString("Tags"));
        QVERIFY(!GroupingTag::FromVariant(QVariant("not a tag")));

        // A copy (e.g. saved expansion state) keeps it alive after the tree item goes
        QVariant saved = itemData;
        itemData = QVariant();
        QVERIFY(!watch.isNull());
        saved = QVariant();
        QVERIFY(watch.isNull());
    }

    void customFieldDefinitionsAndValuesMatchCSharp()
    {
        // C# GetCustomFieldDefinitionXML, as XenCenter stores it in gui_config
        const QList<CustomFieldDefinition> definitions{
            CustomFieldDefinition("Owner", CustomFieldDefinition::Type::String),
            CustomFieldDefinition("Expires", CustomFieldDefinition::Type::Date)};
        const QString xml = CustomFieldsManager::DefinitionsToXml(definitions);
        QCOMPARE(xml, QString("<CustomFieldDefinitions>"
                              "<CustomFieldDefinition name=\"Owner\" type=\"String\" defaultValue=\"\"/>"
                              "<CustomFieldDefinition name=\"Expires\" type=\"Date\" defaultValue=\"\"/>"
                              "</CustomFieldDefinitions>"));
        const QList<CustomFieldDefinition> parsed = CustomFieldsManager::DefinitionsFromXml(xml);
        QCOMPARE(parsed.size(), 2);
        QCOMPARE(parsed.at(1).GetName(), QString("Expires"));
        QVERIFY(parsed.at(1).GetType() == CustomFieldDefinition::Type::Date);

        // Values: dates are stored in UTC with C#'s "u" format
        const CustomFieldDefinition expires = definitions.at(1);
        const QDateTime when(QDate(2026, 3, 4), QTime(5, 6, 7), Qt::UTC);
        QCOMPARE(CustomFieldsManager::ValueToInvariantString(expires.GetType(), when), QString("2026-03-04 05:06:07Z"));
        const QVariantMap otherConfig{{"XenCenter.CustomFields.Expires", "2026-03-04 05:06:07Z"},
                                      {"XenCenter.CustomFields.Owner", ""}};
        QCOMPARE(CustomFieldsManager::GetCustomFieldValue(otherConfig, expires).toDateTime(), when);
        QVERIFY(CustomFieldsManager::GetCustomFieldValue(otherConfig, definitions.at(0)).isNull());
        QVERIFY(CustomFieldsManager::ParseValue(CustomFieldDefinition::Type::Date, "not a date").isNull());
        QVERIFY(CustomFieldsManager::ValueToInvariantString(CustomFieldDefinition::Type::String, QVariant()).isEmpty());
    }

    void filteredSearchDoesNotShareBaseFilter()
    {
        // The navigation tree keeps saved and organization searches and deletes the copy made for
        // the search box; that copy must not own (and so free) the base search's filter
        QueryFilter* baseFilter = new BoolQuery(XenSearch::PropertyNames::in_any_appliance, true);
        Search base(new Query(new QueryScope(XenSearch::ObjectTypes::AllIncFolders), baseFilter), new VAppGrouping(nullptr), "", "", false);

        Search* filtered = base.AddFullTextFilter("web");
        QVERIFY(filtered != &base);
        QCOMPARE(filtered->GetGrouping(), base.GetGrouping()); // shared, not owned by the copy
        GroupQuery* combined = dynamic_cast<GroupQuery*>(filtered->GetQuery()->getQueryFilter());
        QVERIFY(combined);
        QCOMPARE(combined->getSubQueries().size(), 2);
        QVERIFY(combined->getSubQueries().at(0) != baseFilter);
        QVERIFY(combined->getSubQueries().at(0)->Equals(baseFilter));
        delete filtered;

        QCOMPARE(base.GetQuery()->getQueryFilter(), baseFilter);
        QVERIFY(base.GetGrouping());
    }

    void searchManagerListsBuiltInAndPoolSearches()
    {
        SearchManager* manager = SearchManager::instance();
        QSignalSpy changed(manager, &SearchManager::SearchesChanged);

        // C#'s seven built-in searches
        int builtIn = 0;
        for (const SearchManager::Entry& entry : manager->GetSearches())
            builtIn += entry.isDefault ? 1 : 0;
        QCOMPARE(builtIn, 7);
        std::unique_ptr<Search> byOs(manager->CreateSearch("dead-beef-1234-vmsbyos"));
        QVERIFY(byOs);
        QVERIFY(byOs->IsDefaultSearch());
        QVERIFY(dynamic_cast<PropertyGrouping*>(byOs->GetGrouping()));

        // A search saved on a pool appears once its connection is known
        const QString xml = "<Search uuid=\"abc\" name=\"Mine\" major_version=\"2\" minor_version=\"0\">"
                            "<Query><QueryScope><VM /></QueryScope></Query><sorting /></Search>";
        XenConnection* connection = new XenConnection(nullptr);
        connection->GetCache()->Update(XenObjectType::Pool, "OpaqueRef:p", QVariantMap{
            {"name_label", "pool"}, {"gui_config", QVariantMap{{SearchManager::GuiConfigKey("abc"), xml}}}});
        Xen::ConnectionsManager::instance()->AddConnection(connection);
        QTRY_VERIFY(manager->Contains("abc"));
        QVERIFY(changed.count() >= 1);
        QCOMPARE(manager->ConnectionsHolding("abc"), QList<XenConnection*>({connection}));
        std::unique_ptr<Search> mine(manager->CreateSearch("abc"));
        QVERIFY(mine);
        QCOMPARE(mine->GetName(), QString("Mine"));
        QCOMPARE(mine->GetConnection(), connection);
        QVERIFY(!mine->IsDefaultSearch());

        // ... and goes when the pool no longer has it
        connection->GetCache()->Update(XenObjectType::Pool, "OpaqueRef:p", QVariantMap{
            {"name_label", "pool"}, {"gui_config", QVariantMap()}});
        QTRY_VERIFY(!manager->Contains("abc"));

        Xen::ConnectionsManager::instance()->RemoveConnection(connection);
        delete connection;
    }

    void savedSearchesReadAndWriteXenCenterXml()
    {
        // As C# SearchToXML stores it in a pool's gui_config ("xensearch.Search-<uuid>")
        const QString xml =
            "<Search uuid=\"1234\" name=\"Running web VMs\" major_version=\"2\" minor_version=\"0\">"
            "<Query><QueryScope><VM /><Snapshot /></QueryScope>"
            "<GroupQuery type=\"And\">"
            "<EnumPropertyQuery property=\"power_state\" equals=\"yes\" query=\"Running\" />"
            "<EnumPropertyQuery property=\"virtualisation_status\" equals=\"no\" query=\"IoDriversInstalled, ManagementInstalled\" />"
            "<StringPropertyQuery property=\"label\" casesensitive=\"no\" query=\"web\" type=\"startswith\" />"
            "<RecursiveXMOPropertyQuery property=\"pool\"><StringPropertyQuery property=\"uuid\" casesensitive=\"no\" query=\"pool-uuid\" type=\"exactmatch\" /></RecursiveXMOPropertyQuery>"
            "<DatePropertyQuery property=\"start_time\" query=\"20260301\" type=\"lastweek\" />"
            "<IntPropertyQuery property=\"memory\" query=\"1073741824\" type=\"gt\" />"
            "<BooleanQuery property=\"read_caching_enabled\" query=\"yes\" />"
            "<StringListContainsQuery property=\"tags\" contains=\"yes\" query=\"production\" />"
            "<CustomFieldQuery query=\"alice\" type=\"exactmatch\"><CustomFieldDefinition name=\"owner\" type=\"String\" defaultValue=\"\" /></CustomFieldQuery>"
            "</GroupQuery></Query>"
            "<XenModelObjectPropertyGrouping property=\"pool\"><PropertyGrouping property=\"os_name\" /></XenModelObjectPropertyGrouping>"
            "<columns><column name=\"name\" width=\"200\" /><column name=\"state\" width=\"10\" /></columns>"
            "<sorting><sort column=\"name\" ascending=\"yes\" /></sorting>"
            "</Search>";

        std::unique_ptr<Search> search(SearchMarshalling::LoadSearch(xml));
        QVERIFY(search);
        QCOMPARE(search->GetName(), QString("Running web VMs"));
        QCOMPARE(search->GetUUID(), QString("1234"));
        QCOMPARE(search->GetQuery()->getQueryScope()->getObjectTypes(),
                 XenSearch::ObjectTypes::VM | XenSearch::ObjectTypes::Snapshot);
        auto* group = dynamic_cast<GroupQuery*>(search->GetQuery()->getQueryFilter());
        QVERIFY(group);
        QCOMPARE(group->getSubQueries().size(), 9);
        auto* tools = dynamic_cast<EnumQuery*>(group->getSubQueries().at(1));
        QVERIFY(tools && tools->isNegated());
        QCOMPARE(tools->getValue(), QString("IoDriversInstalled, ManagementInstalled"));
        QVERIFY(dynamic_cast<RecursiveXMOPropertyQuery*>(group->getSubQueries().at(3)));
        auto* date = dynamic_cast<DateQuery*>(group->getSubQueries().at(4));
        QVERIFY(date);
        QCOMPARE(date->getComparisonType(), DateQuery::ComparisonType::LastWeek);
        QVERIFY(dynamic_cast<TagQuery*>(group->getSubQueries().at(7)));
        auto* owner = dynamic_cast<CustomFieldQuery*>(group->getSubQueries().at(8));
        QVERIFY(owner);
        QCOMPARE(owner->getFieldName(), QString("owner"));
        QVERIFY(dynamic_cast<PoolGrouping*>(search->GetGrouping()));
        auto* byOs = dynamic_cast<PropertyGrouping*>(search->GetGrouping()->subgrouping());
        QVERIFY(byOs);
        QCOMPARE(byOs->property(), XenSearch::PropertyNames::os_name);
        QCOMPARE(search->GetColumns().size(), 1); // the obsolete "state" column is dropped (CA-25116)
        QCOMPARE(search->GetSorting().size(), 1);

        // Writing and reading again gives the same search
        const QString written = SearchMarshalling::SearchToXml(search.get());
        QVERIFY(written.startsWith("<Search "));
        QVERIFY(written.contains("type=\"lastweek\""));
        QVERIFY(written.contains("<XenModelObjectPropertyGrouping property=\"pool\">"));
        std::unique_ptr<Search> again(SearchMarshalling::LoadSearch(written));
        QVERIFY(again);
        QVERIFY(again->GetQuery()->getQueryFilter()->Equals(search->GetQuery()->getQueryFilter()));
        QCOMPARE(SearchMarshalling::SearchToXml(again.get()), written);

        // A file as C# SearchesToXML writes it (namespace, doctype); placeholders are not saved
        std::unique_ptr<Search> withPlaceholder(new Search(new Query(new QueryScope(XenSearch::ObjectTypes::Server),
            new GroupQuery(GroupQuery::GroupQueryType::Or, {new DummyQuery(),
                new StringPropertyQuery(XenSearch::PropertyNames::label, "x", StringPropertyQuery::MatchType::Contains)})),
            nullptr, "servers", "5678"));
        const QString file = SearchMarshalling::SearchesToXml({search.get(), withPlaceholder.get()});
        QVERIFY(file.startsWith("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE xensearch"));
        QVERIFY(file.contains("xmlns=\"http://www.citrix.com/XenCenter/XenSearch/schema\""));
        QVERIFY(!file.contains("DummyQuery"));
        bool ok = false;
        QList<Search*> loaded = SearchMarshalling::LoadSearches(file, &ok);
        QVERIFY(ok);
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded.at(1)->GetName(), QString("servers"));
        qDeleteAll(loaded);

        // Unknown future versions are refused, as C# does
        QVERIFY(!SearchMarshalling::LoadSearch("<Search uuid=\"u\" name=\"n\" major_version=\"3\" minor_version=\"0\"><Query><QueryScope><VM/></QueryScope></Query></Search>"));
        QVERIFY(!SearchMarshalling::LoadSearch("null"));
    }

    void conditionsReadTypedPropertiesLikeCSharp()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        const QDateTime started = QDateTime::currentDateTimeUtc().addDays(-1);
        cache->Update(XenObjectType::VMGuestMetrics, "OpaqueRef:gm", QVariantMap{
            {"os_version", QVariantMap{{"name", "Debian 12"}}}});
        cache->Update(XenObjectType::VMMetrics, "OpaqueRef:m", QVariantMap{{"start_time", started.toString("yyyyMMdd'T'HH:mm:ss'Z'")}});
        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{{"uuid", "vm-uuid"}, {"name_label", "web"},
            {"power_state", "Running"}, {"guest_metrics", "OpaqueRef:gm"}, {"metrics", "OpaqueRef:m"}});
        const QVariantMap vm = cache->ResolveObjectData(XenObjectType::VM, "OpaqueRef:vm");
        auto match = [&](const QueryFilter& q) { return q.Match(vm, "vm", &connection); };

        // Enum conditions carry the C# names
        QCOMPARE(match(EnumQuery(XenSearch::PropertyNames::power_state, "Running")).toBool(), true);
        QCOMPARE(match(EnumQuery(XenSearch::PropertyNames::power_state, "Halted")).toBool(), false);
        QCOMPARE(XenSearch::PropertyAccessors::EnumName(XenSearch::PropertyNames::virtualisation_status, 12),
                 QString("IoDriversInstalled, ManagementInstalled"));
        QCOMPARE(XenSearch::PropertyAccessors::EnumName(XenSearch::PropertyNames::ha_restart_priority, "best-effort"),
                 QString("BestEffort"));

        // Values that live on related records (guest metrics, metrics)
        QCOMPARE(match(ValuePropertyQuery(XenSearch::PropertyNames::os_name, "Debian 12", true)).toBool(), true);
        QCOMPARE(match(DateQuery(XenSearch::PropertyNames::start_time, QDateTime(), DateQuery::ComparisonType::Yesterday)).toBool(), true);
        QCOMPARE(match(DateQuery(XenSearch::PropertyNames::start_time, QDateTime(), DateQuery::ComparisonType::Today)).toBool(), false);
        QCOMPARE(match(DateQuery(XenSearch::PropertyNames::start_time, QDateTime(), DateQuery::ComparisonType::ThisWeek)).toBool(), true);

        // C# MatchDate: before and after include the day itself
        const QDateTime noon(QDate(2026, 5, 4), QTime(12, 0));
        QVERIFY(DateQuery::MatchDate(noon, noon, DateQuery::ComparisonType::Before));
        QVERIFY(DateQuery::MatchDate(noon, noon, DateQuery::ComparisonType::After));
        QVERIFY(DateQuery::MatchDate(noon.addDays(-8), QDateTime(), DateQuery::ComparisonType::LastWeek, noon));
        QVERIFY(!DateQuery::MatchDate(noon.addDays(-8), QDateTime(), DateQuery::ComparisonType::ThisWeek, noon));
    }

    void genericGroupingsGroupLikeCSharp()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::Network, "OpaqueRef:net", QVariantMap{{"uuid", "net-uuid"}, {"name_label", "LAN"}});
        cache->Update(XenObjectType::VIF, "OpaqueRef:vif", QVariantMap{{"VM", "OpaqueRef:vm"}, {"network", "OpaqueRef:net"}});
        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{{"uuid", "vm-uuid"}, {"name_label", "web"},
            {"power_state", "Running"}, {"VIFs", QVariantList{"OpaqueRef:vif"}},
            {"other_config", QVariantMap{{"XenCenter.CustomFields.owner", "alice"}}}});
        const QVariantMap vm = cache->ResolveObjectData(XenObjectType::VM, "OpaqueRef:vm");
        const QVariantMap network = cache->ResolveObjectData(XenObjectType::Network, "OpaqueRef:net");

        PropertyGrouping byPowerState(XenSearch::PropertyNames::power_state);
        byPowerState.SetConnection(&connection);
        QCOMPARE(byPowerState.getGroup(vm, "vm").toString(), QString("Running"));
        QCOMPARE(byPowerState.getGroupingName(), QString("Power State"));

        PropertyGrouping byTools(XenSearch::PropertyNames::virtualisation_status);
        QCOMPARE(byTools.getGroupName(1), QString("Unknown"));
        QCOMPARE(byTools.getGroupName(2), QString("Out of date"));
        QCOMPARE(byTools.getGroupName(12), QString("Fully optimized"));

        BoolGrouping byCaching(XenSearch::PropertyNames::read_caching_enabled);
        QCOMPARE(byCaching.getGroupName(true), QString("Read Caching Enabled"));
        QVERIFY(byCaching.getGroupName(false) != byCaching.getGroupName(true));

        XenModelObjectPropertyGrouping byNetwork(XenSearch::PropertyNames::networks);
        byNetwork.SetConnection(&connection);
        QCOMPARE(byNetwork.getGroup(vm, "vm").toList(), QVariantList({"OpaqueRef:net"}));
        QCOMPARE(byNetwork.getGroupName("OpaqueRef:net"), QString("LAN"));
        QVERIFY(byNetwork.belongsAsGroupNotMember(network, "network"));
        QVERIFY(!byNetwork.belongsAsGroupNotMember(vm, "vm"));
        std::unique_ptr<QueryFilter> onNetwork(byNetwork.getSubquery(QVariant(), "OpaqueRef:net"));
        QVERIFY(onNetwork);
        QCOMPARE(onNetwork->Match(vm, "vm", &connection).toBool(), true);

        CustomFieldGrouping byOwner(CustomFieldDefinition("owner", CustomFieldDefinition::Type::String));
        QCOMPARE(byOwner.getGroup(vm, "vm").toString(), QString("alice"));
        AllCustomFieldsGrouping byFields;
        QCOMPARE(byFields.getGroup(vm, "vm").toList(), QVariantList({"owner"}));

        XenModelObjectPropertyGrouping sameNetwork(XenSearch::PropertyNames::networks);
        QVERIFY(byNetwork.equals(&sameNetwork));
        QVERIFY(!byPowerState.equals(&byCaching));
    }

    void customFieldQueriesMatchXenCenterValues()
    {
        // XenCenter writes dates in UTC with DateTime.ToString("u")
        const QVariantMap vm{{"other_config", QVariantMap{
            {"XenCenter.CustomFields.owner", "Alice"},
            {"XenCenter.CustomFields.expires", "2026-03-15 10:30:00Z"},
            {"XenCenter.CustomFields.blank", ""}}}};

        CustomFieldQuery owner("owner", "ali", StringPropertyQuery::MatchType::StartsWith);
        QCOMPARE(owner.Match(vm, "vm", nullptr).toBool(), true);
        CustomFieldQuery other("owner", "bob", StringPropertyQuery::MatchType::Contains);
        QCOMPARE(other.Match(vm, "vm", nullptr).toBool(), false);
        // An empty value counts as no value, even for a "has this field" query
        CustomFieldQuery blank("blank", QString(), StringPropertyQuery::MatchType::Contains);
        QCOMPARE(blank.Match(vm, "vm", nullptr).toBool(), false);

        const QDateTime stored(QDate(2026, 3, 15), QTime(10, 30), Qt::UTC);
        CustomFieldDateQuery before("expires", stored.addDays(1), DateQuery::ComparisonType::Before);
        QCOMPARE(before.Match(vm, "vm", nullptr).toBool(), true);
        CustomFieldDateQuery after("expires", stored.addDays(1), DateQuery::ComparisonType::After);
        QCOMPARE(after.Match(vm, "vm", nullptr).toBool(), false);
        CustomFieldDateQuery sameDay("expires", stored.toLocalTime(), DateQuery::ComparisonType::Exact);
        QCOMPARE(sameDay.Match(vm, "vm", nullptr).toBool(), true);
    }

    void groupAndRecursiveQueriesMatchLikeCSharp()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        cache->Update(XenObjectType::Pool, "OpaqueRef:p", QVariantMap{{"uuid", "pool-uuid"}, {"name_label", "pool"}});
        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{{"uuid", "vm-uuid"}, {"name_label", "web01"},
            {"power_state", "Running"}});
        const QVariantMap vm = cache->ResolveObjectData(XenObjectType::VM, "OpaqueRef:vm");

        auto named = [](const QString& text) {
            return new StringPropertyQuery(XenSearch::PropertyNames::label, text, StringPropertyQuery::MatchType::Contains);
        };

        // An unfinished "Select a filter..." child must not decide a group (C# three-valued logic)
        GroupQuery andWithPlaceholder(GroupQuery::GroupQueryType::And, {named("web"), new DummyQuery()});
        QCOMPARE(andWithPlaceholder.Match(vm, "vm", &connection).toBool(), true);
        GroupQuery andFails(GroupQuery::GroupQueryType::And, {named("db"), new DummyQuery()});
        QCOMPARE(andFails.Match(vm, "vm", &connection).toBool(), false);
        GroupQuery onlyPlaceholder(GroupQuery::GroupQueryType::Or, {new DummyQuery()});
        QVERIFY(!onlyPlaceholder.Match(vm, "vm", &connection).isValid());
        GroupQuery nor(GroupQuery::GroupQueryType::Nor, {named("db"), new DummyQuery()});
        QCOMPARE(nor.Match(vm, "vm", &connection).toBool(), true);

        // "Pool is <pool>" as the editor builds it: a recursive query over the pool with a UUID subquery
        RecursiveXMOPropertyQuery inPool(XenSearch::PropertyNames::pool,
            new StringPropertyQuery(XenSearch::PropertyNames::uuid, "pool-uuid", StringPropertyQuery::MatchType::ExactMatch));
        QCOMPARE(inPool.Match(vm, "vm", &connection).toBool(), true);
        RecursiveXMOPropertyQuery otherPool(XenSearch::PropertyNames::pool,
            new StringPropertyQuery(XenSearch::PropertyNames::uuid, "other", StringPropertyQuery::MatchType::ExactMatch));
        QCOMPARE(otherPool.Match(vm, "vm", &connection).toBool(), false);

        // List relationships: a server's VMs, a VM's disks, storage and networks
        cache->Update(XenObjectType::Host, "OpaqueRef:h", QVariantMap{{"uuid", "host-uuid"}, {"name_label", "xcp1"},
            {"resident_VMs", QVariantList{"OpaqueRef:vm"}}});
        cache->Update(XenObjectType::SR, "OpaqueRef:sr", QVariantMap{{"uuid", "sr-uuid"}, {"name_label", "nfs"}});
        cache->Update(XenObjectType::VDI, "OpaqueRef:vdi", QVariantMap{{"uuid", "vdi-uuid"}, {"name_label", "web-disk"},
            {"SR", "OpaqueRef:sr"}, {"VBDs", QVariantList{"OpaqueRef:vbd"}}});
        cache->Update(XenObjectType::VBD, "OpaqueRef:vbd", QVariantMap{{"VM", "OpaqueRef:vm"}, {"VDI", "OpaqueRef:vdi"}});
        cache->Update(XenObjectType::Network, "OpaqueRef:net", QVariantMap{{"uuid", "net-uuid"}, {"name_label", "lan"}});
        cache->Update(XenObjectType::VIF, "OpaqueRef:vif", QVariantMap{{"VM", "OpaqueRef:vm"}, {"network", "OpaqueRef:net"}});
        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{{"uuid", "vm-uuid"}, {"name_label", "web01"},
            {"power_state", "Running"}, {"resident_on", "OpaqueRef:h"},
            {"VBDs", QVariantList{"OpaqueRef:vbd"}}, {"VIFs", QVariantList{"OpaqueRef:vif"}}});
        const QVariantMap host = cache->ResolveObjectData(XenObjectType::Host, "OpaqueRef:h");
        const QVariantMap vmNow = cache->ResolveObjectData(XenObjectType::VM, "OpaqueRef:vm");

        RecursiveXMOListPropertyQuery hostHasWebVm(XenSearch::PropertyNames::vm, named("web"));
        QCOMPARE(hostHasWebVm.Match(host, "host", &connection).toBool(), true);
        RecursiveXMOListPropertyQuery hostHasDbVm(XenSearch::PropertyNames::vm, named("db"));
        QCOMPARE(hostHasDbVm.Match(host, "host", &connection).toBool(), false);

        RecursiveXMOListPropertyQuery vmDisk(XenSearch::PropertyNames::disks, named("web-disk"));
        QCOMPARE(vmDisk.Match(vmNow, "vm", &connection).toBool(), true);
        RecursiveXMOListPropertyQuery vmStorage(XenSearch::PropertyNames::storage, named("nfs"));
        QCOMPARE(vmStorage.Match(vmNow, "vm", &connection).toBool(), true);
        RecursiveXMOListPropertyQuery vmNetwork(XenSearch::PropertyNames::networks, named("lan"));
        QCOMPARE(vmNetwork.Match(vmNow, "vm", &connection).toBool(), true);

        // A nested "Select a filter..." keeps the result indeterminate instead of hiding everything
        RecursiveXMOListPropertyQuery unfinished(XenSearch::PropertyNames::vm, new DummyQuery());
        QVERIFY(!unfinished.Match(host, "host", &connection).isValid());
    }

    void cacheBuildsEveryKnownType()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();

        // Every type the cache lists (cache explorer) must resolve to an object of that type
        const QStringList known = cache->GetKnownTypes();
        QVERIFY(known.size() >= 48);
        for (const QString& typeName : known)
        {
            const XenObjectType type = XenObject::TypeFromString(typeName);
            QVERIFY2(type != XenObjectType::Null, qPrintable(typeName));
            const QString ref = "OpaqueRef:" + typeName;
            cache->Update(type, ref, QVariantMap{{"uuid", typeName}});
            QSharedPointer<XenObject> object = cache->ResolveObject(type, ref);
            QVERIFY2(object, qPrintable(typeName));
            QCOMPARE(object->GetObjectType(), type);
        }

        // Server classes that used to be missing, e.g. vApps
        cache->Update(XenObjectType::VMAppliance, "OpaqueRef:app", QVariantMap{{"name_label", "app"}});
        QSharedPointer<VMAppliance> appliance = cache->ResolveObject<VMAppliance>("OpaqueRef:app");
        QVERIFY(appliance);
        QCOMPARE(appliance->GetName(), QString("app"));
    }

    void installToolsFindsToolsIsoOnIsoSrs()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        QVERIFY(!InstallPVToolsAction::FindToolsIso(&connection));

        // A disk of that name on a non-ISO SR is not the tools ISO
        cache->Update(XenObjectType::SR, "OpaqueRef:local", QVariantMap{{"content_type", "user"}, {"VDIs", QVariantList{"OpaqueRef:fake"}}});
        cache->Update(XenObjectType::VDI, "OpaqueRef:fake", QVariantMap{{"name_label", "guest-tools.iso"}, {"SR", "OpaqueRef:local"}});
        QVERIFY(!InstallPVToolsAction::FindToolsIso(&connection));

        // XCP-ng: guest-tools.iso on the tools ISO SR
        cache->Update(XenObjectType::SR, "OpaqueRef:tools", QVariantMap{{"content_type", "iso"}, {"name_label", "XCP-ng Tools"},
                                                                       {"VDIs", QVariantList{"OpaqueRef:other", "OpaqueRef:iso"}}});
        cache->Update(XenObjectType::VDI, "OpaqueRef:other", QVariantMap{{"name_label", "debian.iso"}, {"SR", "OpaqueRef:tools"}});
        cache->Update(XenObjectType::VDI, "OpaqueRef:iso", QVariantMap{{"name_label", "guest-tools.iso"}, {"SR", "OpaqueRef:tools"}});
        QSharedPointer<VDI> iso = InstallPVToolsAction::FindToolsIso(&connection);
        QVERIFY(iso);
        QCOMPARE(iso->OpaqueRef(), QString("OpaqueRef:iso"));
    }

    void vtpmRulesMatchCSharp()
    {
        XenConnection connection;
        XenCache* cache = connection.GetCache();
        auto setXapi = [&](const QString& version)
        {
            cache->Update(XenObjectType::Host, "OpaqueRef:h", QVariantMap{{"software_version", QVariantMap{{"xapi", version}}}});
            cache->Update(XenObjectType::Pool, "OpaqueRef:p", QVariantMap{{"master", "OpaqueRef:h"}});
        };

        // Before xapi 23.11: halted, below the limit of one vTPM
        setXapi("22.27.0");
        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{{"power_state", "Halted"}, {"VTPMs", QVariantList()}});
        QSharedPointer<VM> vm = cache->ResolveObject<VM>("OpaqueRef:vm");
        QString reason;
        QVERIFY(vm->CanAddVtpm(&reason));
        QVERIFY(reason.isEmpty());
        QVERIFY(vm->CanRemoveVtpm());

        cache->Update(XenObjectType::VTPM, "OpaqueRef:t", QVariantMap{{"VM", "OpaqueRef:vm"}});
        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{{"power_state", "Halted"}, {"VTPMs", QVariantList{"OpaqueRef:t"}}});
        QVERIFY(!vm->CanAddVtpm(&reason));
        QVERIFY(reason.contains("maximum number of vTPMs"));

        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{{"power_state", "Running"}, {"VTPMs", QVariantList()}});
        QVERIFY(!vm->CanAddVtpm(&reason));
        QVERIFY(reason.contains("shut down"));
        QVERIFY(!vm->CanRemoveVtpm(&reason));

        // xapi 23.11+: allowed_operations decides, even for a running VM
        setXapi("23.11.0");
        QVERIFY(!vm->CanAddVtpm(&reason));
        QVERIFY(reason.contains("not allowed"));
        cache->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{
            {"power_state", "Running"}, {"VTPMs", QVariantList()}, {"allowed_operations", QVariantList{"create_vtpm"}}});
        QVERIFY(vm->CanAddVtpm(&reason));
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
