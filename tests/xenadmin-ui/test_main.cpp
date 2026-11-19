#include <QtTest>
#include "xenlib/xencache.h"
#include "xenlib/xen/vm.h"
#include "xenlib/xen/network/connection.h"
#include "iconmanager.h"
#include "dialogs/savesearchdialog.h"
#include "settingspanels/perfmonalertoptionspage.h"
#include "settingspanels/customfieldsdisplaypage.h"
#include "dialogs/newcustomfielddialog.h"
#include <QDateTimeEdit>
#include "controls/xensearch/queryelement.h"
#include "controls/xensearch/resourceselectbutton.h"
#include "controls/xensearch/searcher.h"
#include "controls/xensearch/groupingcontrol.h"
#include "xenlib/xensearch/grouping.h"
#include "xenlib/xensearch/queries.h"
#include "xenlib/xensearch/query.h"
#include "xenlib/xensearch/queryscope.h"
#include "xenlib/xensearch/search.h"
#include "xenlib/xen/network/connectionsmanager.h"
#include "xenlib/otherconfig/otherconfigandtagswatcher.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <memory>

using namespace XenSearch;

class XenAdminUiTests : public QObject
{
    Q_OBJECT

    // Search editor helpers

    static int subRows(QueryElement* e) { return e->findChildren<QueryElement*>(QString(), Qt::FindDirectChildrenOnly).size(); }

    static void choose(QueryElement* e, const QString& typeName)
    {
        QComboBox* combo = e->findChildren<QComboBox*>(QString(), Qt::FindDirectChildrenOnly).first();
        const int index = combo->findText(typeName);
        QVERIFY2(index >= 0, qPrintable(typeName));
        combo->setCurrentIndex(index);
        emit combo->activated(index);
    }

    static QueryElement* topElement(Searcher* searcher)
    {
        return searcher->findChild<QueryElement*>();
    }

    static QStringList offered(QueryElement* e)
    {
        QComboBox* combo = e->findChildren<QComboBox*>(QString(), Qt::FindDirectChildrenOnly).first();
        QStringList items;
        for (int i = 0; i < combo->count(); ++i)
            items << combo->itemText(i);
        return items;
    }

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

    // Search editor (C# QueryElement)
    void nestedQueryRoundTrips()
    {
        QueryElement element;
        RecursiveXMOPropertyQuery query(PropertyNames::pool,
            new StringPropertyQuery(PropertyNames::uuid, "pool-uuid", StringPropertyQuery::MatchType::ExactMatch));
        element.SetQueryFilter(&query);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        std::unique_ptr<QueryFilter> back(element.GetQueryFilter());
        QVERIFY(back);
        QVERIFY(back->Equals(&query));
        QCOMPARE(subRows(&element), 1);
    }

    void groupKeepsPreviousConditionAndOffersAnother()
    {
        QueryElement element;
        choose(&element, "Name");
        element.findChildren<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly).first()->setText("web");

        choose(&element, "All of the following:");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(subRows(&element), 2); // the Name condition and a "Select a filter..." row

        std::unique_ptr<QueryFilter> filter(element.GetQueryFilter());
        auto* group = dynamic_cast<GroupQuery*>(filter.get());
        QVERIFY(group);
        QCOMPARE(group->getSubQueries().size(), 2);
        auto* name = dynamic_cast<StringPropertyQuery*>(group->getSubQueries().first());
        QVERIFY(name);
        QCOMPARE(name->getQuery(), QString("web"));
        QVERIFY(dynamic_cast<DummyQuery*>(group->getSubQueries().last()));

        // Using the "Select a filter..." row adds a new one
        QueryElement* placeholder = element.findChildren<QueryElement*>(QString(), Qt::FindDirectChildrenOnly).last();
        choose(placeholder, "Description");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(subRows(&element), 3);

        // Removing a condition
        QueryElement* first = element.findChildren<QueryElement*>(QString(), Qt::FindDirectChildrenOnly).first();
        QPushButton* remove = first->findChild<QPushButton*>("removeButton", Qt::FindDirectChildrenOnly);
        QVERIFY(remove->isVisibleTo(first));
        remove->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(subRows(&element), 2);
    }

    void groupLoadsWithRows()
    {
        QueryElement element;
        GroupQuery query(GroupQuery::GroupQueryType::Or, {
            new StringPropertyQuery(PropertyNames::label, "a", StringPropertyQuery::MatchType::Contains),
            new RecursiveXMOListPropertyQuery(PropertyNames::vm,
                new StringPropertyQuery(PropertyNames::label, "db", StringPropertyQuery::MatchType::StartsWith))});
        element.SetQueryFilter(&query);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(subRows(&element), 3); // both conditions and a "Select a filter..." row

        std::unique_ptr<QueryFilter> back(element.GetQueryFilter());
        auto* group = dynamic_cast<GroupQuery*>(back.get());
        QVERIFY(group);
        QCOMPARE(group->getSubQueries().size(), 3);
        QVERIFY(group->getSubQueries().at(0)->Equals(query.getSubQueries().at(0)));
        QVERIFY(group->getSubQueries().at(1)->Equals(query.getSubQueries().at(1)));
    }

    void choosingNestedTypeGivesSubRow()
    {
        QueryElement element;
        choose(&element, "Pool");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(subRows(&element), 1);
        QueryElement* sub = element.findChildren<QueryElement*>(QString(), Qt::FindDirectChildrenOnly).first();
        QComboBox* subTypes = sub->findChildren<QComboBox*>(QString(), Qt::FindDirectChildrenOnly).first();
        // The sub-row offers only single conditions for pools, including picking one ("Is")
        QVERIFY(subTypes->findText("Is") >= 0);
        QVERIFY(subTypes->findText("All of the following:") < 0);
        QVERIFY(subTypes->findText("Pool") < 0);
        // The sub-row has no remove button of its own
        QVERIFY(!sub->findChild<QPushButton*>("removeButton", Qt::FindDirectChildrenOnly)->isVisibleTo(sub));

        std::unique_ptr<QueryFilter> filter(element.GetQueryFilter());
        auto* recursive = dynamic_cast<RecursiveXMOPropertyQuery*>(filter.get());
        QVERIFY(recursive);
        QVERIFY(dynamic_cast<DummyQuery*>(recursive->getSubQuery()));
    }



    void offersConditionsForWhatIsSearchedFor()
    {
        Searcher searcher;
        searcher.SetSearch(new Search(new Query(new QueryScope(ObjectTypes::VM), nullptr), nullptr, "vms"));
        QStringList items = offered(topElement(&searcher));
        QVERIFY(items.contains("Name"));
        QVERIFY(items.contains("Pool"));
        QVERIFY(items.contains("Server"));
        QVERIFY(!items.contains("Is")); // picking one object only inside a parent/child row

        searcher.SetSearch(new Search(new Query(new QueryScope(ObjectTypes::Pool), nullptr), nullptr, "pools"));
        items = offered(topElement(&searcher));
        QVERIFY(items.contains("Name"));
        QVERIFY(!items.contains("Pool"));   // constraining pools by their pool is pointless
        QVERIFY(!items.contains("Server")); // C# excludes pools from the Server relation
        QVERIFY(items.contains("VM"));
    }

    void changingSearchForDropsConditionsThatNoLongerApply()
    {
        Searcher searcher;
        GroupQuery* group = new GroupQuery(GroupQuery::GroupQueryType::And, {
            new StringPropertyQuery(PropertyNames::label, "a", StringPropertyQuery::MatchType::Contains),
            new RecursiveXMOPropertyQuery(PropertyNames::pool,
                new StringPropertyQuery(PropertyNames::uuid, "pool-uuid", StringPropertyQuery::MatchType::ExactMatch))});
        searcher.SetSearch(new Search(new Query(new QueryScope(ObjectTypes::VM), group), nullptr, "vms"));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        auto* searchFor = searcher.findChild<XenSearch::SearchFor*>();
        QVERIFY(searchFor);
        searchFor->SetQueryScope(new QueryScope(ObjectTypes::Pool));
        emit searchFor->QueryChanged();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        // The Pool condition went; the All of group with one condition left became that condition
        std::unique_ptr<QueryFilter> filter(topElement(&searcher)->GetQueryFilter());
        auto* name = dynamic_cast<StringPropertyQuery*>(filter.get());
        QVERIFY(name);
        QCOMPARE(name->getQuery(), QString("a"));
    }

    void tagConditionOffersTagsInUse()
    {
        XenConnection* connection = new XenConnection(nullptr);
        connection->GetCache()->Update(XenObjectType::VM, "OpaqueRef:vm", QVariantMap{
            {"name_label", "web"}, {"tags", QVariantList{"production", "web"}}});
        connection->GetCache()->Update(XenObjectType::SR, "OpaqueRef:sr", QVariantMap{
            {"name_label", "nfs"}, {"tags", QVariantList{"backup"}}});
        Xen::ConnectionsManager::instance()->AddConnection(connection);
        emit OtherConfigAndTagsWatcher::instance()->TagsChanged();

        QueryElement element;
        choose(&element, "Tags");
        QComboBox* values = element.findChildren<QComboBox*>(QString(), Qt::FindDirectChildrenOnly).at(2);
        QStringList tags;
        for (int i = 0; i < values->count(); ++i)
            tags << values->itemText(i);
        QCOMPARE(tags, QStringList({"backup", "production", "web"}));

        Xen::ConnectionsManager::instance()->RemoveConnection(connection);
        delete connection;
    }

    static QStringList addGroupOffers(GroupingControl* control)
    {
        DropDownButton* add = nullptr;
        for (DropDownButton* button : control->findChildren<DropDownButton*>())
            if (button->text() == "Add Group")
                add = button;
        emit add->GetMenu()->aboutToShow();
        QStringList offers;
        for (QAction* action : add->GetMenu()->actions())
            if (!action->isSeparator())
                offers << action->text();
        return offers;
    }

    static QStringList groupButtons(GroupingControl* control)
    {
        QStringList texts;
        for (DropDownButton* button : control->findChildren<DropDownButton*>())
            if (button->text() != "Add Group" && button->isVisibleTo(control))
                texts << button->text();
        return texts;
    }

    void groupingControlRoundTripsAndFollowsSearchFor()
    {
        Searcher searcher;
        GroupingControl* control = searcher.findChild<GroupingControl*>();
        QVERIFY(control);

        // C#: grouped by pool to start with
        std::unique_ptr<Grouping> initial(control->GetGrouping());
        QVERIFY(dynamic_cast<PoolGrouping*>(initial.get()));

        // A chain shows as buttons and comes back the same
        searcher.SetSearch(new Search(new Query(new QueryScope(ObjectTypes::VM), nullptr), nullptr, "vms"));
        PropertyGrouping* byOs = new PropertyGrouping(PropertyNames::os_name);
        PoolGrouping chain(new HostGrouping(byOs));
        control->SetGrouping(&chain);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(groupButtons(control), QStringList({"Pool", "Server", "Operating System"}));
        std::unique_ptr<Grouping> back(control->GetGrouping());
        QVERIFY(dynamic_cast<PoolGrouping*>(back.get()));
        QVERIFY(dynamic_cast<HostGrouping*>(back->subgrouping()));
        auto* os = dynamic_cast<PropertyGrouping*>(back->subgrouping()->subgrouping());
        QVERIFY(os);
        QCOMPARE(os->property(), PropertyNames::os_name);

        // Offers depend on what is searched for, and exclude groupings in use
        QStringList offers = addGroupOffers(control);
        QVERIFY(offers.contains("Power State"));
        QVERIFY(offers.contains("Network"));
        QVERIFY(!offers.contains("Pool"));
        QVERIFY(!offers.contains("Operating System"));

        auto* searchFor = searcher.findChild<XenSearch::SearchFor*>();
        searchFor->SetQueryScope(new QueryScope(ObjectTypes::Pool));
        emit searchFor->QueryChanged();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        // Groupings that do not apply to pools are dropped
        QVERIFY(!groupButtons(control).contains("Operating System"));
        QVERIFY(!addGroupOffers(control).contains("Power State"));

        // Searching for folders groups by folder
        searchFor->SetQueryScope(new QueryScope(ObjectTypes::Folder));
        emit searchFor->QueryChanged();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(groupButtons(control), QStringList({"Folder"}));
    }

    void pickerListsObjectsAndSelectsByUuid()
    {
        ResourceSelectButton picker;
        // A heading that is not an object adds no item but its contents are still added
        IAcceptGroups* adapter = picker.Add(nullptr, QVariant("Servers"), QString(), QVariantMap(), 0, nullptr);
        QCOMPARE(adapter, static_cast<IAcceptGroups*>(&picker));
        adapter->Add(nullptr, QVariant("OpaqueRef:p"), "pool", QVariantMap{{"name_label", "Prod"}, {"uuid", "pool-uuid"}}, 1, nullptr);
        adapter->Add(nullptr, QVariant("OpaqueRef:q"), "pool", QVariantMap{{"name_label", "Dev"}, {"uuid", "dev-uuid"}}, 1, nullptr);
        QCOMPARE(picker.GetMenu()->actions().size(), 2);

        QSignalSpy spy(&picker, &ResourceSelectButton::itemSelected);
        picker.GetMenu()->actions().at(1)->trigger();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(picker.selectedRef(), QString("OpaqueRef:q"));
        QCOMPARE(picker.selectedUuid(), QString("dev-uuid"));
        QCOMPARE(picker.text(), QString("Dev"));

        picker.setSelectedUuid("pool-uuid");
        QCOMPARE(picker.selectedRef(), QString("OpaqueRef:p"));
        QCOMPARE(picker.text(), QString("Prod"));
    }

    void saveSearchDialogNeedsNameAndConnectedPool()
    {
        // No connected pool to save on (C# CA-40307): Save stays disabled whatever the name
        SaveSearchDialog dialog("  My search  ");
        QDialogButtonBox* buttons = dialog.findChild<QDialogButtonBox*>();
        QVERIFY(buttons);
        QCOMPARE(dialog.GetName(), QString("My search"));
        QVERIFY(!dialog.GetConnection());
        QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());

        // With somewhere to save, only a blank name keeps it disabled
        QComboBox* location = dialog.findChild<QComboBox*>("locationCombo");
        QVERIFY(location);
        location->addItem("Pool", QVariant::fromValue(quintptr(0)));
        QLineEdit* name = dialog.findChild<QLineEdit*>("nameEdit");
        name->setText("Other");
        QVERIFY(buttons->button(QDialogButtonBox::Save)->isEnabled());
        name->setText("   ");
        QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());
    }

    void newCustomFieldDialogRejectsBlankAndDuplicateNames()
    {
        NewCustomFieldDialog dialog(nullptr);
        dialog.SetExistingFields({CustomFieldDefinition("Owner", CustomFieldDefinition::Type::String)});
        QPushButton* ok = dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        QLineEdit* name = dialog.findChild<QLineEdit*>("nameLineEdit");
        QLabel* duplicate = dialog.findChild<QLabel*>("duplicateLabel");
        QComboBox* type = dialog.findChild<QComboBox*>("typeComboBox");
        QVERIFY(ok && name && duplicate && type);

        QVERIFY(!ok->isEnabled());
        name->setText(" Owner ");
        QVERIFY(!ok->isEnabled());
        QVERIFY(!duplicate->isHidden());
        name->setText("  Expires ");
        type->setCurrentIndex(1);
        QVERIFY(ok->isEnabled());
        QVERIFY(duplicate->isHidden());
        QCOMPARE(dialog.GetDefinition(), CustomFieldDefinition("Expires", CustomFieldDefinition::Type::Date));
    }

    void customFieldsPageShowsPoolDefinitions()
    {
        const CustomFieldDefinition owner("Owner", CustomFieldDefinition::Type::String);
        const CustomFieldDefinition expires("Expires", CustomFieldDefinition::Type::Date);
        CustomFieldsDisplayPage page;
        page.SetXenObject(QSharedPointer<XenObject>(),
                          QVariantMap{{"other_config", QVariantMap{{"XenCenter.CustomFields.Owner", "alice"},
                                                                   {"XenCenter.CustomFields.Stale", "x"}}}},
                          QVariantMap());

        // One row per definition, whatever other keys the object has
        auto textEditors = [&page]()
        {
            QList<QLineEdit*> editors;
            for (QLineEdit* edit : page.findChildren<QLineEdit*>())
            {
                if (!qobject_cast<QDateTimeEdit*>(edit->parentWidget()))
                    editors.append(edit);
            }
            return editors;
        };
        page.SetDefinitions({owner, expires}, true);
        QList<QLineEdit*> texts = textEditors();
        QCOMPARE(texts.size(), 1);
        QCOMPARE(texts.first()->text(), QString("alice"));
        QCOMPARE(page.findChildren<QDateTimeEdit*>().size(), 1);
        QCOMPARE(page.GetSubText(), QString("Owner: alice"));
        QVERIFY(!page.HasChanged());

        texts.first()->setText("bob");
        QVERIFY(page.HasChanged());
        texts.first()->setText("alice");
        QVERIFY(!page.HasChanged());
        page.findChild<QCheckBox*>()->setChecked(true); // give Expires a value
        QVERIFY(page.HasChanged());

        // A field deleted from the pool loses its row
        page.SetDefinitions({expires}, false);
        QVERIFY(textEditors().isEmpty());
        QCOMPARE(page.findChildren<QDateTimeEdit*>().size(), 1);
    }

    void emailNotificationsPageFollowsCheckbox()
    {
        PerfmonAlertOptionsPage page;
        QCheckBox* enable = page.findChild<QCheckBox*>("enableCheckBox");
        QLineEdit* email = page.findChild<QLineEdit*>("emailAddressLineEdit");
        QLineEdit* server = page.findChild<QLineEdit*>("smtpServerLineEdit");
        QLineEdit* port = page.findChild<QLineEdit*>("smtpPortLineEdit");
        QComboBox* language = page.findChild<QComboBox*>("languageComboBox");
        QVERIFY(enable && email && server && port && language);

        // C# defaults: off, port 25, the three mail languages with their codes
        QVERIFY(!enable->isChecked());
        QVERIFY(!email->isEnabled() && !server->isEnabled() && !port->isEnabled() && !language->isEnabled());
        QCOMPARE(port->text(), QString("25"));
        QCOMPARE(language->count(), 3);
        QCOMPARE(language->itemData(0).toString(), QString("en-US"));
        QCOMPARE(page.GetSubText(), QString("None defined"));
        QVERIFY(page.IsValidToSave());

        // Enabled, the fields must be valid before saving
        enable->setChecked(true);
        QVERIFY(email->isEnabled() && server->isEnabled() && port->isEnabled() && language->isEnabled());
        QVERIFY(!page.IsValidToSave());
        email->setText("admin@example.com");
        server->setText("smtp.example.com");
        QVERIFY(page.IsValidToSave());
        QCOMPARE(page.GetSubText(), QString("admin@example.com"));
        port->setText("70000");
        QVERIFY(!page.IsValidToSave());
    }
};

QTEST_MAIN(XenAdminUiTests)
#include "test_main.moc"
