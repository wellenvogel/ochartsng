/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  S52 data
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
// parts from http://www.zedwood.com/article/cpp-csv-parser
#ifndef _CSVREADER_H
#define _CSVREADER_H
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <istream>
#include <map>
#include "Types.h"
#include "Exception.h"
#include "StringHelper.h"

class CsvReader
{
    std::ifstream ifs;
    char delimiter;
    String fileName;
    std::map<String, int> columnIdx;
    StringVector row;
    bool hasHeader = 0;
    int rowNum=0;

public:
    CsvReader(const String &fileName, char delimiter=',')
    {
        this->delimiter = delimiter;
        this->fileName = fileName;
    }
    bool open(bool doThrow=true){
        ifs.open(fileName,std::ifstream::in);
        if (!ifs.is_open()){
            if (doThrow) throw FileException(fileName,"CSV: unable to open");
            return false;
        }
        return true;
    }
    int getIdx(const String &colName, bool doThrow=true){
        if (!hasHeader) throw FileException(fileName,"CSV: no header read");
        auto it=columnIdx.find(colName);
        if (it == columnIdx.end()){
            if (doThrow) throw FileException(fileName,FMT("column %s not found",colName));
            return -1;
        }
        return it->second;
    }
    String getCol(int idx, bool doThrow=true){
        if (idx >= row.size() || idx < 0){
            if (doThrow) throw FileException(fileName,FMT("column %d not found in row %d",idx,rowNum));
            return String();
        }
        return row[idx];
    }
    String getCol(const String &name, bool doThrow=true){
        int idx=getIdx(name,doThrow);
        return getCol(idx);
    }
    bool fail()
    {
        return ifs.fail();
    }
    bool good()
    {
        return ifs.good();
    }
    bool readHeader(const StringVector &columns)
    {
        if (hasHeader)
            return false;
        if (! nextRow()){
            throw FileException(fileName,"CSV: no header row");
        }
        hasHeader = true;
        for (int idx = 0; idx < row.size(); idx++)
        {
            columnIdx[row[idx]] = idx;
        }
        for (auto it=columns.begin();it != columns.end();it++){
            if (columnIdx.find(*it) == columnIdx.end()){
                throw FileException(fileName,FMT("missing column %s in header",*it));
            }
        }
        return true;
    }
    bool nextRow()
    {
        row.clear();
        std::stringstream ss;
        bool inquotes = false;
        while (! ifs.eof())
        {
            char c = ifs.get();
            if (c < 0) break;
            if (!inquotes && c == '"') // beginquotechar
            {
                inquotes = true;
            }
            else if (inquotes && c == '"') // quotechar
            {
                if (ifs.peek() == '"') // 2 consecutive quotes resolve to 1
                {
                    ss << (char)ifs.get();
                }
                else // endquotechar
                {
                    inquotes = false;
                }
            }
            else if (!inquotes && c == delimiter) // end of field
            {
                row.push_back(ss.str());
                ss.str("");
            }
            else if (!inquotes && (c == '\r' || c == '\n'))
            {
                if (ifs.peek() == '\n')
                {
                    ifs.get();
                }
                row.push_back(ss.str());
                rowNum++;
                return true;
            }
            else
            {
                ss << c;
            }
        }
        return false;
    }
};
#endif