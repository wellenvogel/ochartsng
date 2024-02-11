/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  main
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2022 by Andreas Vogel   *
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

#include <iostream>
#include <iterator>
#include <getopt.h>
#include <signal.h>
#include "test.h"
#include "StringHelper.h"
#include "FileHelper.h"
#include "HTTPServer.h"
#include "Logger.h"
#include "StaticRequestHandler.h"
#include "ChartRequestHandler.h"
#include "ChartTestRequestHandler.h"
#include "TestRequestHandler.h"
#include "ShopRequestHandler.h"
#include "UploadRequestHandler.h"
#include "ListRequestHandler.h"
#include "TestDrawingRequestHandler.h"
#include "OexControl.h"
#include "StatusCollector.h"
#include "StatusRequestHandler.h"
#include "SettingsManager.h"
#include "SettingsRequestHandler.h"
#include "TokenRequestHandler.h"
#include "ChartManager.h"
#include "ChartFactory.h"
#include "ChartInstaller.h"
#include "S52Data.h"
#include "FontManager.h"
#include "SystemHelper.h"
#include "TileCache.h"

#define CHART_TEMP_DIR "_TMP"
using std::cout;
using std::endl;
void usage (const char *name){
    std::cerr <<  "Usage: " << name << "[...options...] configDir port" << std::endl;
    std::cerr <<  "       -l logDir directory for log file (default: progDir)" << std::endl;
    std::cerr <<  "       -d logLevel log level 0,1,2" << std::endl;
    std::cerr <<  "       -a oexParameters additional parameters to pass to oexserverd" << std::endl;
    std::cerr <<  "       -u chartDir directory for charts default: configDir/charts" << std::endl; 
    std::cerr <<  "       -g guiDir directory for static gui files, default: progDir/gui" << std::endl;
    std::cerr <<  "       -t s57DataDir directory for dynamic s57 data, default: progDir/s57data" << std::endl;
    std::cerr <<  "       -p pid of a process to monitor, stop if it does not run any more" << std::endl;
    std::cerr <<  "       -k switch on debug info in rendered tiles" << std::endl;
    std::cerr <<  "       -b predefinedName - predefined system name to be used when registering this system (android)" << std::endl;
    std::cerr <<  "       -x memPercent limit the chart memory to this percentage of the system memory (default: 50)" << std::endl;
    std::cerr <<  "       -c tileCacheKb - the memory for the tile cache in KB(default:"<< (10*1024) <<"), use 0 to disable" << std::endl;
}
void termHandler(int sig){
    std::cerr << "termhandler" << std::endl;
    OexControl::Ptr control=OexControl::Instance();
    if (control){
        control->Kill();
    }
    exit(1);
}
class MainStatus : public ItemStatus{
    public:
    virtual void ToJson(StatusStream &stream){
        stream["version"]=TOSTRING(AVNAV_VERSION);
    }
};
int mainFunction(int argc, char **argv,bool *stopFlag=NULL)
{
    try{
    String codeBase=FileHelper::realpath(FileHelper::dirname(argv[0]));
    String guiDir=FileHelper::concatPath(codeBase,"gui");
    String logDir=codeBase;
    String oexParam;
    String chartDir;
    String configDir;
    String predefinedSystemName;
    String s57Dir=FileHelper::concatPath(codeBase,"s57data");
    int memPercent=50;
    int monitorPid=0;
    int opt;
    optind=1;
    bool renderDebug=false;
    int logLevel=LOG_LEVEL_INFO;
    int numOpeners=6;
    int tileCacheMem=10*1024;
    while ((opt = getopt(argc, argv, "l:a:d:u:g:t:kp:b:x:o:c:")) != -1) {
                switch (opt) {
                case 'k':
                    renderDebug=true;
                    break;    
                case 'l':
                   logDir=String(optarg);
                   break;
                case 'a':
                    oexParam=String(optarg);
                    break;
                case 'd':
                    logLevel=atoi(optarg);  
                    break;
                case 'u':
                    chartDir=String(optarg);
                    break;
                case 'g':
                    guiDir=optarg;
                    break;
                case 't':
                    s57Dir=optarg;
                    break;
                case 'p':
                    monitorPid=atoi(optarg);
                    break; 
                case 'b':
                    predefinedSystemName=optarg;
                    break;
                case 'x':
                    memPercent=::atoi(optarg);
                    break;
                case 'o':
                    numOpeners=::atoi(optarg);
                    break;
                case 'c':
                    tileCacheMem=::atoi(optarg);
                    if (tileCacheMem < 0) tileCacheMem=0;                         
                default: /* '?' */
                   usage(argv[0]);
                   return -1;
                }
            }

    if ((argc-optind) < 2){
        usage(argv[0]);
        return(1);
    }
    Logger::CreateInstance(logDir);
    Logger::instance()->SetLevel(logLevel);
    LOG_INFO("Start with param %s",StringHelper::concat(argv,argc).c_str());
    configDir=String(argv[optind]);
    int port=atoi(argv[optind+1]);
    if (chartDir.empty()){
        chartDir=FileHelper::concatPath(configDir,"charts");
    }
    configDir=FileHelper::realpath(configDir);
    chartDir=FileHelper::realpath(chartDir);
    String chartTempDir=FileHelper::concatPath(chartDir,CHART_TEMP_DIR);
    LOG_INFO("configDir=%s, chartDir=%s",configDir.c_str(),chartDir.c_str());
    StringVector dirs({configDir,chartDir});
    for (auto it=dirs.begin();it != dirs.end();it++){
        if (! FileHelper::exists(*it,true)){
            FileHelper::makeDirs(*it);
            if (! FileHelper::exists(*it,true)){
                LOG_ERRORC("unable to create directory %s",it->c_str());
                return -1;
            }
        }
    }
    signal(SIGPIPE, SIG_IGN);
    StatusCollector collector;
    ItemStatus::Ptr mainStatus=std::make_shared<MainStatus>();
    collector.AddItem("main",mainStatus);
    StringVector oexParamList;
    if (oexParam != String("")){
        oexParamList=StringHelper::split(oexParam,":");
    }
    OexControl::Init(codeBase,chartTempDir,oexParamList);
    collector.AddItem("oexserverd",OexControl::Instance());
    OexControl::Instance()->Start();
    signal(SIGTERM,termHandler);
    signal(SIGINT,termHandler);
    signal(SIGHUP,termHandler);
    unsigned int memoryLimit=0;
    if (memPercent > 0){
        int systemKb=0;
        SystemHelper::GetMemInfo(&systemKb,NULL);
        memoryLimit=systemKb*memPercent/100;
        if (tileCacheMem > 0){
            memoryLimit-=tileCacheMem;
        }
        LOG_INFO("setting memory limit to %d kb",memoryLimit);
    }
    ChartManager::Ptr chartManager;
    SettingsManager::Ptr settings=std::make_shared<SettingsManager>(configDir,
        [&chartManager](json::JSON &json,const String &item)->bool{
            if (!chartManager) return false;
            if (item == "colorTables"){
                auto tables=chartManager->GetS52Data()->getColorTables();
                for (auto &&table:tables) json.append(table);
                return true;
            }
            return false;
    });
    ChartFactory::Ptr chartFactory=std::make_shared<ChartFactory>(OexControl::Instance());
    FontFileHolder::Ptr fontFile=std::make_shared<FontFileHolder>(FileHelper::concatPath(s57Dir,"Roboto-Regular.ttf"));
    fontFile->init();
    chartManager=std::make_shared<ChartManager>(fontFile,settings->GetBaseSettings(), settings->GetRenderSettings(),chartFactory,s57Dir, memoryLimit,numOpeners);
    settings->registerUpdater([&chartManager](IBaseSettings::ConstPtr base,RenderSettings::ConstPtr rs){
        chartManager->UpdateSettings(base,rs);
    });
    collector.AddItem("chartManager",chartManager);
    //for our normal chart dir we use an empty prefix
    //to get short names
    chartManager->AddKnownDirectory(chartDir,"");
    TileCache::Ptr tileCache=std::make_shared<TileCache>(tileCacheMem);
    chartManager->registerSetChagend([&tileCache](const String &key){
        tileCache->clean(key);
    });
    chartManager->registerSettingsChanged([&tileCache](s52::S52Data::ConstPtr s52data){
        tileCache->cleanBySettings(s52data->getSequence());
    });
    collector.AddItem("tileCache",tileCache);
    Renderer::Ptr trender=std::make_shared<TestRenderer>(chartManager,tileCache,renderDebug);
    Renderer::Ptr render=std::make_shared<Renderer>(chartManager,tileCache,renderDebug);
    TokenHandler::Ptr tokenHandler=std::make_shared<TokenHandler>("all");
    tokenHandler->start();
    HTTPServer server(port,5);
    server.AddHandler(new StaticRequestHandler(guiDir, s57Dir));
    server.AddHandler(new TestRequestHandler(trender,"fpng"));
    server.AddHandler(new TestRequestHandler(trender,"spng"));
    server.AddHandler(new TestDrawingRequestHandler(chartManager,"all"));
    server.AddHandler(new ChartRequestHandler(render,tokenHandler,"fpng"));
    server.AddHandler(new ChartTestRequestHandler(chartManager));
    server.AddHandler(new ShopRequestHandler("https://o-charts.org/shop/index.php",predefinedSystemName));
    server.AddHandler(new StatusRequestHandler(&collector));
    server.AddHandler(new ListRequestHandler(chartManager));
    server.AddHandler(new SettingsRequestHandler(settings));
    server.AddHandler(new TokenRequestHandler(tokenHandler,guiDir));
    //server.AddHandler(new ShopRequestHandler("http://localhost:9000",predefinedSystemName));
    ChartInstaller::Ptr chartInstaller=std::make_shared<ChartInstaller>(chartManager, chartDir, chartTempDir);
    collector.AddItem("chartInstaller",chartInstaller);
    try{
        chartInstaller->Start();
    } catch (Exception &e){
        LOG_ERRORC("unable to start chartInstaller: %s",e.what());
    }
    server.AddHandler(new UploadRequestHandler(chartInstaller));
    try{
        server.Start();
    }catch (HTTPServer::HTTPException &e){
        LOG_ERRORC("unable to start webserver: %s",e.what());
        return -2;
    }
    catch(Exception &u){
        LOG_ERRORC("unable to start webserver: %s",u.what());
        return -3;
    }
    LOG_INFOC("waiting for oexserverd");
    int returnCode=0;
    bool rt=OexControl::Instance()->WaitForState(OexControl::RUNNING,5000);
    if (rt){
        LOG_INFOC("Start reading charts from %s",chartDir.c_str());
        StringVector dirs=FileHelper::listDir(chartDir);
        StringVector toRead;
        for (auto it=dirs.begin();it != dirs.end();it++){
            if (*it == chartInstaller->GetTempDir()) continue;
            toRead.push_back(*it);
        }
        //TODO: chartInfoCache
        chartManager->ReadCharts(toRead,true);
        chartManager->RemoveUnverified();
        int systemKb=0;
        int ourKb=0;
        SystemHelper::GetMemInfo(&systemKb,&ourKb);
        LOG_INFO("Memory after startUp system %d kb, our %d kb",systemKb,ourKb);
        settings->setAllowChanges(true);
        if (stopFlag != NULL){
            LOG_INFOC("Server started at port %d, waiting for stop flag", port);
            while (! *stopFlag){
                Timer::microSleep(100000);
            }
        }
        else if (monitorPid != 0){
            LOG_INFOC("monitoring process %d",monitorPid);
            while(true){
                int res=kill(monitorPid,0);
                if (res != 0){
                    LOG_INFOC("process %d not running any more, stopping self",monitorPid);
                    break;
                }
                Timer::microSleep(2000000);    
            }
        }
        else{
            LOG_INFOC("Server started at port %d, press ENTER to stop", port);
            getchar();
        }
        settings->setAllowChanges(false);
    }
    else{
        LOG_ERRORC("unable to start oexserverd");
        returnCode=-2;
    }
    LOG_INFOC("Done");
    server.Stop();
    LOG_INFOC("WebServer stopped");
    OexControl::Instance()->Stop();
    OexControl::Instance()->WaitForState(OexControl::UNKNOWN,5000);
    LOG_INFOC("OexControl stopped");
    tokenHandler->stop();
    Logger::instance()->Close();
    chartInstaller->Stop();
    return returnCode;
    }catch (FileException &f){
        LOG_ERRORC("%s: %s",f.getFileName(),f.what());
        return -1;
    }catch (AvException &a){
        LOG_ERRORC("%s",a.msg());
        return -2;
    }catch (Exception &e){
        LOG_ERRORC("Unknown exception: %s",e.what());
        return -3;
    }

}

int main(int argc, char **argv){
   return mainFunction(argc,argv);
}