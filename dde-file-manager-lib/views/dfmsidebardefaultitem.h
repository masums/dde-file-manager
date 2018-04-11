/*
 * Copyright (C) 2017 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     Gary Wang <wzc782970009@gmail.com>
 *
 * Maintainer: Gary Wang <wangzichong@deepin.com>
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
#ifndef DFMSIDEBARDEFAULTITEM_H
#define DFMSIDEBARDEFAULTITEM_H

#include <dfmglobal.h>
#include <dfmsidebaritem.h>

#include "dfmstandardpaths.h"

DFM_BEGIN_NAMESPACE

class DFMSideBarDefaultItem : public DFMSideBarItem
{
public:
    DFMSideBarDefaultItem(DFMStandardPaths::StandardLocation location, QWidget *parent = nullptr);

    DUrl GetDUrlFromStandardLocation(DFMStandardPaths::StandardLocation location);
};

DFM_END_NAMESPACE

#endif // DFMSIDEBARDEFAULTITEM_H
