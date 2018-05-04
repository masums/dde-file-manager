/*
 * Copyright (C) 2017 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     zccrs <zccrs@live.com>
 *
 * Maintainer: zccrs <zhangjide@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "dfmsidebar.h"

#include "app/define.h"
#include "views/dfmsidebaritemgroup.h"
#include "views/dfmsidebarbookmarkitem.h"
#include "views/dfmsidebardefaultitem.h"
#include "views/dfmsidebartrashitem.h"
#include "views/dfmsidebartagitem.h"
#include "views/dfmsidebardeviceitem.h"
#include "views/dfilemanagerwindow.h"
#include "views/dfmsidebarnetworkitem.h"
#include "controllers/bookmarkmanager.h"
#include "deviceinfo/udisklistener.h"

#include <DThemeManager>
#include <QVBoxLayout>
#include <QTimer>
#include <QScrollBar>

#include "dabstractfilewatcher.h"
#include "singleton.h"


DWIDGET_USE_NAMESPACE

DFM_BEGIN_NAMESPACE

class DFMSideBarPrivate
{
    Q_DECLARE_PUBLIC(DFMSideBar)

public:
    DFMSideBarPrivate(DFMSideBar *qq);

    DFMSideBar *q_ptr = nullptr;
    QVBoxLayout *mainLayout;
    QWidget *mainLayoutHolder;
    QSet<QString> disabledSchemes;
    QMap<QString, DFMSideBarItemGroup *> groupNameMap;
    DFMSideBarItem *lastCheckedItem = nullptr; //< managed by setCurrentUrl()

private:
    void initUI();
    void initBookmarkConnection();
    void initMountedVolumes();
    void initUserShareItem();
    void initTagsConnection();
    void addItemToGroup(DFMSideBarItemGroup *group, DFMSideBar::GroupName groupType);

    DAbstractFileWatcher *userShareFileWatcher = nullptr; //< managed by initUserShareItem()
};

DFMSideBarPrivate::DFMSideBarPrivate(DFMSideBar *qq)
    : q_ptr(qq)
{
    initUI();
    initBookmarkConnection();
    initMountedVolumes();
    initUserShareItem();
    initTagsConnection();
}

void DFMSideBarPrivate::initUI()
{
    Q_Q(DFMSideBar);
    q->setAcceptDrops(true);
    q->setFocusPolicy(Qt::NoFocus);
    q->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    q->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    q->setViewportMargins(0, 0, -q->verticalScrollBar()->sizeHint().width(), 0);

    // to make QScrollArea scroallable, we need a widget.
    mainLayoutHolder = new QWidget();
    q->setWidget(mainLayoutHolder);

    // our main QVBoxLayout, which hold our `DFMSideBarItemGroup`s
    mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(0);
    mainLayout->setMargin(0);
    mainLayout->setAlignment(Qt::AlignTop);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);
    mainLayoutHolder->setLayout(mainLayout);

    static QList<DFMSideBar::GroupName> groups = {
        DFMSideBar::GroupName::Common,
        DFMSideBar::GroupName::Device,
        DFMSideBar::GroupName::Bookmark,
        DFMSideBar::GroupName::Network,
        DFMSideBar::GroupName::Tag
    };

    foreach (const DFMSideBar::GroupName &groupType, groups) {
        DFMSideBarItemGroup *group = new DFMSideBarItemGroup(q->groupName(groupType));
        addItemToGroup(group, groupType);
        groupNameMap[q->groupName(groupType)] = group;
        mainLayout->addLayout(group);
    }
}

void DFMSideBarPrivate::initBookmarkConnection()
{
    Q_Q(DFMSideBar);

    DFMSideBarItemGroup *group = groupNameMap[q->groupName(DFMSideBar::GroupName::Bookmark)];
    Q_CHECK_PTR(group);

    DAbstractFileWatcher *bookmark_watcher = fileService->createFileWatcher(q_func(), DUrl(BOOKMARK_ROOT), group);
    bookmark_watcher->startWatcher();

    q->connect(bookmark_watcher, &DAbstractFileWatcher::subfileCreated, group, [group](const DUrl & url) {
        group->appendItem(new DFMSideBarBookmarkItem(url));
    });

    q->connect(bookmark_watcher, &DAbstractFileWatcher::fileDeleted, group,
    [this, q](const DUrl & url) {
        DFMSideBarItem *item = q->itemAt(url);
        if (item) {
            q->removeItem(item);
        }
    });

    q->connect(bookmark_watcher, &DAbstractFileWatcher::fileMoved, group,
    [this, group, q](const DUrl & source, const DUrl & target) {
        DFMSideBarItem *item = q->itemAt(source);
        if (item) {
            item->setUrl(target);
        }
    });
}

void DFMSideBarPrivate::initMountedVolumes()
{
    Q_Q(DFMSideBar);

    DFMSideBarItemGroup *group = groupNameMap[q->groupName(DFMSideBar::GroupName::Device)];
    Q_CHECK_PTR(group);

    DAbstractFileWatcher *devices_watcher = fileService->createFileWatcher(q_func(), DUrl(DEVICE_ROOT), group);
    Q_CHECK_PTR(devices_watcher);
    devices_watcher->startWatcher();

    auto devices_info = fileService->getChildren(q_func(), DUrl(DEVICE_ROOT),
                        QStringList(), QDir::AllEntries);
    for (const DAbstractFileInfoPointer &info : devices_info) {
        UDiskDeviceInfoPointer pointer(dynamic_cast<UDiskDeviceInfo *>(info.data()));
        group->appendItem(new DFMSideBarDeviceItem(DUrl::fromDeviceId(pointer->getId())));
    }

    // New device/volume added.
    q->connect(devices_watcher, &DAbstractFileWatcher::subfileCreated, group, [group](const DUrl & url) {
        group->appendItem(new DFMSideBarDeviceItem(url));
    });

    // Device/volume get mounted/unmounted
    q->connect(devices_watcher, &DAbstractFileWatcher::fileAttributeChanged, group, [group](const DUrl & url) {
        DFMSideBarItem *item = group->findItem(url);
        DAbstractFileInfoPointer pointer = fileService->createFileInfo(item, url);
        if (item) {
            DFMSideBarDeviceItem *casted = qobject_cast<DFMSideBarDeviceItem *>(item);
            casted->unmountButton->setVisible(pointer->extensionPropertys().value("isMounted", false).toBool());
        }
    });

    // Device/volume get removed.
    q->connect(devices_watcher, &DAbstractFileWatcher::fileDeleted, group, [group, q](const DUrl & url) {
        DFMSideBarItem *item = group->findItem(url);
        Q_CHECK_PTR(item); // should always find one
        q->removeItem(item);
    });
}

void DFMSideBarPrivate::initUserShareItem()
{
    Q_Q(DFMSideBar);

    using DFM_STD_LOCATION = DFMStandardPaths::StandardLocation;
    DFMSideBarItemGroup *group = groupNameMap[q->groupName(DFMSideBar::GroupName::Network)];

    int count = fileService->getChildren(group, DUrl::fromUserShareFile("/"),
                                         QStringList(), QDir::AllEntries).count();
    if (count) {
        group->appendItem(new DFMSideBarNetworkItem(DFM_STD_LOCATION::UserShareRootPath));
    }

    userShareFileWatcher = fileService->createFileWatcher(q, DUrl::fromUserShareFile("/"), q);
    Q_CHECK_PTR(userShareFileWatcher);
    userShareFileWatcher->startWatcher();

    auto userShareLambda = [ = ]() {
        int cnt = fileService->getChildren(group, DUrl::fromUserShareFile("/"),
                                           QStringList(), QDir::AllEntries).count();
        DFMSideBarItem *item = group->findItem(DUrl::fromUserShareFile("/"));
        if (cnt > 0 && item == nullptr) {
            item = new DFMSideBarNetworkItem(DFM_STD_LOCATION::UserShareRootPath);
            group->appendItem(item);
        } else if (cnt == 0 && item) {
            q->removeItem(item);
            item = nullptr;
        }

        return item;
    };

    q->connect(userShareFileWatcher, &DAbstractFileWatcher::fileDeleted, q, userShareLambda);
    q->connect(userShareFileWatcher, &DAbstractFileWatcher::subfileCreated, q, [ = ]() {
        DFMSideBarItem *item = userShareLambda();
        if (item) {
            item->playAnimation();
        }
    });
}

void DFMSideBarPrivate::initTagsConnection()
{
    Q_Q(DFMSideBar);

    DFMSideBarItemGroup *group = groupNameMap[q->groupName(DFMSideBar::GroupName::Tag)];
    Q_CHECK_PTR(group);

    DAbstractFileWatcher *tags_watcher = fileService->createFileWatcher(q_func(), DUrl(TAG_ROOT), group);
    Q_CHECK_PTR(tags_watcher);
    tags_watcher->startWatcher();

    // New tag added.
    q->connect(tags_watcher, &DAbstractFileWatcher::subfileCreated, group, [group](const DUrl & url) {
        qDebug() << "new tag added 1111111111 "<< url;
        group->appendItem(new DFMSideBarTagItem(url));
    });

    // Tag get removed.
    q->connect(tags_watcher, &DAbstractFileWatcher::fileDeleted, group, [group, q](const DUrl & url) {
        qDebug() << url;
        DFMSideBarItem *item = group->findItem(url);
        Q_CHECK_PTR(item); // should always find one
        q->removeItem(item);
    });

    // Tag got rename
    q->connect(tags_watcher, &DAbstractFileWatcher::fileMoved, group,
    [this, group, q](const DUrl & source, const DUrl & target) {
        DFMSideBarItem *item = q->itemAt(source);
        if (item) {
            item->setUrl(target);
        }
    });

    // Tag changed color
    q->connect(tags_watcher, &DAbstractFileWatcher::fileAttributeChanged, group, [group](const DUrl & url) {
        DFMSideBarItem *item = group->findItem(url);
        item->setIconFromThemeConfig("BookmarkItem." + url.fileName());
    });
}

void DFMSideBarPrivate::addItemToGroup(DFMSideBarItemGroup *group, DFMSideBar::GroupName groupType)
{
    // to make group touch less DFM internal implement, we add item here.
    using DFM_STD_LOCATION = DFMStandardPaths::StandardLocation;

    switch (groupType) {
    case DFMSideBar::GroupName::Common:
        group->appendItem(new DFMSideBarDefaultItem(DFM_STD_LOCATION::HomePath));
        group->appendItem(new DFMSideBarDefaultItem(DFM_STD_LOCATION::DesktopPath));
        group->appendItem(new DFMSideBarDefaultItem(DFM_STD_LOCATION::VideosPath));
        group->appendItem(new DFMSideBarDefaultItem(DFM_STD_LOCATION::MusicPath));
        group->appendItem(new DFMSideBarDefaultItem(DFM_STD_LOCATION::PicturesPath));
        group->appendItem(new DFMSideBarDefaultItem(DFM_STD_LOCATION::DocumentsPath));
        group->appendItem(new DFMSideBarDefaultItem(DFM_STD_LOCATION::DownloadsPath));
        group->appendItem(new DFMSideBarTrashItem());
        break;
    case DFMSideBar::GroupName::Device:
        group->appendItem(new DFMSideBarDefaultItem(DFM_STD_LOCATION::ComputerRootPath));
        group->appendItem(new DFMSideBarDefaultItem(DFM_STD_LOCATION::Root)); // TODO: check dfmPlatformManager->isRoot_hidden()
        break;
    case DFMSideBar::GroupName::Bookmark: {
        auto bookmark_infos = DFileService::instance()->getChildren(q_func(), DUrl(BOOKMARK_ROOT),
                              QStringList(), QDir::AllEntries);
        for (const DAbstractFileInfoPointer &info : bookmark_infos) {
            group->appendItem(new DFMSideBarBookmarkItem(info->fileUrl()));
        }
        break;
    }
    case DFMSideBar::GroupName::Network:
        group->appendItem(new DFMSideBarNetworkItem(DFM_STD_LOCATION::NetworkRootPath));
        break;
    case DFMSideBar::GroupName::Tag: {
        auto tag_infos = DFileService::instance()->getChildren(q_func(), DUrl(TAG_ROOT),
                              QStringList(), QDir::AllEntries);
        for (const DAbstractFileInfoPointer &info : tag_infos) {
            group->appendItem(new DFMSideBarTagItem(info->fileUrl()));
        }
        break;
    }
    default: // make compiler happy
        break;
    }
}

DFMSideBar::DFMSideBar(QWidget *parent)
    : QScrollArea(parent)
    , d_ptr(new DFMSideBarPrivate(this))
{
    DThemeManager::instance()->registerWidget(this);
}

DFMSideBar::~DFMSideBar()
{

}

int DFMSideBar::count() const
{
    Q_D(const DFMSideBar);

    int count = 0;

    QMap<QString, DFMSideBarItemGroup *>::const_iterator i;
    for (i = d->groupNameMap.begin(); i != d->groupNameMap.end(); ++i) {
        count += i.value()->itemCount();
    }

    return count;
}

QStringList DFMSideBar::groupList() const
{
    Q_D(const DFMSideBar);

    QStringList groupNameList;

    QMap<QString, DFMSideBarItemGroup *>::const_iterator i;
    for (i = d->groupNameMap.begin(); i != d->groupNameMap.end(); ++i) {
        groupNameList.append(i.key());
    }

    return groupNameList;
}

void DFMSideBar::setCurrentUrl(const DUrl &url)
{
    Q_D(DFMSideBar);

    if (d->lastCheckedItem) {
        d->lastCheckedItem->setChecked(false);
    }

    DFMSideBarItem *item = itemAt(url);

    if (item) {
        d->lastCheckedItem = item;
        d->lastCheckedItem->setChecked(true);
    }
}

/*!
 * \brief Hide sidebar items by given url \a schemes .
 *
 * Notice that this is for *HIDE* the items, NOT for display a *DISABLED* state.
 */
void DFMSideBar::setDisableUrlSchemes(const QSet<QString> &schemes)
{
    Q_D(DFMSideBar);

    d->disabledSchemes = schemes;

    for (QString &key : d->groupNameMap.keys()) {
        DFMSideBarItemGroup *groupPointer = d->groupNameMap.value(key);

        for (int i = 0; i < groupPointer->itemCount(); ++i) {
            DFMSideBarItem *item = (*groupPointer)[i];
            item->setVisible(!schemes.contains(item->url().scheme()));
        }
    }
}

/*!
 * \brief Add a sidebar item into a sidebar group.
 * \param item The item which is going to be added.
 * \param group The name (in `QString`, all lower case) of the sidebar group.
 * \return the index of the inserted item in that group.
 */
int DFMSideBar::addItem(DFMSideBarItem *item, const QString &group)
{
    Q_D(DFMSideBar);

    int index;

    if (d->groupNameMap.contains(group)) {
        DFMSideBarItemGroup *groupPointer = d->groupNameMap[group];
        index = groupPointer->appendItem(item);
    } else {
        QString groupName = group.isEmpty() ? "" : group;
        DFMSideBarItemGroup *group = new DFMSideBarItemGroup(groupName);
        d->groupNameMap[groupName] = group;
        index = group->appendItem(item);
        d->mainLayout->addLayout(group);
    }

    if (d->disabledSchemes.contains(item->url().scheme())) {
        item->hide();
    }

    return index;
}

/*!
 * \fn void DFMSideBar::insertItem(int index, DFMSideBarItem *item, const QString &groupName)
 * \brief Insert \a item before \a index into \a groupName .
 *
 * Insert a `DFMSideBarItem` \a item to group before \a index into the
 * given \a groupName .
 */
void DFMSideBar::insertItem(int index, DFMSideBarItem *item, const QString &group)
{
    Q_D(DFMSideBar);

    if (d->groupNameMap.contains(group)) {
        DFMSideBarItemGroup *groupPointer = d->groupNameMap[group];
        groupPointer->insertItem(index, item);
    }

    if (d->disabledSchemes.contains(item->url().scheme())) {
        item->hide();
    }
}

void DFMSideBar::removeItem(int index, const QString &group)
{
    Q_D(DFMSideBar);

    if (d->groupNameMap.contains(group)) {
        DFMSideBarItemGroup *groupPointer = d->groupNameMap[group];
        Q_CHECK_PTR(groupPointer);
        if ((*groupPointer)[index] == d->lastCheckedItem) {
            d->lastCheckedItem = nullptr;
        }
        groupPointer->removeItem(index);
    }
}

void DFMSideBar::removeItem(DFMSideBarItem *item)
{
    Q_D(DFMSideBar);

    if (item == d->lastCheckedItem) {
        d->lastCheckedItem = nullptr;
    }

    DFMSideBarItemGroup *groupPointer = d->groupNameMap[item->groupName()];
    Q_CHECK_PTR(groupPointer);
    int index = groupPointer->itemIndex(item);
    if (index != -1) {
        groupPointer->removeItem(index);
        return;
    }

}

/*!
 * \brief Get the item index from its group.
 *
 * \return the index from its group. return -1 if not found the item.
 *
 * Please notice that this return the index from the item's owner group,
 * not the index of the full sidebar items.
 */
int DFMSideBar::itemIndex(const DFMSideBarItem *item) const
{
    Q_D(const DFMSideBar);

    DFMSideBarItemGroup *groupPointer = d->groupNameMap[item->groupName()];
    Q_CHECK_PTR(groupPointer);
    return groupPointer->itemIndex(item);
}

/*!
 * \fn DFMSideBarItem *DFMSideBar::itemAt(int index, const QString &groupName) const
 *
 * \brief Get the reference of DFMSideBarItem at \a index in the given \a groupName.
 * \param index The item index which is wanted.
 * \param groupName The group name which is wanted.
 * \return the reference (pointer) of the item.
 */
DFMSideBarItem *DFMSideBar::itemAt(int index, const QString &group) const
{
    Q_D(const DFMSideBar);

    if (d->groupNameMap.contains(group)) {
        DFMSideBarItemGroup *groupPointer = d->groupNameMap[group];
        return (*groupPointer)[index];
    }

    return nullptr;
}

DFMSideBarItem *DFMSideBar::itemAt(const DUrl &url) const
{
    Q_D(const DFMSideBar);

    for (QString &key : d->groupNameMap.keys()) {
        DFMSideBarItemGroup *groupPointer = d->groupNameMap.value(key);
        DFMSideBarItem *item = groupPointer->findItem(url);

        if (item) {
            return item;
        }
    }

    // fallback
    for (QString &key : d->groupNameMap.keys()) {
        DFMSideBarItemGroup *groupPointer = d->groupNameMap.value(key);

        for (int i = 0; i < groupPointer->itemCount(); ++i) {
            DFMSideBarItem *item = (*groupPointer)[i];
            const DAbstractFileInfoPointer &info = fileService->createFileInfo(this, item->url());

            if (info && info->canRedirectionFileUrl()) {
                if (info->redirectedFileUrl() == url) {
                    return item;
                }
            }
        }
    }

    return nullptr;
}

DFMSideBarItem *DFMSideBar::takeItem(int index, const QString &group)
{
    Q_D(DFMSideBar);

    if (d->groupNameMap.contains(group)) {
        DFMSideBarItemGroup *groupPointer = d->groupNameMap[group];
        DFMSideBarItem *item = groupPointer->takeItem(index);
        if (d->lastCheckedItem == item) {
            d->lastCheckedItem = nullptr;
        }
        return item;
    }

    return nullptr;
}

int DFMSideBar::itemCount(const QString &group) const
{
    Q_D(const DFMSideBar);

    if (d->groupNameMap.contains(group)) {
        DFMSideBarItemGroup *groupPointer = d->groupNameMap[group];
        return groupPointer->itemCount();
    }

    return 0;
}

QRect DFMSideBar::groupGeometry(const QString &group) const
{
    Q_D(const DFMSideBar);

    if (d->groupNameMap.contains(group)) {
        DFMSideBarItemGroup *groupPointer = d->groupNameMap[group];
        return groupPointer->geometry();
    }

    // or throw a exception if not found?
    return QRect();
}

DFMSideBar::GroupName DFMSideBar::groupFromName(const QString &name)
{
    if (name.isEmpty()) {
        return Other;
    }

    switch (name.toLatin1().at(0)) {
    case 'c':
        if (name == QStringLiteral("common")) {
            return Common;
        }

        break;
    case 'd':
        if (name == QStringLiteral("device")) {
            return Device;
        }

        break;
    case 'b':
        if (name == QStringLiteral("bookmark")) {
            return Bookmark;
        }

        break;
    case 'n':
        if (name == QStringLiteral("network")) {
            return Network;
        }

        break;
    case 't':
        if (name == QStringLiteral("tag")) {
            return Tag;
        }

        break;
    default:
        break;
    }

    return Unknow;
}

QString DFMSideBar::groupName(DFMSideBar::GroupName group)
{
    Q_ASSERT(group != Unknow);

    switch (group) {
    case Common:
        return "common";
    case Device:
        return "device";
    case Bookmark:
        return "bookmark";
    case Network:
        return "network";
    case Tag:
        return "tag";
    case Other: // deliberate
    default:
        break;
    }

    return QString();
}

DFM_END_NAMESPACE