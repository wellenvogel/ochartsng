/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Settings Manager
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
#include "SettingsManager.h"
#include "UserSettings.h"
#include "FileHelper.h"
#include "Logger.h"
#include "json.hpp"
#define SETTINGS "settings.json"
#define BASE_SETTINGS "base.json"
#define CSKEY "chartSets" //in base settings

json::JSON readJson(String fn){
    LOG_INFO("reading settings file %s",fn);
    std::stringstream data;
    std::ifstream fstream(fn);
    if (! fstream.is_open()){
        throw FileException(fn,"unable to open");
    }
    data << fstream.rdbuf();
    return json::JSON::Load(data.str());
}
void writeJson(const json::JSON &data, String fn){
    String tmp=fn+".tmp";
    FileHelper::unlink(tmp);
    std::ofstream fstream(tmp);
    if (!fstream.is_open()){
        throw FileException(tmp,"unable to open outfile");
    }
    LOG_INFO("write settings %s",tmp);
    fstream << data << std::endl;
    fstream.close();
    if (!FileHelper::rename(tmp,fn)){
        throw FileException(fn, FMT("unable to rename settings file from %s",tmp));
    }
}

class BaseSettings : public IBaseSettings{
    SettingsManager::SetMap configuredSets;
    MD5 md5;
    MD5Name md5name;
    public:
    BaseSettings(SettingsManager::SetMap m):configuredSets(m){
        for (auto it=configuredSets.begin();it != configuredSets.end();it++){
            md5.AddValue(it->first);
            MD5_ADD_VALUE(md5,it->second);
        }
        md5name=md5.GetValueCopy();
    }
    virtual EnabledState IsChartSetEnabled(const String &setKey) const{
        auto it=configuredSets.find(setKey);
        if (it == configuredSets.end()) return UNCONFIGURED;
        return it->second?ENABLED:DISABLED;
    }
    virtual MD5Name GetMD5()const {
        return md5name;
    };
    virtual ~BaseSettings(){}
};

SettingsManager::SettingsManager(String dir, SettingsManager::ListProvider p):provider(p){
    settingsDir=dir;
    RenderSettings::ConstPtr newSettings=loadRenderSettings();
    renderSettings=newSettings;
    if (!newSettings->loadError.empty()){
        try{
            saveRenderSettings();
        }catch (AvException &e){
            LOG_ERROR("unable to save render settings: %s",e.msg());
        }
    }
    //load base settings
    String fn=FileHelper::concatPath(settingsDir,BASE_SETTINGS);
    bool readOk=false;
    if (FileHelper::canRead(fn)){
        try{
            json::JSON jdata=readJson(fn);
            if (jdata.hasKey(CSKEY)){
                auto chartSets=jdata[CSKEY].ObjectRange();
                for (auto it=chartSets.begin();it != chartSets.end();it++){
                    configuredSets[it->first]=it->second.ToBool();
                }
                //TODO: cleanup configured sets
                readOk=true;
            }
        }catch (AvException &e){
            LOG_ERROR("error reading base settings %s: %s",fn,e.msg());
        }
    }
    if (!readOk){
        LOG_ERROR("unable to read base settings, creating it now");
        saveBaseSettings();
    }
    baseSettings=std::make_shared<BaseSettings>(configuredSets);
}
void SettingsManager::registerUpdater(Updater updater){
    Synchronized l(lock);
    this->updater=updater;
}
void fillFromJson(RenderSettings *rs, json::JSON &data)
{
    for (auto it = userSettings.begin(); it != userSettings.end(); it++)
    {
        UserSettingsEntry *e = *it;
        if (!data.hasKey(e->path))
        {
            continue;
        }
        json::JSON parent = data[e->path];
        if (!parent.hasKey(e->name))
        {
            continue;
        }
        json::JSON value = parent[e->name];
        LOG_DEBUG("reading setting %s=%s", e->name, value.ToString());
        try{
            e->setFromJson(rs, value);
        }catch (UserSettingsEntry::InvalidValueException &ex){
            LOG_ERROR("invalid json for %s: %s, using default",e->name,ex.msg());
        }
    }
}
void computeMd5(RenderSettings *rs){
    MD5 md5;
    for (auto it = userSettings.begin(); it != userSettings.end(); it++)
    {
        (*it)->addToMd5(rs, &md5);
    }
    rs->setMd5Name(md5.GetValueCopy());
}
RenderSettings::ConstPtr SettingsManager::loadRenderSettings(){
    String fn=FileHelper::concatPath(settingsDir,SETTINGS);
    std::shared_ptr<RenderSettings> newSettings=std::make_shared<RenderSettings>();
    RenderSettings *rs = newSettings.get();
    if (FileHelper::canRead(fn)){
        try{
            json::JSON jdata=readJson(fn);
            fillFromJson(rs,jdata);
        }
        catch (AvException &e){
            rs->loadError=FMT("unable to load settings: %s",e.msg());
            LOG_ERROR("%s",rs->loadError);
        }
        catch (Exception &e){
            rs->loadError=FMT("unable to load settings: %s",e.what());
            LOG_ERROR("%s",rs->loadError);
        }
    }
    else{
        rs->loadError=FMT("settings file %s not found",fn);
        LOG_ERROR("%s",rs->loadError);
    }
    computeMd5(rs);
    return newSettings;
}

void writeToJson(const RenderSettings *rs, json::JSON &jdata){
    for (auto it = userSettings.begin(); it != userSettings.end(); it++)
    {
        UserSettingsEntry *e = *it;
        if (e->path.empty()) continue;
        if (!jdata.hasKey(e->path)){
            jdata[e->path]=json::JSON();
        }
        e->storeToJson(rs,jdata[e->path]);
    }
}

void SettingsManager::saveRenderSettings(){
    RenderSettings::ConstPtr oldSettings=renderSettings; //atomic access
    const RenderSettings *rs=oldSettings.get();
    json::JSON jdata;
    writeToJson(rs,jdata);
    String fn=FileHelper::concatPath(settingsDir,SETTINGS);
    writeJson(jdata,fn);
}

void SettingsManager::updateSettings(json::JSON &values){
    std::shared_ptr<RenderSettings> newSettings=std::make_shared<RenderSettings>(*renderSettings);
    fillFromJson(newSettings.get(),values);
    computeMd5(newSettings.get());
    newSettings->loadError.clear();
    {
        Synchronized l(lock);
        renderSettings=newSettings;
        saveRenderSettings();
        if (updater){
            updater(baseSettings,renderSettings);
        }
    }
}

json::JSON SettingsManager::getSettings() const{
    json::JSON rt;
    RenderSettings::ConstPtr settings=renderSettings; //atomic access
    const RenderSettings *rs=settings.get();
    writeToJson(rs,rt);
    return rt;
}

bool SettingsManager::getAllowChanges() const{
    return allowChanges;
}
void SettingsManager::setAllowChanges(bool allow){
    allowChanges.store(allow);
}
void SettingsManager::saveBaseSettings(){
    String fn=FileHelper::concatPath(settingsDir,BASE_SETTINGS);
    json::JSON outSets;
    for (auto it=configuredSets.begin();it != configuredSets.end();it++){
        outSets[it->first]=it->second;
    }
    json::JSON out;
    out[CSKEY]=outSets;
    writeJson(out,fn);
}
bool SettingsManager::enableChartSet(const String &setKey, IBaseSettings::EnabledState enabled){
    Synchronized l(lock);
    auto it=configuredSets.find(setKey);
    bool shouldEnable=enabled==IBaseSettings::ENABLED;
    if (it != configuredSets.end()){
        if (enabled == IBaseSettings::UNCONFIGURED){
            configuredSets.erase(it);
        }
        else{
            if (it->second == shouldEnable) return false;
        }
    }
    else{
        if (enabled == IBaseSettings::UNCONFIGURED) return false;
    }
    if (enabled != IBaseSettings::UNCONFIGURED){
        configuredSets[setKey]=shouldEnable;
    }
    std::shared_ptr<BaseSettings> newSettings=std::make_shared<BaseSettings>(configuredSets);
    baseSettings=newSettings;
    saveBaseSettings();
    if (updater){
        updater(baseSettings,renderSettings);
    }
    return true;
}
bool SettingsManager::getList(json::JSON &json,const String &item) const{
    return provider(json,item);
}