#ifndef _SETTINGSMANAGER_H
#define _SETTINGSMANAGER_H
#include "MD5.h"
#include <memory>
#include "s52/S52Types.h"
#include "SimpleThread.h"
#include "RenderSettings.h"
#include "json.hpp"
#include <atomic>
#define MAX_CLIENTS 5

class IBaseSettings
{
public:
    typedef std::shared_ptr<IBaseSettings> Ptr;
    typedef std::shared_ptr<const IBaseSettings> ConstPtr;
    typedef enum
    {
        ENABLED,
        UNCONFIGURED,
        DISABLED
    } EnabledState;
    virtual EnabledState IsChartSetEnabled(const String &setKey) const =0;
    virtual MD5Name GetMD5()const =0;
    virtual ~IBaseSettings(){}
};
class SettingsManager{
    public:
        using Updater=std::function<void(IBaseSettings::ConstPtr, RenderSettings::ConstPtr)>;
        using ListProvider=std::function<bool(json::JSON &json,const String &item)>;
        using Ptr=std::shared_ptr<SettingsManager>;
        virtual ~SettingsManager(){}
        SettingsManager(String dir,ListProvider provider);
        void registerUpdater(Updater updater);
        RenderSettings::ConstPtr GetRenderSettings() const { return renderSettings;}
        IBaseSettings::ConstPtr GetBaseSettings() const {return baseSettings;}
        void updateSettings(json::JSON &values);
        json::JSON getSettings() const;
        bool getAllowChanges() const;
        void setAllowChanges(bool allow=true); 
        bool enableChartSet(const String &setKey, bool enabled);
        bool getList(json::JSON &json,const String &item) const;
        using SetMap=std::map<String,bool>;
    protected:
        ListProvider provider;
        RenderSettings::ConstPtr renderSettings;
        IBaseSettings::ConstPtr baseSettings;  
        std::mutex lock;
        String settingsDir;
        String error; 
        Updater updater;
        SetMap configuredSets;
        std::atomic<bool> allowChanges={false};
        RenderSettings::ConstPtr loadRenderSettings();
        void saveRenderSettings();
        void saveBaseSettings(); 
};
#endif