#include "dataeventlist.h"
#include "../../iconmanager.h"
#include <algorithm>
#include <QDateTime>
#include <QObject>

namespace
{
    QIcon iconForLifecycleEvent(const QString& message)
    {
        const QString type = message.trimmed().toUpper();
        auto iconType = IconManager::EventIconType::Unknown;

        if (type == QStringLiteral("VM_STARTED"))
            iconType = IconManager::EventIconType::VmStarted;
        else if (type == QStringLiteral("VM_SHUTDOWN"))
            iconType = IconManager::EventIconType::VmShutdown;
        else if (type == QStringLiteral("VM_REBOOTED"))
            iconType = IconManager::EventIconType::VmRebooted;
        else if (type == QStringLiteral("VM_RESUMED"))
            iconType = IconManager::EventIconType::VmResumed;
        else if (type == QStringLiteral("VM_SUSPENDED"))
            iconType = IconManager::EventIconType::VmSuspended;
        else if (type == QStringLiteral("VM_CRASHED"))
            iconType = IconManager::EventIconType::VmCrashed;
        else if (type == QStringLiteral("VM_CLONED"))
            iconType = IconManager::EventIconType::VmCloned;

        return IconManager::instance().GetEventIcon(iconType);
    }

    QString formatLifecycleTimestamp(qint64 timestampMs)
    {
        if (timestampMs <= 0)
            return QString();

        const QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestampMs, Qt::LocalTime);
        if (!dt.isValid())
            return QString();

        // C# parity: Messages.DATEFORMAT_DMY_HM = "MMM d, yyyy h:mm tt"
        return dt.toString(QStringLiteral("MMM d, yyyy h:mm AP"));
    }

    QString tooltipForLifecycleEvent(const QString& message, const QString& objectName)
    {
        const QString type = message.trimmed().toUpper();
        const QString vmName = objectName.isEmpty() ? QObject::tr("VM") : objectName;

        if (type == QStringLiteral("VM_STARTED"))
            return QObject::tr("VM '%1' started").arg(vmName);
        if (type == QStringLiteral("VM_SHUTDOWN"))
            return QObject::tr("VM '%1' shut down").arg(vmName);
        if (type == QStringLiteral("VM_REBOOTED"))
            return QObject::tr("VM '%1' rebooted").arg(vmName);
        if (type == QStringLiteral("VM_RESUMED"))
            return QObject::tr("VM '%1' resumed").arg(vmName);
        if (type == QStringLiteral("VM_SUSPENDED"))
            return QObject::tr("VM '%1' suspended").arg(vmName);
        if (type == QStringLiteral("VM_CRASHED"))
            return QObject::tr("VM '%1' crashed").arg(vmName);
        if (type == QStringLiteral("VM_CLONED"))
            return QObject::tr("VM '%1' cloned").arg(vmName);

        if (!objectName.isEmpty())
            return QStringLiteral("%1 (%2)").arg(message, objectName);
        return message;
    }
}

namespace CustomDataGraph
{
    DataEventList::DataEventList(QWidget* parent) : QListWidget(parent)
    {
        this->setSelectionMode(QAbstractItemView::SingleSelection);
        this->setUniformItemSizes(true);
    }

    void DataEventList::SetPlotNav(DataPlotNav* plotNav)
    {
        this->m_plotNav = plotNav;
    }

    void DataEventList::AddEvent(const DataEvent& eventItem)
    {
        this->m_events.append(eventItem);
        this->RebuildItems();
    }

    void DataEventList::RemoveEvent(const DataEvent& eventItem)
    {
        const auto it = std::find(this->m_events.begin(), this->m_events.end(), eventItem);
        if (it != this->m_events.end())
            this->m_events.erase(it);

        this->RebuildItems();
    }

    void DataEventList::ClearEvents()
    {
        this->m_events.clear();
        this->clear();
    }

    const QList<DataEvent>& DataEventList::Events() const
    {
        return this->m_events;
    }

    void DataEventList::RebuildItems()
    {
        this->clear();

        QList<DataEvent> sortedEvents = this->m_events;
        std::sort(sortedEvents.begin(), sortedEvents.end(), [](const DataEvent& a, const DataEvent& b)
        {
            return a.TimestampTicks > b.TimestampTicks;
        });

        for (const DataEvent& eventItem : sortedEvents)
        {
            const QString timestampText = formatLifecycleTimestamp(eventItem.TimestampTicks);
            auto* item = new QListWidgetItem(iconForLifecycleEvent(eventItem.Message),
                                             timestampText.isEmpty() ? eventItem.Message : timestampText);
            item->setData(Qt::UserRole, eventItem.TimestampTicks);
            item->setData(Qt::UserRole + 1, eventItem.ObjectUuid);
            item->setToolTip(tooltipForLifecycleEvent(eventItem.Message, eventItem.ObjectName));
            this->addItem(item);
        }
    }
}
