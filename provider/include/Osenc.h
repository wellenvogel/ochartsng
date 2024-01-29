/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Osenc structures
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
#ifndef _INCLUDEOSENC_H
#define _INCLUDEOSENC_H
#include "Types.h"
#include "Exception.h"
#include "FileHelper.h"

//  OSENC V2 record definitions
#define HEADER_SENC_VERSION             1
#define HEADER_CELL_NAME                2
#define HEADER_CELL_PUBLISHDATE         3
#define HEADER_CELL_EDITION             4
#define HEADER_CELL_UPDATEDATE          5
#define HEADER_CELL_UPDATE              6
#define HEADER_CELL_NATIVESCALE         7
#define HEADER_CELL_SENCCREATEDATE      8
#define HEADER_CELL_SOUNDINGDATUM       9

#define FEATURE_ID_RECORD               64
#define FEATURE_ATTRIBUTE_RECORD        65

#define FEATURE_GEOMETRY_RECORD_POINT           80
#define FEATURE_GEOMETRY_RECORD_LINE            81
#define FEATURE_GEOMETRY_RECORD_AREA            82
#define FEATURE_GEOMETRY_RECORD_MULTIPOINT      83
#define FEATURE_GEOMETRY_RECORD_AREA_EXT        84

#define VECTOR_EDGE_NODE_TABLE_EXT_RECORD        85
#define VECTOR_CONNECTED_NODE_TABLE_EXT_RECORD   86

#define VECTOR_EDGE_NODE_TABLE_RECORD             96
#define VECTOR_CONNECTED_NODE_TABLE_RECORD        97

#define CELL_COVR_RECORD                        98
#define CELL_NOCOVR_RECORD                      99
#define CELL_EXTENT_RECORD                      100
#define CELL_TXTDSC_INFO_FILE_RECORD            101

#define SERVER_STATUS_RECORD                    200

//--------------------------------------------------------------------------
//      Utility Structures
//--------------------------------------------------------------------------

class IRecordBuffer{
    public:
    DECL_EXC(FileException,NoDataException);
    DECL_EXC(FileException,OutOfBufferException);
    virtual char * WriteToBuffer(InputStream::Ptr,uint32_t len)=0;
    virtual uint32_t GetFill()=0;
    virtual char* GetBuffer()=0;
    virtual const String & GetFileName()=0;
    virtual ~IRecordBuffer(){}
};
template<typename T, uint16_t Code>
class OsencRecord{
    protected:
    IRecordBuffer *buffer=NULL;
    uint32_t bufferFill=0;
    char *bufferData=nullptr;
    size_t bufferPtr=0;
    size_t savedPtr=0;
    public:
        using Type=T;
        OsencRecord(IRecordBuffer *b):buffer(b){
            bufferFill=b->GetFill();
            bufferData=b->GetBuffer();
        }
        static const constexpr uint16_t code(){ return Code;}
        inline T* GetBuffer(){
            if (bufferFill < sizeof(T)){
                throw IRecordBuffer::NoDataException(
                    buffer->GetFileName(),
                    FMT("record type %d too short, expected %d, got %d",
                    Code,
                    sizeof(T),
                    bufferFill
                ));
            }
            return (T*)(bufferData);
        }
        inline uint32_t GetFill(){return bufferFill;}
        void SetBufferPointer(size_t offset){
            if (offset >= bufferFill) throw IRecordBuffer::OutOfBufferException(
                buffer->GetFileName(),
                FMT("record type %d nout enough data in buffer, expected %d, current %d",
                Code, offset, bufferFill));
            bufferPtr=offset;    
        }
        void SaveBufferPointer(){
            savedPtr=bufferPtr;
        }
        void RestoreBufferPointer(){
            bufferPtr=savedPtr;
        }
        template <typename DT>
        inline void GetFromBuffer(DT * target, size_t number=1, const char *info=NULL){
            size_t requested=sizeof(DT)*number;
            if (bufferPtr+requested > bufferFill) throw IRecordBuffer::OutOfBufferException(
                buffer->GetFileName(),
                FMT("record type %d - length too short expected %d at offset %d [%s], available %d",
                Code, requested,bufferPtr,info?info:"",buffer->GetFill()-bufferPtr)
            );
            size_t current=bufferPtr;
            bufferPtr+=requested;
            memcpy(target, bufferData + current,requested);
        }
        inline char * GetString(size_t &available /*out*/, const char *info=NULL, size_t maxLen=0){
            if (bufferPtr >= bufferFill) throw IRecordBuffer::OutOfBufferException(
                buffer->GetFileName(),
                FMT("record type %d - length too short expected string at offset %d[%s], available %d",
                    Code, bufferPtr,info?info:"",buffer->GetFill()-bufferPtr)
                );
            available=bufferFill-bufferPtr;
            if (maxLen > 0 && available > maxLen) available=maxLen;
            size_t current=bufferPtr;
            bufferPtr+=available;
            if (available > 0){
                //remove the trailing '0' from the length
                while (available > 0 &&  (*(bufferData+current+available-1) == 0)) available--;
            }
            return bufferData+current;
        }
        template <typename DS>
        inline void skip(size_t number=1){
            //no checks here
            bufferPtr+=sizeof(DS)*number;
        }
        inline void skipInt(size_t number=1){
            skip<int>(number);
        }
        inline void skipDouble(size_t number=1){
            skip<double>(number);
        }
        #define OFFSET_AFTER(type,member) (offsetof(type,member)+sizeof(((type*)nullptr)->member))
};

#pragma pack(push,1)

typedef struct{
    uint16_t        record_type;
    uint32_t        record_length;
}_OSENC_Record_Base;

class OSENC_RecordBase: public OsencRecord<_OSENC_Record_Base,0>{
    using OsencRecord::OsencRecord;
    public:
    uint32_t GetLength(){
        if (buffer->GetFill() < sizeof(Type)) return 0;
        uint32_t raw=GetBuffer()->record_length;
        if (raw == 0){
            return 0; //end of data
        }
        if (raw  < sizeof(Type)) throw AvException("RecordBase: record length shorter then header");
        return raw-sizeof(Type);
    }
};
typedef struct {
    uint16_t version;
}_OSENC_RecordVersion;

typedef OsencRecord<_OSENC_RecordVersion,HEADER_SENC_VERSION> OSENC_RecordVersion;

typedef struct{
    char name; //can be the rest of the buffer
}_OSENC_RecordName;

template<u_int16_t Code>
class OSENC_RecordName : public OsencRecord<_OSENC_RecordName,Code>{
    using OsencRecord<_OSENC_RecordName,Code>::OsencRecord;
    public:
    String GetName(){
        auto p=this->GetBuffer();
        char *np=&(p->name);
        char *ne=np;
        while (*ne != 0 && (ne-np) < this->GetFill()) ne++;
        return String(np,(ne-np));
    }
};
using OSENC_RecordCellName=OSENC_RecordName<HEADER_CELL_NAME>;
using OSENC_RecordCellPublishDate=OSENC_RecordName<HEADER_CELL_PUBLISHDATE>;
using OSENC_RecordCellUpdateDate=OSENC_RecordName<HEADER_CELL_UPDATEDATE>;
using OSENC_RecordCellCreatedDate=OSENC_RecordName<HEADER_CELL_SENCCREATEDATE>;
using OSENC_RecordSoundingDatum=OSENC_RecordName<HEADER_CELL_SOUNDINGDATUM>;


typedef struct {
    uint32_t scale;
} _OSENC_RecordNativeScale;
typedef OsencRecord<_OSENC_RecordNativeScale,HEADER_CELL_NATIVESCALE> OSENC_RecordNativeScale;

typedef struct _OSENC_Record{
    uint16_t        record_type;
    uint32_t        record_length;
    unsigned char   payload;
}OSENC_Record;





typedef struct _OSENC_Feature_Identification_Record_Payload{
    uint16_t        feature_type_code;
    uint16_t        feature_ID;
    uint8_t         feature_primitive;
}OSENC_Feature_Identification_Record_Payload;

typedef OsencRecord<OSENC_Feature_Identification_Record_Payload,FEATURE_ID_RECORD> OSENC_RecordFeatureId;

typedef struct _OSENC_Attribute_Record_Payload{
    uint16_t        attribute_type_code;
    unsigned char   attribute_value_type;
    //could be double, uint32_t or ASCII string
    char            attribute_value;
}OSENC_Attribute_Record_Payload;
typedef OsencRecord<OSENC_Attribute_Record_Payload,FEATURE_ATTRIBUTE_RECORD> OSENC_RecordAttribute;


typedef struct _OSENC_TXTDSCInfo_Record_Payload{
    uint32_t        file_name_length;
    uint32_t        content_length;
    char *          data;
}OSENC_TXTDSCInfo_Record_Payload;
typedef OsencRecord<OSENC_TXTDSCInfo_Record_Payload,CELL_TXTDSC_INFO_FILE_RECORD> OSENC_RecordTXTDSCInfo;

typedef struct{
    double          lat;
    double          lon;
}OSENC_PointGeometry_Record_Payload;
typedef OsencRecord<OSENC_PointGeometry_Record_Payload,FEATURE_GEOMETRY_RECORD_POINT> OSENC_RecordPointGeometry;


typedef struct _OSENC_MultipointGeometry_Record_Payload{
    double          extent_s_lat;
    double          extent_n_lat;
    double          extent_w_lon;
    double          extent_e_lon;
    uint32_t        point_count;
    //void *          payLoad; //point_count could b 0...
}OSENC_MultipointGeometry_Record_Payload;
typedef OsencRecord<OSENC_MultipointGeometry_Record_Payload,FEATURE_GEOMETRY_RECORD_MULTIPOINT> OSENC_RecordMultiPointGeometry;



typedef struct _OSENC_LineGeometry_Record_Payload{
    double          extent_s_lat;
    double          extent_n_lat;
    double          extent_w_lon;
    double          extent_e_lon;
    uint32_t        edgeVector_count;
    //void *          payLoad;
}OSENC_LineGeometry_Record_Payload;
typedef OsencRecord<OSENC_LineGeometry_Record_Payload,FEATURE_GEOMETRY_RECORD_LINE> OSENC_RecordLineGeometry;


typedef struct _OSENC_AreaGeometry_Record_Payload{
    double          extent_s_lat;
    double          extent_n_lat;
    double          extent_w_lon;
    double          extent_e_lon;
    uint32_t        contour_count;
    uint32_t        triprim_count;
    uint32_t        edgeVector_count;
    void *          payLoad;
}OSENC_AreaGeometry_Record_Payload;

typedef OsencRecord<OSENC_AreaGeometry_Record_Payload,FEATURE_GEOMETRY_RECORD_AREA> OSENC_RecordAreaGeometry;

typedef struct {
    uint16_t edition;
} OSENC_CellEdition_Record_Payload;

typedef OsencRecord<OSENC_CellEdition_Record_Payload,HEADER_CELL_EDITION> OSENC_RecordCellEdition;

typedef struct {
    uint16_t update;
} OSENC_CellUpdate_Record_Payload;

typedef OsencRecord<OSENC_CellUpdate_Record_Payload,HEADER_CELL_UPDATE> OSENC_RecordCellUpdate;

typedef struct _OSENC_AreaGeometryExt_Record_Base{
    uint16_t        record_type;
    uint32_t        record_length;
    double          extent_s_lat;
    double          extent_n_lat;
    double          extent_w_lon;
    double          extent_e_lon;
    uint32_t        contour_count;
    uint32_t        triprim_count;
    uint32_t        edgeVector_count;
    double          scaleFactor;
}OSENC_AreaGeometryExt_Record_Base;

typedef struct _OSENC_AreaGeometryExt_Record_Payload{
    double          extent_s_lat;
    double          extent_n_lat;
    double          extent_w_lon;
    double          extent_e_lon;
    uint32_t        contour_count;
    uint32_t        triprim_count;
    uint32_t        edgeVector_count;
    double          scaleFactor;
    void *          payLoad;
}OSENC_AreaGeometryExt_Record_Payload;
typedef OsencRecord<OSENC_AreaGeometryExt_Record_Payload,FEATURE_GEOMETRY_RECORD_AREA_EXT> OSENC_RecordExtAreaGeometry;
typedef struct{
    int numEntries;
} OSENC_VectorTableRecordPayload;
typedef OsencRecord<OSENC_VectorTableRecordPayload,VECTOR_EDGE_NODE_TABLE_RECORD> OSENC_RecordEdgeVectors;
typedef OsencRecord<OSENC_VectorTableRecordPayload,VECTOR_CONNECTED_NODE_TABLE_RECORD> OSENC_RecordNodeVectors;
typedef struct{
    double scaleFactor;
    int numEntries;
} OSENC_VectorTableExtRecordPayload;
typedef OsencRecord<OSENC_VectorTableExtRecordPayload,VECTOR_EDGE_NODE_TABLE_EXT_RECORD> OSENC_RecordEdgeVectorsExt;
typedef OsencRecord<OSENC_VectorTableExtRecordPayload,VECTOR_CONNECTED_NODE_TABLE_EXT_RECORD> OSENC_RecordNodeVectorsExt;

typedef struct _OSENC_COVR_Record{
    uint16_t        record_type;
    uint32_t        record_length;
    unsigned char   payload;
}_OSENC_COVR_Record;

typedef struct _OSENC_COVR_Record_Base{
    uint16_t        record_type;
    uint32_t        record_length;
}_OSENC_COVR_Record_Base;

typedef struct _OSENC_COVR_Record_Payload{
    uint32_t        point_count;
    float           point_array;
}_OSENC_COVR_Record_Payload;

typedef struct _OSENC_NOCOVR_Record{
    uint16_t        record_type;
    uint32_t        record_length;
    unsigned char   payload;
}_OSENC_NOCOVR_Record;

typedef struct _OSENC_NOCOVR_Record_Base{
    uint16_t        record_type;
    uint32_t        record_length;
}_OSENC_NOCOVR_Record_Base;

typedef struct _OSENC_NOCOVR_Record_Payload{
    uint32_t        point_count;
    float           point_array;
}_OSENC_NOCOVR_Record_Payload;


typedef struct _OSENC_EXTENT_Record{
    uint16_t        record_type;
    uint32_t        record_length;
    double          extent_sw_lat;
    double          extent_sw_lon;
    double          extent_nw_lat;
    double          extent_nw_lon;
    double          extent_ne_lat;
    double          extent_ne_lon;
    double          extent_se_lat;
    double          extent_se_lon;
 }_OSENC_EXTENT_Record;

typedef struct _OSENC_EXTENT_Record_Payload{
     double          extent_sw_lat;
     double          extent_sw_lon;
     double          extent_nw_lat;
     double          extent_nw_lon;
     double          extent_ne_lat;
     double          extent_ne_lon;
     double          extent_se_lat;
     double          extent_se_lon;
 }_OSENC_EXTENT_Record_Payload;
 
typedef OsencRecord<_OSENC_EXTENT_Record_Payload,CELL_EXTENT_RECORD> OSENC_RecordExtent;

typedef struct {
    uint16_t        serverStatus;
    uint16_t        decryptStatus;
    uint16_t        expireStatus;
    uint16_t        expireDaysRemaining;
    uint16_t        graceDaysAllowed;
    uint16_t        graceDaysRemaining;
}_OSENC_SERVERSTAT_Record_Payload;

typedef OsencRecord<_OSENC_SERVERSTAT_Record_Payload,SERVER_STATUS_RECORD> OSENC_RecordServerStatus;
    

 #pragma pack(pop)

#endif