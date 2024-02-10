/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  OESU chart
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
#include "Chart.h"
#include "OESUChart.h"
#include "FileHelper.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_set>
#include "Osenc.h"
#include "generated/S57ObjectClasses.h"
#include "generated/S57AttributeIds.h"

#define INITIAL_BUFFER_SIZE 256
#define MAX_BUFFER_SIZE (40*1024*1024)
class RecordBuffer: public IRecordBuffer{
    uint32_t size=INITIAL_BUFFER_SIZE;
    char *buffer=new char[INITIAL_BUFFER_SIZE];
    uint32_t fill=0;
    String fileName;
    bool ensure(uint32_t newSize){
        bool changed=false;
        while (size < newSize){
            size = size << 2;
            changed=true;
        }
        if (changed){
            delete[] buffer;
            buffer=new (std::nothrow) char[size]; 
            if (! buffer){
                throw FileException(fileName,FMT("unable to allocated %d bytes for record buffer",newSize));
            }
        }
        return true;
    }
public:
    RecordBuffer(const String &fileName){
        this->fileName=fileName;
    }
    virtual ~RecordBuffer(){
        delete []buffer;
    }
    virtual char * WriteToBuffer(InputStream::Ptr stream,uint32_t len) override{
        fill=0;
        if (len == 0){
            return buffer;
        }
        if (len > MAX_BUFFER_SIZE){
            throw Chart::InvalidChartException(fileName, FMT("buffer size %d too big",len));
        }
        ensure(len);
        ssize_t rd=stream->read(buffer,len);
        if (rd == 0){
            throw NoDataException(fileName,"no data read from buffer");
        }
        if (rd < len){
            throw NoDataException(fileName,FMT("unable to read %d bytes from stream, only got %d",len,rd));
        }
        fill=rd;
        return buffer;
    }
    virtual uint32_t GetFill() override {return fill;}
    virtual char* GetBuffer()override { return buffer;}
    uint32_t forward(InputStream::Ptr stream,uint32_t len){
        uint32_t skipped=0;
        while (skipped < len){
            size_t toRead=len-skipped;
            if (toRead > size) toRead=size;
            ssize_t rd=stream->read(buffer,toRead);
            if (rd <= 0){
                return skipped;
            }
            skipped+=rd;
        }
        return skipped;    
    }
    virtual const String & GetFileName() override{
        return fileName;
    }
};

OESUChart::OESUChart(const String &setKey, ChartType type, const String &fileName)
        :Chart(setKey,type,fileName),
        apool(ocalloc::makePool(FileHelper::fileName(fileName,false))),
        poolRef(apool),
        s57Objects(poolRef),
        rigidAtons(poolRef),
        floatAtons(poolRef),
        connectedNodeTable(poolRef),
        edgeNodeTable(poolRef),
        txtdscTable(poolRef)
        {
        }

template<class BT, bool hasScale>
void parseAreaRecord(BT &reader,S57Object *currentObject,
    bool versionAbove200,
    ocalloc::PoolRef poolRef,
    const Coord::WorldXy &refPoint,
    double scale=1
    ){
    typename BT::Type *p=reader.GetBuffer();
    Coord::LLBox ext;
    ext.e_lon=p->extent_e_lon;
    ext.w_lon=p->extent_w_lon;
    ext.n_lat=p->extent_n_lat;
    ext.s_lat=p->extent_s_lat;
    currentObject->extent=ext.toWorld();
    currentObject->geoPrimitive=s52::GEO_AREA;
    reader.SetBufferPointer(offsetof(typename BT::Type,payLoad));
    //OSenc.cpp#2773
    //skip pointcounts
    reader.skipInt(p->contour_count);
    for(int i=0 ; i < p->triprim_count ; i++){
        uint8_t triType=0;
        reader.GetFromBuffer(&triType,1,"triType");
        uint32_t numberVertices=0;
        reader.GetFromBuffer(&numberVertices, 1,"numberVertices");
        Coord::Extent ext;
        reader.skipDouble(4); //vert extent
        S57Object::VertexList::Ptr vertexList=ocalloc::allocate_shared<S57Object::VertexList>(poolRef,triType,numberVertices,poolRef);
        for (int vidx=0;vidx < numberVertices;vidx++){
            float tmp[2];
            reader.GetFromBuffer(&tmp[0],2,"Vertice");
            if constexpr(hasScale){
                vertexList->add(Coord::worldFromSM(tmp[0]/scale,tmp[1]/scale,refPoint));
            } else{
                vertexList->add(Coord::worldFromSM(tmp[0],tmp[1],refPoint));
            }
        }
        currentObject->area.push_back(vertexList);
        if (vertexList->points.size() > 0){
            currentObject->extent.extend(vertexList->extent);
        }
    }
    int stride=versionAbove200?4:3;
    for (uint32_t i=0;i<p->edgeVector_count;i++){
        int indices[4];
        reader.GetFromBuffer(&indices[0],stride,"line index entry");
        currentObject->lines.push_back(S57Object::LineIndex(indices,versionAbove200));
    }
    //TODO: the original code uses easting and norting
    //      computes the min/max and the midpoints
    //      and only afterwards converts to coordinates
    //      this seems to give slightly different results
    //      see eSENCChart.cpp setAreaGeometry
    currentObject->point=currentObject->extent.midPoint(); 
}

template <class BT, bool hasScale>
static void parseEdgeVectors(
    const String &fileName,
    BT &reader, OESUChart::VectorEdgeNodeTable &edgeNodeTable,
    ocalloc::PoolRef poolRef,
    const Coord::WorldXy &refPoint,
    double scale=1)
{
    auto p = reader.GetBuffer();
    reader.SetBufferPointer(OFFSET_AFTER(typename BT::Type, numEntries));
    for (int i = 0; i < p->numEntries; i++)
    {
        int idxAndCount[2];
        reader.GetFromBuffer(&idxAndCount[0], 2, "edge vector feature header");
        OESUChart::VectorEdgeNode edges(poolRef);
        for (int en = 0; en < idxAndCount[1]; en++)
        {
            float temp[2];
            reader.GetFromBuffer(&temp[0], 2, "edge node point");
            if constexpr (hasScale){
                edges.points.push_back(Coord::worldFromSM(temp[0]/scale, temp[1]/scale, refPoint));
            }
            else{
                edges.points.push_back(Coord::worldFromSM(temp[0], temp[1], refPoint));
            }
        }
        if (idxAndCount[0] > 0x7FFFFFFF)
        {
            LOG_ERROR("%s: invalid edge node index %d", fileName, idxAndCount[0]);
        }
        else
        {
            edgeNodeTable.set(idxAndCount[0], edges);
        }
    }
}
template <class BT, bool hasScale>
static void parseNodeVectors(
    BT &reader, OESUChart::ConnectedNodeTable &connectedNodeTable,
    ocalloc::PoolRef poolRef,
    const Coord::WorldXy &refPoint,
    double scale = 1)
{
    auto p = reader.GetBuffer();
    reader.SetBufferPointer(OFFSET_AFTER(typename BT::Type, numEntries));
    for (int i = 0; i < p->numEntries; i++)
    {
        int index;
        reader.GetFromBuffer(&index, 1, "node vector feature header");
        float temp[2];
        reader.GetFromBuffer(&temp[0], 2, "node point");
        if constexpr (hasScale){
            connectedNodeTable.set(index,
                Coord::worldFromSM(temp[0]/scale, temp[1]/scale, refPoint));
        }
        else{
            connectedNodeTable.set(index,
                Coord::worldFromSM(temp[0], temp[1], refPoint));
        }
    }
}
bool OESUChart::ReadChartStream(InputStream::Ptr input,s52::S52Data::ConstPtr s52data, bool headerOnly){
    if (! input){
        throw FileException(fileName,"no input stream when reading");
    }
    std::map<uint16_t,bool> requiredRecords; //initially false, set true when read
    if (headerOnly){
        requiredRecords[OSENC_RecordExtent::code()]=false;
        requiredRecords[OSENC_RecordNativeScale::code()]=false;
        requiredRecords[OSENC_RecordVersion::code()]=false;
        requiredRecords[OSENC_RecordCellName::code()]=false;
        requiredRecords[OSENC_RecordCellEdition::code()]=false;
    }
    else{
        if (! s52data) throw FileException(fileName,"s52data not set in readChartStream");
        md5=s52data->getMD5();
    }
    RecordBuffer buffer(fileName);
    S57Object::Ptr currentObject;
    std::unordered_set<int> unknownRecords;
    avnav::VoidGuard guard([&currentObject,&unknownRecords,this](){
        if (currentObject){
            s57Objects.push_back(currentObject);
        }
        if (unknownRecords.size() > 0){
            std::stringstream out;
            out << "unknown records: ";
            for (auto it=unknownRecords.begin();it != unknownRecords.end();it++){
                out << *it << ",";
            }
            LOG_DEBUG("%s: %s",this->fileName,out.str());
        }
    });
    while (true){
        bool allRead=true;
        if (requiredRecords.size() > 0){
            for (auto it=requiredRecords.begin();it != requiredRecords.end();it++){
                    if (! it->second){ 
                        allRead=false;
                        break;
                    }
            }
            if (allRead){
                LOG_DEBUG("all required records have been read, closing stream");
                return true;
            }
        }
        uint16_t recordType=-1;
        uint32_t recordLength=0;
        try{
            buffer.WriteToBuffer(input,sizeof(OSENC_RecordBase::Type));
            OSENC_RecordBase baseRecord(&buffer);
            OSENC_RecordBase::Type *base=baseRecord.GetBuffer();
            recordType=base->record_type;
            auto it=requiredRecords.find(recordType);
            if (it != requiredRecords.end()) it->second=true;
            recordLength=baseRecord.GetLength();
            if (recordLength == 0) {
                throw IRecordBuffer::NoDataException("end of data");
            }
            if (recordType > SERVER_STATUS_RECORD){
                LOG_ERROR("%s: invalid record type %d at %d, treating as eof",fileName, recordType, input->BytesRead()-sizeof(OSENC_RecordBase::Type));
                throw IRecordBuffer::NoDataException("invalid record type");
            }
            buffer.WriteToBuffer(input,recordLength);
        }catch (IRecordBuffer::NoDataException &e){
            if (requiredRecords.size()){
                for (auto it=requiredRecords.begin();it != requiredRecords.end();it++){
                    if (! it->second){
                        throw InvalidChartException(fileName,FMT("missing record %d",it->first));
                    }
                }
            }
            //normal end - unable to read header
            return true;
        } catch (FileException &f){
            throw;
        } catch (Exception &t){
            throw FileException(fileName,FMT("exception while reading: %s",t.what()));
        }
        if (headerOnly && requiredRecords.find(recordType) == requiredRecords.end()){
            continue;
        }
        switch(recordType){
            case OSENC_RecordServerStatus::code():
            {
                OSENC_RecordServerStatus reader(&buffer);
                OSENC_RecordServerStatus::Type *p=reader.GetBuffer();
                if (! p->decryptStatus) throw InvalidChartException(fileName,"decrypt status %d",p->decryptStatus);
                if (! p->expireStatus) throw ExpiredException(fileName,"chart is expired");
            }
            break;
            case OSENC_RecordCellName::code():
            {
                OSENC_RecordCellName reader(&buffer);
                headerInfo.cellName=reader.GetName();
                LOG_DEBUG("cell name %s",headerInfo.cellName);
            }
            break;
            case OSENC_RecordCellPublishDate::code():
            {
                OSENC_RecordCellPublishDate reader(&buffer);
                headerInfo.cellPublishDate=reader.GetName();
                LOG_DEBUG("cell publish date %s",headerInfo.cellPublishDate);
            }
            break;
            case OSENC_RecordCellUpdateDate::code():
            {
                OSENC_RecordCellUpdateDate reader(&buffer);
                headerInfo.cellUpdateDate=reader.GetName();
                LOG_DEBUG("cell update date %s",headerInfo.cellUpdateDate);
            }
            break;
            case OSENC_RecordCellCreatedDate::code():
            {
                OSENC_RecordCellCreatedDate reader(&buffer);
                headerInfo.createdDate=reader.GetName();
                LOG_DEBUG("cell update date %s",headerInfo.createdDate);
            }
            break;
            case OSENC_RecordSoundingDatum::code():
            {
                OSENC_RecordSoundingDatum reader(&buffer);
                headerInfo.soundingDatum=reader.GetName();
                LOG_DEBUG("sounding datum %s",headerInfo.soundingDatum);
            }
            break;
            case OSENC_RecordCellUpdate::code():
            {
                OSENC_RecordCellUpdate reader(&buffer);
                auto record=reader.GetBuffer();
                headerInfo.cellUpdate=record->update;
                LOG_DEBUG("cell update %d",headerInfo.cellUpdate);
            }
            break;
            case OSENC_RecordExtent::code():
            {
                OSENC_RecordExtent reader(&buffer);
                OSENC_RecordExtent::Type *p=reader.GetBuffer();
                Coord::LLBox LLBox;
                //TODO value checks?
                LLBox.e_lon=p->extent_se_lon;
                LLBox.w_lon=p->extent_nw_lon;
                LLBox.n_lat=p->extent_nw_lat;
                LLBox.s_lat=p->extent_se_lat;
                Coord::LLXy llref=LLBox.midPoint();
                referencePoint=std::make_unique<Coord::CombinedPoint>(
                    llref,
                    Coord::latLonToWorld(llref)
                );
                extent=LLBox.toWorld();
                if (extent.xmax < extent.xmin){
                    LOG_ERROR("invalid chart extent in %s: latlon=%s, world=%s",
                        fileName,LLBox.toString(),extent.toString()
                    );
                    //TODO: should we throw here?
                    //the chart will never be found any way...
                }
            }
            break;
            case OSENC_RecordNativeScale::code():
            {
                OSENC_RecordNativeScale reader(&buffer);
                nativeScale=reader.GetBuffer()->scale;
                if (nativeScale <= 0) throw ValueException(fileName,FMT("invalid native scale %d",nativeScale));
                
            }
            break;
            case OSENC_RecordVersion::code():
            {
                OSENC_RecordVersion reader(&buffer);
                sencVersion=reader.GetBuffer()->version;
                if (sencVersion == 1024) throw VersionException(fileName, FMT("version %d (E3)",sencVersion));
                if (sencVersion < 200 || sencVersion > 299) throw VersionException(fileName,FMT("version %d (E4)",sencVersion));
            }
            break;


            case OSENC_RecordFeatureId::code():
            {
                OSENC_RecordFeatureId reader(&buffer);
                OSENC_RecordFeatureId::Type *p=reader.GetBuffer();
                if (currentObject){
                    s57Objects.push_back(currentObject);
                    currentObject.reset();
                }
                currentObject=ocalloc::allocate_shared_pool<S57Object>(poolRef,
                    p->feature_ID,p->feature_type_code,p->feature_primitive);                
            }
            break;
            case OSENC_RecordAttribute::code():
            {
                OSENC_RecordAttribute reader(&buffer);
                OSENC_RecordAttribute::Type *p=reader.GetBuffer();
                if (! currentObject){
                    break;
                }
                uint16_t tc=p->attribute_type_code;
                if (! s52data->hasAttribute(tc)){
                    LOG_DEBUG("unkown attribute id=%d",tc);
                    break;
                }
                reader.SetBufferPointer(offsetof(OSENC_RecordAttribute::Type,attribute_value));
                switch(p->attribute_value_type){
                    case 0:
                        {   
                        uint32_t v;
                        reader.GetFromBuffer(&v,1);
                        currentObject->attributes.set(tc,s52::Attribute(poolRef,tc,v));
                        if (tc == 133){ //SCAMIN
                            currentObject->scamin=v;
                        }
                        }
                        break;
                    case 2:
                        {
                        double v=0;
                        reader.GetFromBuffer(&v,1);
                        currentObject->attributes.set(tc,s52::Attribute(poolRef,tc,v));
                        }
                        break;
                    case 4:
                        {
                            size_t slen=0;
                            char *v=reader.GetString(slen);
                            currentObject->attributes.set(tc,s52::Attribute(poolRef,tc,v,slen));
                        }
                        break;
                    default:
                        LOG_DEBUG("unknown attribute type %d for object %d, attribute %d",p->attribute_value_type,currentObject->featureTypeCode,tc);
                }
            }
            break;
            case OSENC_RecordAreaGeometry::code():
            {
                if (! currentObject){
                    break;
                }
                if (! referencePoint) throw InvalidChartException(fileName,"area record before extent record");
                OSENC_RecordAreaGeometry reader(&buffer);
                parseAreaRecord<OSENC_RecordAreaGeometry,false>(reader,currentObject.get(),aboveV200(),poolRef,referencePoint->worldPoint);  
            }
            break;
            case OSENC_RecordExtAreaGeometry::code():
            {
                if (! currentObject){
                    break;
                }
                if (! referencePoint) throw InvalidChartException(fileName,"ext area record before extent record");
                OSENC_RecordExtAreaGeometry reader(&buffer);
                auto record=reader.GetBuffer();
                double scale=record->scaleFactor;
                if (std::abs(scale) < 1e-5) scale=1;
                parseAreaRecord<OSENC_RecordExtAreaGeometry,true>(reader,currentObject.get(),aboveV200(),poolRef,referencePoint->worldPoint,scale);  
            }
            break;
            case OSENC_RecordMultiPointGeometry::code():
            {
                if (! currentObject){
                    break;
                }
                if (! referencePoint) throw InvalidChartException(fileName,"multipoint record before extent record");
                OSENC_RecordMultiPointGeometry reader(&buffer);
                OSENC_RecordMultiPointGeometry::Type *p = reader.GetBuffer();
                Coord::LLBox ext;
                ext.e_lon=p->extent_e_lon;
                ext.w_lon=p->extent_w_lon;
                ext.n_lat=p->extent_n_lat;
                ext.s_lat=p->extent_s_lat;
                currentObject->extent=ext.toWorld();
                currentObject->geoPrimitive=s52::GEO_POINT;
                uint32_t numPoints=p->point_count;
                if (numPoints < 1) break;
                reader.SetBufferPointer(offsetof(OSENC_RecordMultiPointGeometry::Type,point_count)+sizeof(uint32_t));
                for (int i=0;i<numPoints;i++){
                    float temp[3];
                    reader.GetFromBuffer(&temp[0],3,"multi point geometry");
                    S57Object::Sounding s=Coord::worldFromSM(temp[0],temp[1],referencePoint->worldPoint);
                    s.depth=temp[2];
                    currentObject->soundigs.add(s);
                    currentObject->extent.extend(s);
                }
                break;
            }
            case OSENC_RecordPointGeometry::code():
            {
                if (! currentObject){
                    break;
                }
                OSENC_RecordPointGeometry reader(&buffer);
                OSENC_RecordPointGeometry::Type *p=reader.GetBuffer();
                currentObject->geoPrimitive=s52::GEO_POINT;
                currentObject->point=Coord::latLonToWorld(p->lat,p->lon);
                currentObject->isAton=setRigidFloat(currentObject.get());
            }
            break;
            case OSENC_RecordLineGeometry::code():
            {
                if (! currentObject){
                    break;
                }
                if (currentObject->lines.size() > 0){
                    LOG_ERROR("%s: muiltiple line geometries for object %d",fileName,currentObject->featureId);
                    currentObject->lines.clear();
                }
                if (! referencePoint) throw InvalidChartException(fileName,"line geometry record before extent record");
                OSENC_RecordLineGeometry reader(&buffer);
                auto p=reader.GetBuffer();
                Coord::LLBox ext;
                ext.e_lon=p->extent_e_lon;
                ext.w_lon=p->extent_w_lon;
                ext.n_lat=p->extent_n_lat;
                ext.s_lat=p->extent_s_lat;
                currentObject->extent=ext.toWorld();
                //set the object center to the middle of the extent
                currentObject->point=currentObject->extent.midPoint();
                currentObject->geoPrimitive=s52::GEO_LINE;
                int stride=aboveV200()?4:3;
                reader.SetBufferPointer(offsetof(OSENC_RecordLineGeometry::Type,edgeVector_count)+sizeof(uint32_t));
                for (uint32_t i=0;i<p->edgeVector_count;i++){
                    int indices[4];
                    reader.GetFromBuffer(&indices[0],stride,"line index entry");
                    currentObject->lines.push_back(S57Object::LineIndex(indices,aboveV200()));
                }
            }
            break;
            case OSENC_RecordEdgeVectors::code():
            {
                OSENC_RecordEdgeVectors reader(&buffer);
                if (! referencePoint) throw InvalidChartException(fileName,"edge node record before extent record");
                parseEdgeVectors<OSENC_RecordEdgeVectors,false>(fileName,reader,edgeNodeTable,poolRef,referencePoint->worldPoint);
            }
            break;
            case OSENC_RecordEdgeVectorsExt::code():
            {
                OSENC_RecordEdgeVectorsExt reader(&buffer);
                if (! referencePoint) throw InvalidChartException(fileName,"edge node ext record before extent record");
                auto record=reader.GetBuffer();
                double scale=record->scaleFactor;
                if (std::abs(scale) < 1e-5) scale=1;
                parseEdgeVectors<OSENC_RecordEdgeVectorsExt,true>(fileName,reader,edgeNodeTable,poolRef,referencePoint->worldPoint);
            }
            break;
            case OSENC_RecordNodeVectors::code():
            {
                if (! currentObject){
                    break;
                }
                if (! referencePoint) throw InvalidChartException(fileName,"node vector record before extent record");
                OSENC_RecordNodeVectors reader(&buffer);
                parseNodeVectors<OSENC_RecordNodeVectors,false>(reader, connectedNodeTable,poolRef, referencePoint->worldPoint);
            }
            break;
            case OSENC_RecordNodeVectorsExt::code():
            {
                if (! currentObject){
                    break;
                }
                if (! referencePoint) throw InvalidChartException(fileName,"node vector ext record before extent record");
                OSENC_RecordNodeVectorsExt reader(&buffer);
                auto record=reader.GetBuffer();
                double scale=record->scaleFactor;
                if (std::abs(scale) < 1e-5) scale=1;
                parseNodeVectors<OSENC_RecordNodeVectorsExt,true>(reader, connectedNodeTable,poolRef, referencePoint->worldPoint,scale);
            }
            break;
            case OSENC_RecordCellEdition::code():
            {
                OSENC_RecordCellEdition reader(&buffer);
                cellEdition=reader.GetBuffer()->edition;

            }
            break;
            case OSENC_RecordTXTDSCInfo::code():
            {
                OSENC_RecordTXTDSCInfo reader(&buffer);
                size_t len=reader.GetBuffer()->content_length;
                size_t namelen=reader.GetBuffer()->file_name_length;
                reader.SetBufferPointer(OFFSET_AFTER(OSENC_TXTDSCInfo_Record_Payload,content_length));
                ocalloc::String name(poolRef);
                if (namelen > 0){
                    name.assign(reader.GetString(namelen,"txtdsc name",namelen));
                }
                ocalloc::String txt(poolRef);
                if (len > 0){
                    txt.assign(reader.GetString(len,"txtdsc string",len));    
                }
                if (! name.empty() && ! txt.empty()){
                    txtdscTable.set(name,txt);
                }
            }
            break;
            default:
                unknownRecords.insert(recordType);
                //LOG_DEBUG("%s: unknown record %d",fileName,recordType);
        }

    }
    LOG_ERROR("%s: unexpected end of chart stream",fileName);
    return false;
}

class CurrentPolygon{
    public:
    int startIndex=0;
    int lastIndex=0;
    double sum=0; //for winding direction
    Coord::WorldXy startPoint;
    Coord::WorldXy lastPoint;
    int currentConnTableIndex=0;
    bool isComplete=true;
    void addPoint(Coord::WorldXy p){
        if (p == lastPoint) return;
        //https://stackoverflow.com/questions/1165647/how-to-determine-if-a-list-of-polygon-points-are-in-clockwise-order
        //sum += ((double)p.x-(double)lastPoint.x)*((double)p.y+(double)lastPoint.y);
        //dfSum += ptp[iseg].x * ptp[iseg+1].y - ptp[iseg].y * ptp[iseg+1].x;
        sum+=((double)lastPoint.x)*((double)p.y)-((double)lastPoint.y)*((double)p.x);
        lastPoint=p;
    }
    CurrentPolygon(int startLineIndex, Coord::WorldXy p, int connIndex):
        startIndex(startLineIndex),
        lastIndex(startLineIndex),
        currentConnTableIndex(connIndex), 
        startPoint(p),
        lastPoint(p){}
    void updateLineIndex(int index){
        lastIndex=index;
    }
    S57Object::Polygon::Winding finalize(){
        if (! (startPoint == lastPoint)){
            addPoint(startPoint);
            isComplete=false;
        }
        S57Object::Polygon::Winding wd=(sum<0)?S57Object::Polygon::WD_CW:S57Object::Polygon::WD_CC;
        if (wd == S57Object::Polygon::WD_CW){
            int debug=1;
        }
        else{
            int debug=2;
        }
        return wd;    
    }
};

static void addPolygonToObject(CurrentPolygon *polygon,S57Object *object){
    S57Object::Polygon::Winding wd=polygon->finalize();
                        object->polygons.push_back(
                            S57Object::Polygon(
                                object,
                                polygon->startIndex,
                                polygon->lastIndex,
                                wd,
                                polygon->isComplete
                            )
                        );
}

void OESUChart::buildLineGeometries(){
    //must be called when all objects and vectors are read
    //refer to AssembleLineGeometry in eSENCChart.cpp
    for (auto it=s57Objects.begin();it!=s57Objects.end();it++){
        S57Object *obj=(*it).get();
        if (obj->lines.size() < 1) continue;
        std::unique_ptr<CurrentPolygon> polygon;
        for (auto line=obj->lines.begin();line != obj->lines.end();line++){
            auto firstConPoint=connectedNodeTable.find(line->first);
            auto lastConPoint=connectedNodeTable.find(line->endNode);
            auto edgeNodes=edgeNodeTable.find(line->vectorEdge);
            if (edgeNodes != edgeNodeTable.end()){
                //we are sure that the edge nodes are not changed any more
                //so we can safely get pointers to them
                line->edgeNodes=&(edgeNodes->second.points);
                for (auto it=edgeNodes->second.points.begin();
                        it != edgeNodes->second.points.end();it++){
                            obj->extent.extend(*it);
                        }
            }
            if (firstConPoint == connectedNodeTable.end()){
                int debug=1;
                LOG_DEBUG("line segment without start node c=%s obj=%d",fileName,obj->featureId);
            }
            if (firstConPoint != connectedNodeTable.end()){
                obj->extent.extend(firstConPoint->second);
                if (edgeNodes != edgeNodeTable.end() && edgeNodes->second.points.size() > 0){
                    line->startSegment.start=firstConPoint->second;
                    line->startSegment.end=line->forward?
                        edgeNodes->second.points[0]:
                        edgeNodes->second.points[edgeNodes->second.points.size()-1];
                    line->startSegment.valid=true;    
                }
            }
            if (lastConPoint == connectedNodeTable.end()){
                int debug=1;
                LOG_DEBUG("line segment without end node c=%s obj=%d",fileName,obj->featureId);
            }
            if (lastConPoint != connectedNodeTable.end() && firstConPoint != connectedNodeTable.end()){
                obj->extent.extend(lastConPoint->second);
                if (edgeNodes != edgeNodeTable.end() && edgeNodes->second.points.size() > 0){
                    line->endSegment.end=lastConPoint->second;
                    line->endSegment.start=line->forward?
                        edgeNodes->second.points[edgeNodes->second.points.size()-1]:
                        edgeNodes->second.points[0];
                    line->endSegment.valid=true;
                }
                else{
                    line->endSegment.start=firstConPoint->second;
                    line->endSegment.end=lastConPoint->second;
                    line->endSegment.valid=true;
                }
            }
            //check if we can continue a polygon
            if (polygon){
                if ((line->startSegment.valid && 
                        line->first == polygon->currentConnTableIndex)
                         ||
                        line->firstPoint() == polygon->lastPoint
                    ){
                       ;//we can continue
                    }
                    else{
                        addPolygonToObject(polygon.get(),obj);
                        polygon.reset();
                    }
            }
            if (! polygon){
                polygon.reset(new CurrentPolygon(std::distance(obj->lines.begin(),line),line->firstPoint(),line->first));
            }
            else{
                polygon->updateLineIndex(std::distance(obj->lines.begin(),line));
            }
            line->iterateSegments([&polygon](Coord::WorldXy start,Coord::WorldXy end,bool isFirst){
                //the polygon simply ignores any duplicate points so we can safely add all of them
                polygon->addPoint(start);
                polygon->addPoint(end);
            });
            if (line->endSegment.valid){
                polygon->currentConnTableIndex=line->endNode;
            }
            else{
                polygon->currentConnTableIndex=-1;
            }
        }
        if (polygon){
            addPolygonToObject(polygon.get(),obj);
            polygon.reset();
        }
    }
}

static std::unordered_set<uint16_t> BOYS({
    S57ObjectClasses::BOYCAR, //BOYCAR
    S57ObjectClasses::BOYINB, //BOYINB
    S57ObjectClasses::BOYISD, //BOYISD
    S57ObjectClasses::BOYLAT, //BOYLAT
    S57ObjectClasses::BOYSAW, //BOYSAW
    S57ObjectClasses::BOYSPP, //BOYSPP
    S57ObjectClasses::boylat, //boylat
    S57ObjectClasses::boywtw //boywtw
});
static std::unordered_set<uint16_t> BCN({
    S57ObjectClasses::BCNCAR, //BCNCAR
    S57ObjectClasses::BCNISD, //BCNISD
    S57ObjectClasses::BCNLAT, //BCNLAT
    S57ObjectClasses::BCNSAW, //BCNSAW
    S57ObjectClasses::BCNSPP, //BCNSPP
    S57ObjectClasses::bcnlat, //bcnlat
    S57ObjectClasses::bcnwtw //bcnwtw
});


bool OESUChart::setRigidFloat(const S57Object *obj){
    bool isFloating=obj->featureTypeCode == S57ObjectClasses::LITVES||
        obj->featureTypeCode == S57ObjectClasses::LITFLT||
        BOYS.find(obj->featureTypeCode) !=BOYS.end();
    bool isRigid=BCN.find(obj->featureTypeCode) != BCN.end();
    if (isFloating) {
        floatAtons.insert(obj->point);
    }
    if (isRigid){
        rigidAtons.insert(obj->point);
    }
    return isFloating || isRigid || obj->featureTypeCode == S57ObjectClasses::LIGHTS;    
}

OESUChart::~OESUChart(){
    int x=1;
}
bool OESUChart::PrepareRender(s52::S52Data::ConstPtr s52data){
    LOG_DEBUG("%s: prepareRender",fileName);
    if (! s52data) throw FileException(fileName,"s52data not set in prepareRender");
    //TEMP - can already been done after loading
    buildLineGeometries();
    //currently we cannot recreate the render data as our allocator is 
    //bound to the chart and we are unable to drop the render data allone
    //additionally we have to ensure that only on thread at a time
    //is working with our allocator any way
    renderData=ocalloc::allocate_shared_pool<RenderData>(poolRef,s52data);
    RenderSettings::ConstPtr rs=s52data->getSettings();
    s52::LUPname boundaryStyle=rs->nBoundaryStyle;
    s52::LUPname symbolStyle=rs->nSymbolStyle;
    //compute the best matching safety contour
    //see eSencChart::BuildDepthContourArray
    double safetyDepth=rs->S52_MAR_SAFETY_CONTOUR;
    for (auto it=s57Objects.begin();it != s57Objects.end();it++){
        if ((*it)->featureTypeCode==S57ObjectClasses::DEPCNT){
            double valdco=1e6;
            if ((*it)->attributes.getDouble(S57AttrIds::VALDCO,valdco)){
                if (valdco >= safetyDepth && valdco < renderData->nextSafetyContour){
                    renderData->nextSafetyContour=valdco;
                }
            }
        }
    }
    for (auto it=s57Objects.begin();it != s57Objects.end();it++){
        const s52::LUPrec *LUP=nullptr;
        s52::LUPname LUP_Name = s52::PAPER_CHART;

        switch( (*it)->geoPrimitive ){
            case s52::GEO_POINT:
            case s52::GEO_META:
            case s52::GEO_PRIM:
                if( symbolStyle == s52::LUPname::PAPER_CHART)
                    LUP_Name = s52::PAPER_CHART;
                else
                    LUP_Name = s52::SIMPLIFIED;
                break;
            case s52::GEO_LINE:
                LUP_Name = s52::LINES;
                break;

            case s52::GEO_AREA:
                if( boundaryStyle == s52::LUPname::PLAIN_BOUNDARIES)
                    LUP_Name = s52::PLAIN_BOUNDARIES;
                else
                    LUP_Name = s52::SYMBOLIZED_BOUNDARIES;
                break;
        }
        LUP=s52data->findLUPrecord(LUP_Name,(*it)->featureTypeCode,&((*it)->attributes));
        //LOG_DEBUG("chart object %d->%s: LUP RCID %d",(*it)->featureTypeCode, s52data->objectName((*it)->featureTypeCode),LUP?LUP->RCID:-1);
        if (! LUP) {
            ;
            //ignore this object
        }
        else{
            S57Object::RenderObject::Ptr renderObject=ocalloc::allocate_shared_pool<S57Object::RenderObject>(poolRef,(*it));        
            s52::RuleConditions conditions;
            conditions.geoPrimitive=(*it)->geoPrimitive;
            conditions.attributes=&((*it)->attributes);
            conditions.nextSafetyContour=renderData->nextSafetyContour;
            conditions.featureTypeCode=(*it)->featureTypeCode;
            if ((*it)->geoPrimitive == s52::GEO_POINT){
                //floating base if there is a floating object at the same coordinates - but not a rigid
                //see TOPMAR01 rule in orig plugin code
                conditions.hasFloatingBase=floatAtons.find((*it)->point) != floatAtons.end() && rigidAtons.find((*it)->point) == rigidAtons.end();
            }
            renderObject->SetLUP(LUP);
            renderObject->expand(s52data.get(), &(renderData->ruleCreator), &conditions);
            if (renderObject->shouldRenderCat(s52data->getSettings().get())){
                renderData->addObject(renderObject);
            }
        }
    }
    //sort by display priority
    std::stable_sort(renderData->renderObjects.begin(),renderData->renderObjects.end(),
        [](const S57Object::RenderObject::Ptr &left,const S57Object::RenderObject::Ptr &right){
            return left->GetDisplayPriority() < right->GetDisplayPriority();
        });
    LOG_DEBUG("%s: prepareRender with %d objects",fileName,renderData->renderObjects.size());
    renderData=renderData;
    return true;
}
static std::vector<s52::RenderStep> renderSteps({
    s52::RenderStep::RS_AREAS2,
    s52::RenderStep::RS_LINES,
    s52::RenderStep::RS_POINTS
    });
class OESURenderContext: public ChartRenderContext{
    String name;
    public:
        using ObjectList=std::vector<const S57Object::RenderObject*>;
        ObjectList matchingObjects;
        ObjectList::iterator last;
        ObjectList::iterator first;
        int lastPriority=-1;
        OESURenderContext(String n):name(n){
            last=matchingObjects.end();
            first=matchingObjects.end();
        }
        void prepare(){
            last=matchingObjects.begin();
            first=matchingObjects.begin();
        }
        virtual ~OESURenderContext(){
            matchingObjects.clear();
            LOG_DEBUG("ChartRenderCtx %s destroy",name);
        }
};
int OESUChart::getRenderPasses() const{
    return s52::PRIO_NUM * renderSteps.size() +2; //see comment below about passes
}
Chart::RenderResult OESUChart::Render(int pass,RenderContext & renderCtx, DrawingContext &ctx, Coord::TileBox const &tile) const
{
    /**
     * render flow:
     * (1) render areas only (AC)
     * (2) symbolized areas
     * (3) iterate over priorities
     *     on each priority:
     *     (2) render areas - all other rules (tables PLAIN_BOUNDARIES/SYMBOLIZED_BOUNDARIES)
     *     (3) render lines (table LINES)
     *     (4) render points (tables PAPER_CHART/SIMPLIFIED)       
     **/
    //render objects are sorted by prio
    //1st step: render all areas
    //we use this to keep in mind the objects that intersect
    //as the list of objects does not change we can safely use pointers
    // the renderer mus ensure to hold a reference to the chart
    //during the complete render process
    RenderData* currentRenderData=renderData.get(); 
    if (! currentRenderData){
        throw AvException(FMT("chart %s not prepared for render",fileName));
    }
    if (currentRenderData->renderObjects.size() < 1){
        LOG_DEBUG("chart %s has nothing to render",fileName);
        return RenderResult::ROK;
    }
    if (pass >= getRenderPasses()){
        LOG_DEBUG("%s: invalid render pass %d",fileName,pass);
        return RenderResult::ROK;
    }
    OESURenderContext *chartCtx=nullptr;
    if (!renderCtx.chartContext)
    {   
        if (pass != 0){
            throw AvException(FMT("%s: pass %d without chart context",fileName,pass));
        }
        chartCtx = new OESURenderContext(fileName);
        renderCtx.chartContext.reset(chartCtx);
        for (auto it = currentRenderData->renderObjects.begin(); it != currentRenderData->renderObjects.end(); it++)
        {
            if (!(*it)->Intersects(renderCtx.tileExtent, tile))
            {
                continue;
            }
            chartCtx->matchingObjects.push_back((*it).get());
            //we assume step 0 here any way
            (*it)->Render(renderCtx, ctx, tile, s52::RS_AREAS1);
        }
        chartCtx->prepare();
        return RenderResult::ROK;
    }
    chartCtx=(OESURenderContext*)(renderCtx.chartContext.get());
    if (pass < 1){
        throw AvException(FMT("%s: pass 0 with existing chartContext",fileName));
    }
    pass--;
    if (pass == 1){
        //symbolized areas
        for (auto &&ro:chartCtx->matchingObjects){
            ro->Render(renderCtx, ctx, tile, s52::RS_AREASY);
        }
        return RenderResult::ROK;
    }
    //now all the rest priority based
    pass--;
    int prio = pass / renderSteps.size();
    int step=pass - prio * renderSteps.size();
    if (step >= renderSteps.size()){
        throw AvException(FMT("%s: invalid renderStep %d",fileName,step));
    }
    if (prio >= s52::PRIO_NUM){
        throw AvException(FMT("%s: invalid priority %d",fileName,prio));
    }
    if (chartCtx->first == chartCtx->matchingObjects.end()){
        return RenderResult::ROK;
    }
    s52::RenderStep renderStep=renderSteps[step];
    if (prio < (*(chartCtx->first))->GetDisplayPriority()){
        return RenderResult::ROK;
    }
    if (prio > (*(chartCtx->first))->GetDisplayPriority()){
        //new priority
        chartCtx->first=chartCtx->last;
        while (chartCtx->first != chartCtx->matchingObjects.end() && (*(chartCtx->first))->GetDisplayPriority() < prio){
            chartCtx->first++;
        }
        chartCtx->last=chartCtx->first;
    }
    if (chartCtx->first == chartCtx->matchingObjects.end()){
        //only smaller priorities in list
        return RenderResult::ROK;
    }
    if (prio == (*(chartCtx->first))->GetDisplayPriority()){
        chartCtx->last=chartCtx->first;
        while (chartCtx->last != chartCtx->matchingObjects.end() && (*(chartCtx->last))->GetDisplayPriority() == prio ){
            (*(chartCtx->last))->Render(renderCtx,ctx,tile,renderStep);
            chartCtx->last++;
        }
        return RenderResult::ROK;
    }
    //seems that we only habe higher priorities in the list
    return RenderResult::ROK;
}
MD5Name OESUChart::GetMD5() const {
    if (! renderData || !renderData->s52data) return md5;
    return renderData->s52data->getMD5();
}
void OESUChart::LogInfo(const String &prefix) const{
    apool->logStatistics(prefix);
}

class OESUChartDescription: public ChartDescription{
    public:
    using ChartDescription::ChartDescription;
    int scale=-1;
    uint16_t cellEdition=-1;
    int sencVersion=-1;
    OESUChart::HeaderInfo info;
    //add chart info
    virtual void toJson(json::JSON &js) const override{
        ChartDescription::toJson(js);
        js["Scale"]=scale;
        js["Edition"]=cellEdition;
        js["SencVersion"]=sencVersion;
        js["Created"]=info.createdDate;
        js["CName"]=info.cellName;
        js["PublishDate"]=info.cellPublishDate;
        js["UpdateDate"]=info.cellUpdateDate;
        js["Update"]=info.cellUpdate;
        js["Sounding"]=info.soundingDatum;
    }
    virtual void jsonOverview(json::JSON &js) const override{
        ChartDescription::jsonOverview(js);
    }
};

ObjectList OESUChart::FeatureInfo(RenderContext & context,DrawingContext &drawing, const Coord::TileBox &box,bool overview) const{
    Coord::WorldXy ref=box.midPoint();
    ObjectList rt;
    std::shared_ptr<OESUChartDescription> cd=std::make_shared<OESUChartDescription>(FileHelper::fileName(fileName,true),setKey);
    cd->scale=nativeScale;
    cd->cellEdition=cellEdition;
    cd->sencVersion=sencVersion;
    cd->info=headerInfo;
    rt.push_back(cd);
    RenderData *currentRenderData=renderData.get();
    context.chartContext.reset();
    for (auto &it : currentRenderData->renderObjects){
        ObjectDescription::Ptr description=it->getObjectDescription(context,drawing,box,overview,
            [this](const String &name){
                ocalloc::String key(this->txtdscTable.get_allocator());
                key.assign(name.c_str());
                auto it=this->txtdscTable.find(key);
                if (it != this->txtdscTable.end()) return String(it->second.c_str());
                return String();
            });
        if (description){
            description->computeDistance(ref);
            if (! overview){
                description->addValue("chart",FileHelper::fileName(fileName,true));
            }
            rt.push_back(description);
        }
    }
    return rt;
}