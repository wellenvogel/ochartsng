/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Installer
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
#include "ChartInstaller.h"
#include "Exception.h"
#include "StringHelper.h"
#include "SystemHelper.h"
#include "FileHelper.h"
#include "Logger.h"
#include "miniz.h"
#include "Types.h"
#include "ChartSetInfo.h"
#include <curl/curl.h>
#include "ShopRequestHandler.h" //for timeouts
#include <functional>
#define CHART_ZIP "charts.zip"
#define KEY_FILE "keys.xml"
#define UNPACK_BASE "unpack"
#define KEEP_TIME (30*60*1000) //keep results for 30 minutes

static StringVector chartInfos={
    ChartSetInfo::LEGACY_CHARTINFO,
    ChartSetInfo::NEW_CHARTINFO
};
class ResultWrite
{
public:
    String fileName;
    std::ofstream writer;
    bool error = false;
    size_t numWritten = 0;
    ResultWrite(const String &fn) : fileName(fn)
    {
    }
    int write(void *src, size_t length)
    {
        if (!writer.is_open())
        {
            writer.open(fileName);
            if (!writer.is_open())
                throw FileException(fileName, "unable to open for writing");
        }
        writer.write((const char *)src, length);
        if (writer.bad())
        {
            error = true;
            throw FileException(fileName, "error writing...");
        }
        numWritten += length;
        return length;
    }
    ~ResultWrite()
    {
        if (writer.is_open())
        {
            writer.close();
        }
        if (error)
        {
            FileHelper::unlink(fileName);
        }
    }
};
static size_t download_write(void *data, size_t size, size_t nmemb, void *userp){
    ResultWrite *response=(ResultWrite*)userp;
    response->write(data,size*nmemb);
    return size*nmemb;
}
class ProgressHandler{
    public:
    typedef std::function<int(int)> Updater;
    Updater updater;
    bool interrupted=false;
    ProgressHandler(Updater u):updater(u){}
    int update(int percent){
        if (updater) {
            int rt=updater(percent);
            if (rt != 0) interrupted=true;
            return rt;
        }
        return 0;
    }
};
static int progress_callback(void *clientp,
                      curl_off_t dltotal,
                      curl_off_t dlnow,
                      curl_off_t ultotal,
                      curl_off_t ulnow)
{
    ProgressHandler *handler=(ProgressHandler*)clientp;
    if (dltotal == 0) return handler->update(0);
    return handler->update(floor(dlnow*100.0/dltotal));
}
class ChartInstaller::WorkerRunner : public Thread{
        ChartInstaller *installer;
        std::atomic<bool> active={false};
        std::atomic<int> request={-1};
        public:
        WorkerRunner(ChartInstaller *installer){
            this->installer=installer;
        }
        bool isActive(){
            return active;
        }
        int currentRequest(){
            return request;
        }
        void run(){
            while (! shouldStop()){
                Request current=installer->NextRequest();
                if (current.state == ST_WAIT && current.id >= 0){
                    active=true;
                    request=current.id;
                    current.progress=0;
                    LOG_INFO("starting request %d",current.id);
                    try{
                        if (current.tempDir.empty()){
                            current.tempDir=installer->CreateTempDir(current.id);
                        }
                        else{
                            if (! FileHelper::makeDirs(current.tempDir)){
                                throw AvException(FMT("unable to create temp dir %s",current.tempDir));
                            }
                        }
                        if (!current.chartUrl.empty() || ! current.keyFileUrl.empty()){
                            if (current.targetZipFile.empty() && ! current.chartUrl.empty()){
                                current.targetZipFile=FileHelper::concatPath(current.tempDir,CHART_ZIP);
                            }
                            if (current.targetKeyFile.empty() && !current.keyFileUrl.empty()){
                                current.targetKeyFile=FileHelper::concatPath(current.tempDir,KEY_FILE);
                            }
                            current.state=ST_DOWNLOAD;
                            installer->UpdateRequest(current);
                            if (!current.chartUrl.empty()){
                                DownloadFile(current.chartUrl,current.targetZipFile,current.id);
                            }
                            if (! current.keyFileUrl.empty()){
                                DownloadFile(current.keyFileUrl,current.targetKeyFile,current.id);
                            }
                        }
                        String unpackDestination=FileHelper::concatPath(current.tempDir,UNPACK_BASE);
                        String chartSub;
                        String chartsTarget;
                        if (!current.targetZipFile.empty()){
                            current.state=ST_UNPACK;
                            installer->UpdateRequest(current);
                            if (! FileHelper::makeDirs(unpackDestination)){
                                throw AvException(FMT("unable to create unpack dir %s",unpackDestination));
                            }
                            LOG_INFO("unpacking zip %s",current.targetZipFile);
                            unpackZip(current.targetZipFile,unpackDestination,current.id);
                            StringVector unpacked=FileHelper::listDir(unpackDestination);
                            int numSub=0;
                            for (auto it=unpacked.begin();it != unpacked.end();it++){
                                if (FileHelper::exists(*it,true)){
                                    //sub directory
                                    for (auto idf=chartInfos.begin();idf!=chartInfos.end();idf++){
                                        String infoFile=FileHelper::concatPath(*it,*idf);
                                        if (FileHelper::exists(infoFile)){
                                            if (numSub > 0){
                                                throw ZipException(FMT("found multiple chart directories in zip %s and %s",chartSub,*it));
                                            }
                                            LOG_DEBUG("found chartinfo %s for %s",*idf,*it);
                                            chartSub=*it;
                                            numSub++;
                                            break;
                                        }
                                    }
                                }
                            }
                            if (chartSub.empty()){
                                throw ZipException(FMT("no valid chart dir found in zip"));
                            }
                            LOG_INFO("found charts directory %s",chartSub);
                            chartsTarget=FileHelper::concatPath(installer->chartDir,FileHelper::fileName(chartSub));
                            if (FileHelper::exists(chartsTarget,true) || FileHelper::exists(chartsTarget,false)){
                                throw AvException(FMT("chart directory %s already exists",chartsTarget));
                            }
                            if (!current.targetKeyFile.empty()){
                                LOG_DEBUG("move key file %s into %s",current.targetKeyFile,chartSub);
                                String target=FileHelper::concatPath(chartSub,FileHelper::fileName(current.targetKeyFile));
                                if (! FileHelper::rename(current.targetKeyFile,target)){
                                    throw AvException(FMT("cannot move key file %s into %s",current.targetKeyFile,chartSub));
                                }
                            }
                        }
                        current.state=ST_TEST;
                        current.progress=0;
                        installer->UpdateRequest(current);
                        testChartSet(chartSub,current.setId);
                        LOG_INFO("moving chart dir %s in place",FileHelper::fileName(chartSub));
                        if (! FileHelper::rename(chartSub,chartsTarget)){
                            throw AvException(FMT("cannot move chart dir into %s: %s",chartsTarget,SystemHelper::sysError()));
                        }
                        current.state=ST_PARSE;
                        current.progress=0;
                        installer->UpdateRequest(current);
                        int numCharts=installer->chartManager->ReadChartDirs(StringVector{chartsTarget},true);
                        if (numCharts < 1){
                            throw AvException(FMT("no charts parsed from %s",chartsTarget));
                        }
                        current.state=ST_DONE;
                    }catch (InterruptedException &i){
                        current.state=ST_INTERRUPT;
                        current.error="interrupted by user";
                    } catch (Chart::InvalidChartException &c){
                        current.state=ST_ERROR;
                        current.error=FMT("unable to read %s (%s) - possibly wrong key",FileHelper::fileName(c.getFileName()),c.what());
                    }catch (FileException &f){
                        current.state=ST_ERROR;
                        current.error=FMT("error for chart %s:%s",FileHelper::fileName(f.getFileName()),f.what());
                    }catch (Exception &e){
                        current.state=ST_ERROR;
                        current.error=FMT("%s",e.what());
                    }
                    if (current.state == ST_ERROR || current.state == ST_INTERRUPT){
                        LOG_ERROR("%s",current.error);
                    }
                    current.progress=0;
                    current.end=Timer::steadyNow();
                    LOG_INFO("finished request %d",current.id);
                    installer->UpdateRequest(current,false);
                    current.Cleanup();
                    request=-1;
                    active=false;
                }
                else{
                    installer->waiter.wait(5000);
                }
                installer->HouseKeeping(false);
            }
        }
        protected:
            bool unpackZip(const String &zipFile, const String &targetDir, int currentRequest){
                if (! FileHelper::canRead(zipFile)) throw ZipException(FMT("cannot read zip file %s",zipFile));
                FileHelper::makeDirs(targetDir);
                if (! FileHelper::canWrite(targetDir)) throw ZipException(FMT("cannot write to irectory %s",targetDir));
                mz_bool status;
                mz_zip_archive *zip_archive=new mz_zip_archive();
                mz_zip_zero_struct(zip_archive);
                avnav::VoidGuard zipGuard([&zip_archive](){
                    mz_zip_reader_end(zip_archive);
                    delete zip_archive;
                });
                status = mz_zip_reader_init_file(zip_archive,zipFile.c_str(),0);
                if (! status){
                    throw ZipException(FMT("invalid zip file %s, cannot open",zipFile));
                }
                int numEntries=(int)mz_zip_reader_get_num_files(zip_archive);
                for (int i = 0; i < numEntries; i++){
                    int progress=i*100/numEntries;
                    installer->UpdateProgress(currentRequest,progress);
                    mz_zip_archive_file_stat file_stat;
                    if (!mz_zip_reader_file_stat(zip_archive, i, &file_stat)){
                        throw ZipException(FMT("unable to read zip entry %d in %s",i,zipFile));
                    }
                    String extractDest=FileHelper::concatPath(targetDir,file_stat.m_filename);
                    if (file_stat.m_is_directory){
                        LOG_DEBUG("creating directory %s",extractDest);
                        if (! FileHelper::makeDirs(extractDest) || ! FileHelper::canWrite(extractDest)){
                            throw ZipException(FMT("unable to create directory %s",extractDest));
                        }
                    }
                    else{
                        LOG_DEBUG("extracting file %s",extractDest);
                        String dirName=FileHelper::dirname(extractDest);
                        if (! FileHelper::exists(dirName,true)){
                            FileHelper::makeDirs(dirName);
                        }
                        if (! FileHelper::canWrite(dirName)){
                            throw FileException(dirName,"unable to create writable directory");
                        }
                        if (! mz_zip_reader_extract_to_file(zip_archive,i,extractDest.c_str(),0)){
                            throw ZipException(FMT("unable to extract %s",extractDest));
                        }
                    }
                }
                return true;
            }

            bool testChartSet(const String &chartDir, const String &chartSetId){
                if (! FileHelper::canRead(chartDir)) throw AvException(FMT("cannot read chart dir %s",chartDir));
                StringVector charts=FileHelper::listDir(chartDir);
                for (auto it=charts.begin();it != charts.end();it++){
                    if (installer->chartManager->GetChartType(*it) != Chart::UNKNOWN){
                        ChartManager::TryResult res=installer->chartManager->TryOpenChart(*it);
                        if (! res.set){
                            throw FileException(*it,"no chart set created");
                        }
                        if (! chartSetId.empty()){
                            if (chartSetId != res.set->info->chartSetId){
                                throw FileException(*it,FMT("chart set has unexpected id %s (expected %s)",res.set->info->chartSetId,chartSetId));
                            }
                        }
                        return true;
                    }
                }
                throw AvException(FMT("no valid charts found in %s",chartDir));
            } 
            bool DownloadFile(const String &url, const String &targetFile, int requestId){
                LOG_INFO("start downloading %s to %s",url,targetFile);
                String dirName=FileHelper::dirname(targetFile);
                if (!FileHelper::exists(dirName,true)){
                    FileHelper::makeDirs(dirName);
                }
                if (! FileHelper::canWrite(dirName)){
                    throw FileException(dirName,"cannot write");
                }
                if (FileHelper::exists(targetFile)){
                    FileHelper::unlink(targetFile);
                }
                ResultWrite *result=new ResultWrite(targetFile);
                ProgressHandler *progress=new ProgressHandler([this,requestId](int percent)->int{
                    try{
                        installer->UpdateProgress(requestId,percent);
                        return 0;
                    }catch (InterruptedException &i){
                        return 1;
                    };
                });
                CURL *curl=curl_easy_init();
                CURLM *curlMulti=curl_multi_init();
                avnav::VoidGuard guard([&curl,&result,&progress,&curlMulti](){
                    if (curlMulti){
                        if (curl) curl_multi_remove_handle(curlMulti,curl);
                        curl_multi_cleanup(curlMulti);
                    }
                    if (curl) curl_easy_cleanup(curl);
                    delete result;
                    delete progress;
                });
                if (! curl) throw AvException("unable to init curl for download");
                curl_easy_setopt(curl, CURLOPT_URL,url.c_str());
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); //as we cannot be sure to have valid certs
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,SHOP_CONNECT_TIMEOUT);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,download_write);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, result);
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,progress_callback);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA,progress);
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS,0);
                curl_multi_add_handle(curlMulti,curl);
                int runningHandles=0;
                do {
                    CURLMcode res = curl_multi_perform(curlMulti,&runningHandles);
                    if (res == CURLM_OK){
                        res=curl_multi_wait(curlMulti, NULL, 0, 1000, NULL);
                    }
                    if(res != CURLM_OK){
                        throw AvException(FMT("unable to download %s: %s",url,curl_multi_strerror(res)));         
                    }
                    installer->CheckInterrupted(requestId);
                }while (runningHandles > 0);
                if (progress->interrupted) {
                    throw InterruptedException("interrupted by user");
                }
                LOG_INFO("finished downloading %s to %s after %d bytes",url,targetFile,result->numWritten);
                return true;
            }
    };
ChartInstaller::ChartInstaller(ChartManager::Ptr chartManager,const String &chartDir,const String &tempDir){
    this->chartDir=chartDir;
    this->tempDir=tempDir;
    this->chartManager=chartManager;
}
String ChartInstaller::CreateTempDir(int requestId){
    String rt=FileHelper::concatPath(tempDir,FMT("tmp%05d",requestId));
    if (! FileHelper::makeDirs(rt)) throw AvException(FMT("unable to create temp dir %s",rt));
    return rt;
}
void ChartInstaller::Start(){
    if (worker) return;
    FileHelper::makeDirs(this->chartDir);
    FileHelper::makeDirs(this->tempDir);
    if (!FileHelper::exists(chartDir,true) || ! FileHelper::canWrite(chartDir)){
        startupError=FMT("cannot write to chartDir %s",chartDir);
        throw AvException(startupError);
    }
    if (! FileHelper::exists(tempDir,true) || ! FileHelper::canWrite(tempDir) ){
        startupError=FMT("cannot write to tempDir %s",tempDir);
        throw AvException(startupError);
    }
    if (! FileHelper::emptyDirectory(tempDir)){
        startupError=FMT("cannot cleanup tempDir %s",tempDir);
        throw AvException(startupError);
    }
    worker=new WorkerRunner(this);
    worker->start();
}
void ChartInstaller::Stop(){
    if (! worker) return;
    worker->stop();
    worker->join();
    delete worker;
    worker=NULL;
}
ChartInstaller::~ChartInstaller(){
    Stop();
}
ChartInstaller::Request ChartInstaller::GetRequest(int requestId)
{
    CondSynchronized l(waiter);
    if (! worker) throw AvException("not running");
    auto it=requests.find(requestId);
    if (it == requests.end()) throw AvException(FMT("request %d not in the list",requestId));
    return it->second;
}
ChartInstaller::Request ChartInstaller::CurrentRequest(){
    int id=-1;
    if (worker) id=worker->currentRequest();
    if (id <= 0){
        //get the latest
        CondSynchronized l(waiter);
        if (requests.size()>0){
            Timer::SteadyTimePoint oldest=Timer::steadyNow();
            for (auto it=requests.begin();it != requests.end();it++){
                if (it->second.start < oldest){
                    id=it->second.id;
                }
            }
        }
    }
    return GetRequest(id);
}
void ChartInstaller::HouseKeeping(bool doThrow)
{
    std::vector<Request> toRemove;
    bool tooMany=false;
    {
        CondSynchronized l(waiter);
        while (requests.size() > MAX_REQUESTS)
        {
            // remove oldest request
            Timer::SteadyTimePoint oldest=Timer::steadyNow();
            int id = -1;
            for (auto it = requests.begin(); it != requests.end(); it++)
            {
                if (it->second.state == ST_WAIT || it->second.state == ST_DOWNLOAD)
                {
                    continue;
                }
                if (Timer::steadyPassedMillis(it->second.start,KEEP_TIME)){
                    toRemove.push_back(it->second);
                }
                if (id < 0 || it->second.start < oldest)
                {
                    oldest = it->second.start;
                    id = it->second.id;
                }
            }
            if (id >= 0){
                toRemove.push_back(requests[id]);
            }
            for (auto it=toRemove.begin();it != toRemove.end();it++){
                if (it->id >= 0){
                    requests.erase(it->id);
                    it->id=-1;
                }
            }
            if (id < 0)
            {
                if (requests.size() > MAX_REQUESTS){
                    tooMany=true;
                }
                break;
            }
        }
    }
    //outside lock
    for (auto it=toRemove.begin();it != toRemove.end();it++){
        it->Cleanup();
    }
    if (tooMany && doThrow){
        throw AvException("too many open requests");
    }
}
ChartInstaller::Request ChartInstaller::StartRequest(const String &chartUrl, const String &keyUrl)
{
    if (!worker)
        throw AvException("not running");
    if (worker->isActive())
        throw AvException("a request is already running");
    HouseKeeping();
    {
        CondSynchronized l(waiter);
        Request nextRequest;
        nextRequest.chartUrl = chartUrl;
        nextRequest.keyFileUrl = keyUrl;
        nextRequestId++;
        nextRequest.id = nextRequestId;
        nextRequest.state = ST_WAIT;
        nextRequest.start = Timer::steadyNow();
        requests[nextRequestId] = nextRequest;
        l.notifyAll();
        LOG_INFO("StartRequest chartUrl=%s, keyUrl=%s, id=%d", chartUrl, keyUrl, nextRequest.id);
        return nextRequest;
    }
}

ChartInstaller::Request ChartInstaller::StartRequestForUpload()
{
    if (!worker)
        throw AvException("not running");
    if (worker->isActive())
        throw AvException("a request is already running");
    HouseKeeping();
    {
        CondSynchronized l(waiter);
        Request nextRequest;
        nextRequestId++;
        nextRequest.id = nextRequestId;
        nextRequest.state = ST_UPLOAD;
        nextRequest.tempDir = CreateTempDir(nextRequest.id);
        nextRequest.targetZipFile = FileHelper::concatPath(nextRequest.tempDir, CHART_ZIP);
        std::shared_ptr<std::ofstream> stream = std::make_shared<std::ofstream>(nextRequest.targetZipFile);
        if (!stream->is_open())
        {
            FileHelper::emptyDirectory(nextRequest.tempDir, true);
            throw AvException(FMT("unable to open %s for writing", nextRequest.targetZipFile));
        }
        nextRequest.stream = stream;
        nextRequest.start = Timer::steadyNow();
        requests[nextRequestId] = nextRequest;
        l.notifyAll();
        LOG_INFO("StartRequestForUpload %d", nextRequest.id);
        return nextRequest;
    }
}

void ChartInstaller::FinishUpload(int requestId, const String &error){
    Request request;
    bool cleanup=false;
    {
        CondSynchronized l(waiter);
        if (!worker)
            throw AvException("not running");
        auto it = requests.find(requestId);
        if (it == requests.end())
            throw AvException(FMT("request %s not found", requestId));
        if (it->second.state != ST_UPLOAD){
            throw AvException(FMT("request %s is not uploading",requestId));
        }    
        LOG_INFO("finish upload for request %d", requestId);
        if (it->second.stream)
        {
            it->second.stream->close();
            it->second.stream.reset();
        }
        if (error.empty())
        {
            it->second.state = ST_WAIT;
        }
        else
        {
            it->second.error = error;
            it->second.state = ST_ERROR;
            cleanup=true;
        }
        l.notifyAll();
        request = it->second;
    }
    //cleanup outside of lock
    if (cleanup){
        request.Cleanup();
    }
}

ChartInstaller::Request ChartInstaller::NextRequest(){
    CondSynchronized l(waiter);
    for (auto it=requests.begin();it != requests.end();it++){
        if (it->second.state == ST_WAIT){
            return it->second;
        }
    }
    return Request();
}
void ChartInstaller::UpdateRequest(const ChartInstaller::Request &v, bool checkInterrupt){
    CondSynchronized l(waiter);
    auto it=requests.find(v.id);
    if (it == requests.end()) {
        if (checkInterrupt) throw InterruptedException("request removed");
        return;
    }
    if (checkInterrupt && it->second.state == ST_INTERRUPT){
        throw InterruptedException("");
    }
    it->second=v;
}
void ChartInstaller::UpdateProgress(int requestId, int progress, bool checkInterrupt){
    CondSynchronized l(waiter);
    auto it=requests.find(requestId);
    if (it == requests.end()) {
        if (checkInterrupt) throw InterruptedException("request removed");
        return;
    }
    if (checkInterrupt && it->second.state == ST_INTERRUPT){
        throw InterruptedException("");
    }
    it->second.progress=progress;
}

void ChartInstaller::ToJson(StatusStream &stream){
    CondSynchronized l(waiter);
    stream["active"]=worker != nullptr;
    stream["working"]=(worker != nullptr) && worker->isActive();
    stream["requests"]=requests.size();
    stream["current"]=(worker != nullptr)?worker->currentRequest():-1;
    stream["tempDir"]=tempDir;
    stream["error"]=startupError;
}
bool ChartInstaller::CanInstall() const{
    return (worker != NULL) && (worker->currentRequest() < 0);
}
bool ChartInstaller::InterruptRequest(int requestId){
    CondSynchronized l(waiter);
    auto it=requests.find(requestId);
    if (it == requests.end()) return false;
    it->second.state=ST_INTERRUPT;
    return true;
}
bool ChartInstaller::CheckInterrupted(int requestId, bool throwInterrupted){
    CondSynchronized l(waiter);
    auto it=requests.find(requestId);
    if (it == requests.end()) return false;
    if (it->second.state != ST_INTERRUPT) return false;
    if (throwInterrupted) throw InterruptedException("");
    return true;
}
bool ChartInstaller::DeleteChartSet(const String &key){
    ChartSet::ConstPtr set=chartManager->GetChartSet(key);
    if (! set) throw AvException(FMT("chart set %s not found",key));
    const String dirName=set->info->dirname;
    set.reset(); //free the set
    bool rt=chartManager->DeleteChartSet(key);
    if (! rt) throw AvException(FMT("unable to delete parsed chart data for %s",key));
    FileHelper::emptyDirectory(dirName,true);
    return true;
}
int ChartInstaller::ParseChartDir(const String &name){
    for (auto && chartSubDir: FileHelper::listDir(chartDir)){
        String fullName=FileHelper::realpath(FileHelper::concatPath(chartDir,chartSubDir));
        if (fullName == FileHelper::realpath(GetTempDir())) continue;
        if (chartSubDir == name){
            return chartManager->ReadChartDirs({fullName},true);
        }
    }
    return 0;
}
void ChartInstaller::Request::ToJson(StatusStream &stream){
    stream["chartUrl"]=chartUrl;
    stream["keyUrl"]=keyFileUrl;
    stream["error"]=error;
    stream["state"]=(int)state;
    stream["progress"]=progress;
    stream["start"]=Timer::steadyToTime(start);
    int64_t age=-1;
    if (end != Timer::SteadyTimePoint()){
        //set
        age=Timer::steadyDiffMillis(end);
    }
    stream["age"]=age;
    stream["id"]=id;
}

void ChartInstaller::Request::Cleanup(){
    if (stream){
        stream->close();
        stream.reset();
    }
    if (! tempDir.empty()){
        LOG_DEBUG("cleaning up for request %d, tempDir %s",id,tempDir);
        FileHelper::emptyDirectory(tempDir,true);
        tempDir.clear();
    }
}
