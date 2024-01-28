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

#ifndef _S52TYPES_H
#define _S52TYPES_H
namespace s52{
    class Rule;
    class RuleCreator;
    class RuleList: public std::vector<const Rule*>{
        public:
        using std::vector<const Rule*>::vector;
    };
    class RGBColor{
        public:
        uint8_t R=0;
        uint8_t G=0;
        uint8_t B=0;
    };
    typedef enum
    {
        POINT_T = 'P',
        LINES_T = 'L',
        AREAS_T = 'A',
        OBJ_NUM = 3 // number of object type
    } Object_t;
    typedef enum
    {
        PRIO_NODATA = 0,     // no data fill area pattern
        PRIO_GROUP1 = 1,     // S57 group 1 filled areas
        PRIO_AREA_1 = 2,     // superimposed areas
        PRIO_AREA_2 = 3,     // superimposed areas also water features
        PRIO_SYMB_POINT = 4, // point symbol also land features
        PRIO_SYMB_LINE = 5,  // line symbol also restricted areas
        PRIO_SYMB_AREA = 6,  // area symbol also traffic areas
        PRIO_ROUTEING = 7,   // routeing lines
        PRIO_HAZARDS = 8,    // hazards
        PRIO_MARINERS = 9,   // VRM, EBL, own ship
        PRIO_NUM = 10,        // number of priority levels
        PRIO_NONE = 9999

    } DisPrio;
    // RADAR Priority
    typedef enum
    {
        RAD_OVER = 'O', // presentation on top of RADAR
        RAD_SUPP = 'S', // presentation suppressed by RADAR
        RAD_NUM = 2
    } RadPrio;
    // name of the addressed look up table set (fifth letter)
    typedef enum 
    {
        SIMPLIFIED = 'L',            // points
        PAPER_CHART = 'R',           // points
        LINES = 'S',                 // lines
        PLAIN_BOUNDARIES = 'N',      // areas
        SYMBOLIZED_BOUNDARIES = 'O' // areas
    } LUPname;
    // display category type
    typedef enum
    {
        DISPLAYBASE = 'D',       //
        STANDARD = 'S',          //
        OTHER = 'O',             // O for OTHER
        MARINERS_STANDARD = 'M', // Mariner specified
        UNDEFINED = 'U'
    } DisCat;

    typedef enum
    {
        GEO_UNSET,
        GEO_POINT,
        GEO_LINE,
        GEO_AREA,
        GEO_META,
        GEO_PRIM, // number of primitive
    } GeoPrimitive;

    typedef enum{
        OFF=0,
        ON=1
    } OnOff;

    typedef enum{
        METERS=0,
        FEET=1,
        FATHOMS=2
    } DepthUnit;

    typedef enum{
        DAY_BRIGHT=0,
        DAY_BLACK=1,
        DAY_WHITE=2,
        DUSK=3,
        NIGHT=4
    }ColorScheme;
}
#endif