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

#include "StatusCollector.h"
#include <unordered_set>
#include <sstream>

typedef std::unordered_set<ItemStatus::Ptr> HolderItems;

class StatusCollector::ItemHolder : public ItemStatus{
private:
    HolderItems items;
    bool        alwaysArray=false;
public:
    typedef std::shared_ptr<StatusCollector::ItemHolder> Ptr;
    ItemHolder(ItemStatus::Ptr item,bool alwaysArray=false){
        items.insert(item);
        this->alwaysArray=alwaysArray;
    }
    virtual void ToJson(StatusStream &stream){
        if (items.size() == 1 && ! alwaysArray){
            (*(items.begin()))->ToJson(stream);
            return;
        }
        for (auto it=items.begin();it!=items.end();it++){
            json::JSON ito;
            (*it)->ToJson(ito);
            stream.append(ito);
        }
    }
    void AddItem(ItemStatus::Ptr item){
        items.insert(item);
    }
    void RemoveItem(ItemStatus::Ptr item){
        items.erase(item);
    }
    bool Empty(){
        return items.size() < 1;
    }
};

StatusCollector::StatusCollector() {
}

void StatusCollector::ToJson(StatusStream &newStatus) {
    LocalJson(newStatus);
    Synchronized locker(lock);
    for (auto it=items.begin();it!=items.end();it++){
        json::JSON ito;
        it->second->ToJson(ito);
        newStatus[it->first]=ito;
    }
}
void StatusCollector::AddItem(String name,ItemStatus::Ptr item,bool array){
    Synchronized locker(lock);
    StatusItems::iterator it=items.find(name);
    if (it == items.end()){
        items[name]= std::make_shared<ItemHolder>(item,array);
    }
    else{
        it->second->AddItem(item);
    }
}
void StatusCollector::RemoveItem(String name,ItemStatus::Ptr item){
    Synchronized locker(lock);
    if (item == NULL){
        items.erase(name);
    }
    else{
        StatusItems::iterator it=items.find(name);
        if (it != items.end()){
            it->second->RemoveItem(item);
            if (it->second->Empty()) items.erase(it);
        }
    }
}

StatusCollector::~StatusCollector() {
    
}

