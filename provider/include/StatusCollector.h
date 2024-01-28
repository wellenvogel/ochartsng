/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Status Collector
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2020 by Andreas Vogel   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 *
 */

#ifndef STATUSCOLLECTOR_H
#define STATUSCOLLECTOR_H
#include "Types.h"
#include "ItemStatus.h"
#include "SimpleThread.h"
#include <map>
#include <memory>

class StatusCollector : public ItemStatus
{
public:
    StatusCollector();
    virtual ~StatusCollector();
    virtual void ToJson(StatusStream &stream);
    void AddItem(String name, ItemStatus::Ptr item, bool array = false);
    void RemoveItem(String name, ItemStatus::Ptr item = NULL);

private:
    class ItemHolder;
    typedef std::map<String, std::shared_ptr<ItemHolder>> StatusItems;
    StatusItems items;
    std::mutex lock;

protected:
    /**
     * get the local status objects
     * @return true if data written
     */

    virtual bool LocalJson(StatusStream &stream) { return false; }
};

#endif /* STATUSCOLLECTOR_H */
