/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Render settings
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2023 by Andreas Vogel   *
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
#ifndef _RENDERSETTINGSBASE_H
#define _RENDERSETTINGSBASE_H
#include "Types.h"
#include "s52/S52Types.h"
#include "MD5.h"
class RenderSettingsBase{
    private:
    std::map<int,bool> visibilities;
    MD5Name md5;
    public:
    void setVisibility(int id, bool vis){
        visibilities[id]=vis;
    }
    bool getVisibility(int id) const{
        auto it=visibilities.find(id);
        if (it == visibilities.end()) return true;
        return it->second;
    }
    MD5Name GetMD5()const {return md5;}
    void setMd5Name(const MD5Name &v){md5=v;}
    String loadError;
};
#endif