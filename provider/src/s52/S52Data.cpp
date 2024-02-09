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
#include "S52Data.h"
#include "S52Symbols.h"
#include "S52CondRules.h"
#include "S52Rules.h"
#include "Exception.h"
#include "FileHelper.h"
#include "Logger.h"
#include "CsvReader.h"
#include <cmath>
#include <functional>
#include "fpng.h"
#include "PngHandler.h"
#include "generated/S57ObjectClasses.h"
#include "generated/S57AttributeIds.h"

namespace s52
{
    std::map<ColorScheme, String> colorSchemes = {
        {DAY_BRIGHT, "DAY_BRIGHT"},
        {DAY_BLACK, "DAY_BLACKBACK"},
        {DAY_WHITE, "DAY_WHITEBACK"},
        {DUSK, "DUSK"},
        {NIGHT, "NIGHT"}};
    static String getColorTableName(RenderSettings::ConstPtr settings)
    {
        auto it = colorSchemes.find(settings->ColorScheme);
        if (it == colorSchemes.end())
            it = colorSchemes.begin();
        return it->second;
    }
    bool Attribute::operator==(const Attribute &other) const
    {
        if (type != other.type)
            return false;
        switch (type)
        {
        case T_INT:
            return iv == other.iv;
        case T_DOUBLE:
            return dv == other.dv;
        case T_STRING:
            return sv == other.sv;
        }
        return false;
    }
    bool Attribute::equals(const String &lupv) const
    {
        // see s52plib::FindBestLUP
        if (lupv.empty() || lupv == " ")
            return true;
        if (lupv == "?")
            return false;
        switch (this->type)
        {
        case T_INT:
            return ::atoi(lupv.c_str()) == iv;
            break;
        case T_DOUBLE:
        {
            double cmp = ::atof(lupv.c_str());
            if (::fabs(cmp - dv) < 1e-6)
                return true;
        }
        break;
        case T_STRING:
            return strcmp(lupv.c_str(), sv.c_str()) == 0;
            break;
        default:
            return false;
        }
        return false;
    }

    bool Attribute::Map::hasAttr(uint16_t id, Attribute::Type type) const
    {
        auto it = find(id);
        if (it == end())
            return false;
        if (type == Attribute::T_ANY)
            return true;
        return it->second.type == type;
    }
    double Attribute::Map::getDouble(uint16_t id) const
    {
        auto it = find(id);
        if (it == end() || it->second.type != T_DOUBLE)
        {
            throw AvException(FMT("Double attribute %d not found", id));
        }
        return it->second.dv;
    }
    int Attribute::Map::getInt(uint16_t id) const
    {
        auto it = find(id);
        if (it == end() || it->second.type != T_INT)
        {
            throw AvException(FMT("Int attribute %d not found", id));
        }
        return it->second.iv;
    }
    String Attribute::Map::getString(uint16_t id, bool convert) const
    {
        auto it = find(id);
        if (it == end())
        {
            throw AvException(FMT("String attribute %d not found", id));
        }
        return it->second.getString(convert);
    }
    StringVector Attribute::Map::getList(uint16_t id) const
    {
        String av;
        if (!getString(id, av))
            return StringVector();
        return StringHelper::split(av, ",");
    }
    String Attribute::Map::getParsedList(uint16_t id) const
    {
        String rt;
        StringVector parts = getList(id);
        if (!parts.size())
            return rt;
        for (auto it = parts.begin(); it != parts.end(); it++)
        {
            int v = ::atoi(it->c_str());
            if (v)
                rt.push_back(v);
        }
        return rt;
    }
    bool Attribute::Map::listContains(uint16_t id, const StringVector &values) const
    {
        String av;
        if (!getString(id, av))
            return false;
        StringVector parts = StringHelper::split(av, ",");
        for (auto it = values.begin(); it != values.end(); it++)
        {
            for (auto pit = parts.begin(); pit != parts.end(); pit++)
            {
                if (*pit == *it)
                    return true;
            }
        }
        return false;
    }

    int LUPrec::attributeMatch(const Attribute::Map *objectAttributes) const
    {
        if (objectAttributes->size() < 1)
        {
            // if there are no object attributes
            // any LUP matches
            return 0;
        }
        if (ATTCArray.size() < 1)
        {
            // no attributes in the rule
            // return false here
            // rule still will be found by 2nd try
            return 0;
        }
        int rt = 0;
        for (auto it = ATTCArray.begin(); it != ATTCArray.end(); it++)
        {
            auto oit = objectAttributes->find(it->first);
            if (oit == objectAttributes->end())
            {
                if (it->second == "?")
                {
                    //? means the object should not have this attribute
                    rt++;
                    continue;
                }
                return 0;
            }
            if (!oit->second.equals(it->second))
                return 0;
            rt++;
        }
        return rt;
    }
    S52Data::S52Data(RenderSettings::ConstPtr settings, FontFileHolder::Ptr f, int s) : sequence(s), pool(ocalloc::makePool("s52Data")), pref(pool), renderSettings(settings), fontFile(f)
    {
        ruleCreator = new RuleCreator(pref, 0);
    }
    S52Data::~S52Data()
    {
        delete ruleCreator;
    }
#define FONT_BASE 16.0;
    void S52Data::init(const String &dataDir)
    {
        LOG_DEBUG("starting S52Data::init in %s", dataDir);
        timer.reset();
        RenderSettings::ConstPtr settings = getSettings();
        int sndgFontSize = settings->soundingsFont * FONT_BASE;
        int txtFontSize = settings->chartFont * FONT_BASE;
        FontManager::Ptr fontManager = std::make_shared<FontManager>(fontFile, sndgFontSize);
        fontManager->init();
        fontManagers[sndgFontSize] = fontManager;
        if (txtFontSize != sndgFontSize)
        {
            fontManager = std::make_shared<FontManager>(fontFile, txtFontSize);
            fontManager->init();
            fontManagers[txtFontSize] = fontManager;
        }
        symbolCache = std::make_shared<SymbolCache>(
            settings->symbolScaleTolerance,
            settings->rotationTolerance);
        AddItem("symbolCache", symbolCache);
        timer.add("init font");
        S52Symbols symbolParser(this);
        Timer::SteadyTimePoint startSymbols = timer.current();
        symbolParser.parseSymbols(FileHelper::concatPath(dataDir, "chartsymbols.xml"));
        StringVector patchFiles = FileHelper::listDir(dataDir, "*Patch*.xml", true);
        std::sort(patchFiles.begin(), patchFiles.end());
        for (auto patch = patchFiles.begin(); patch != patchFiles.end(); patch++)
        {
            symbolParser.parseSymbols(*patch);
        }
        timer.set(startSymbols, "parseAllXML");
        Timer::SteadyTimePoint startRaster = timer.current();
        createRasterSymbols(dataDir);
        timer.set(startRaster, "createRasterSymbols");
        buildRules();
        timer.add("buildRules");
        initialized = true;
        LOG_DEBUG("finishing S52Data::init %s", timer.toString());
    }
    FontManager::Ptr S52Data::getFontManager(FontType type, int rsz) const
    {
        int size = 16;
        if (type == FONT_TXT)
        {
            size = getSettings()->chartFont * FONT_BASE;
        }
        else
        {
            size = getSettings()->soundingsFont * FONT_BASE;
        }
        auto it = fontManagers.find(size);
        if (it == fontManagers.end())
            return fontManagers.begin()->second;
        return it->second;
    }
    bool S52Data::hasAttribute(int code) const
    {
        return S57AttrIds::getDescription(code) != nullptr;
    }
    uint16_t S52Data::getAttributeCode(const String &name) const
    {
        uint16_t rt = S57AttrIds::getId(name);
        if (rt == 0)
            throw AvException(FMT("attribute name %s not found in map", name));
        return rt;
    }
    const LUPrec *S52Data::findLUPrecord(const LUPname &name, uint16_t featureTypeCode, const Attribute::Map *attributes) const
    {
        AVASSERT((froozenRules), "rules not froozen");
        auto it = lupTables.find(name);
        if (it == lupTables.end())
            return NULL;
        if (it->second->size() == 0)
            return NULL;
        auto records=it->second->find(featureTypeCode);
        if (records == it->second->end()){
            return NULL;
        }
        const LUPrec *noAttrMatch = nullptr;
        const LUPrec *bestMatch = nullptr;
        int score = 0;
        for (auto &lui : *(records->second))
        {
            // 1st try: exactly matching attributes
            // 2nd try: LUP with no attributes

            if (lui.ATTCArray.size() == 0 && noAttrMatch == nullptr)
            {
                noAttrMatch = &lui;
                continue;
            }
            int nScore = lui.attributeMatch(attributes);
            if (nScore > score)
            {
                score = nScore;
                bestMatch = &lui;
            }
        }
        if (bestMatch)
        {
            return bestMatch;
        }
        return noAttrMatch;
    }
    void S52Data::addColorTable(const ColorTable &table)
    {
        auto it=colorTables.find(table.tableName);
        if (it != colorTables.end()){
            LOG_INFO("updating color table %s",table.tableName);
            if (!table.rasterFileName.empty()){
                it->second.rasterFileName=table.rasterFileName;
            }
            for (const auto & [name,color]:table.colors){
                it->second.colors[name]=color;
            }
        }
        else{
            colorTables[table.tableName] = table;
        }
    }
    void S52Data::addLUP(const LUPrec &lup)
    {
        AVASSERT((!froozenRules), "rules froozen");
        uint16_t typeId = S57ObjectClassesBase::getId(lup.OBCL);
        if (typeId == 0)
        {
            LOG_ERROR("unknown OBCL %s in LUP, RCID=%d", lup.OBCL, lup.RCID);
            return;
        }
        LUPMap::Ptr map;
        auto it = lupTables.find(lup.TNAM);
        if (it == lupTables.end())
        {
            map = std::make_shared<LUPMap>();
            lupTables[lup.TNAM] = map;
        }
        else
        {
            map=it->second;
        }
        auto mit=map->find(typeId);
        if (mit == map->end()){
            LUPRecords::Ptr newList=std::make_shared<LUPRecords>();
            newList->push_back(lup);
            map->insert({typeId,newList});
        }
        else{
            avnav::erase_if(*(mit->second), [lup](const LUPrec &item)
                            { return item.RCID == lup.RCID; });
            mit->second->push_back(lup);
        }
        
    }
    MD5Name S52Data::getMD5() const
    {
        return renderSettings->GetMD5();
    }

    int S52Data::getSequence() const
    {
        return sequence;
    }

    void S52Data::createRulesForLup(LUPrec &lup)
    {
        AVASSERT((!froozenRules), "rules froozen");
        ruleCreator->rulesFromString(
            &lup, lup.INST, this, [&lup](const s52::Rule *r)
            { lup.ruleList.push_back(r); },
            true);
    }

    void S52Data::buildRules()
    {
        LOG_DEBUG("start building rules");
        AVASSERT((!froozenRules), "rules froozen");
        for (auto &[n,lupMap]: lupTables)
        {
            for (auto &[k,lupRecords] : *lupMap)
            {
                for (auto &lup:*lupRecords){
                    createRulesForLup(lup);
                }
            }
        }
        LOG_DEBUG("finish building rules");
        froozenRules = true;
    }
    RGBColor S52Data::getColor(const String &color) const
    {
        String tableName = getColorTableName(renderSettings);
        auto table = colorTables.find(tableName);
        if (table == colorTables.end())
        {
            table = colorTables.begin();
        }
        auto cit = table->second.colors.find(color);
        if (cit == table->second.colors.end())
        {
            return RGBColor({0, 0, 0});
        }
        return cit->second;
    }

    void S52Data::addSymbol(const String &name, const SymbolPosition &symbol)
    {
        AVASSERT((!froozenSymbols), "symbols froozen");
        symbolMap[name] = symbol;
    }
    void S52Data::addVectorSymbol(const String &name, const VectorSymbol &symbol)
    {
        AVASSERT((!froozenSymbols), "symbols froozen");
        symbolCache->fillVectorSymbol(name, symbol, getSettings()->symbolScale);
    }

    void S52Data::createRasterSymbols(const String &dataDir)
    {
        LOG_DEBUG("starting createRasterSymbols");
        AVASSERT((!froozenSymbols), "symbols frozen");
        String colorTableName = getColorTableName(getSettings());
        auto it = colorTables.find(colorTableName);
        if (it == colorTables.end())
            it = colorTables.begin();
        String rasterName = it->second.rasterFileName;
        if (rasterName.empty())
        {
            throw AvException(FMT("no raster file in color table %s", it->second.tableName));
            return;
        }
        rasterName = FileHelper::concatPath(dataDir, rasterName);
        if (!FileHelper::canRead(rasterName))
        {
            throw FileException(rasterName, "cannot read raster file");
        }
        std::unique_ptr<PngReader> reader(PngReader::createReader("spng"));
        if (!reader)
        {
            throw FileException(rasterName, "unable to create png reader");
        }
        if (!reader->read(rasterName))
        {
            throw FileException(rasterName, "unable to read png");
        }
        timer.add("readPng");
        LOG_DEBUG("building rasterSymbols");
        for (auto it = symbolMap.begin(); it != symbolMap.end(); it++)
        {
            if (!symbolCache->fillRasterSymbol(it->first,
                                               it->second,
                                               reader.get(), getSettings()->symbolScale))
            {
                LOG_DEBUG("graphics location for symbol %s out of png range", it->first);
            }
        }
        froozenSymbols = true;
        LOG_DEBUG("finishing createRasterSymbols");
    }
    const SymbolPtr S52Data::getSymbol(const String &name, int rotation, double scale) const
    {
        AVASSERT(froozenSymbols, "symbols not frozen");
        SymbolCache::GetColorFunction getColorFunction = [this](const String &name)
        {
            RGBColor c = this->getColor(name);
            return DrawingContext::convertColor(c.R, c.G, c.B);
        };
        SymbolPtr rt = symbolCache->getSymbol(name, getColorFunction, rotation, scale);
        if (rt)
            return rt;
        rt = symbolCache->getSymbol("QUESMRK1", getColorFunction, rotation, scale);
        return rt;
    }
    String S52Data::checkSymbol(const String &name) const
    {
        AVASSERT(froozenSymbols, "symbols not frozen");
        String rt = symbolCache->checkSymbol(name);
        if (!rt.empty())
            return rt;
        return symbolCache->checkSymbol("QUESMRK1");
    }

    bool S52Data::LocalJson(StatusStream &stream)
    {
        if (froozenSymbols)
        {
            stream["numRasterSymbols"] = symbolMap.size();
        }
        return true;
    }
    double S52Data::convertSounding(double valMeters, uint16_t attrid) const
    {
        if (attrid == 0 || attrid == S57AttrIds::VERCLR || attrid == S57AttrIds::VERCCL || attrid == S57AttrIds::VERCOP || attrid == S57AttrIds::HEIGHT || attrid == S57AttrIds::HORCLR
            // eSENCChart::GetAttributeValueAsString
        )
        {
            static const double fm = 3.2808399;
            switch (renderSettings->S52_DEPTH_UNIT_SHOW)
            {
            case s52::DepthUnit::FATHOMS:
                return valMeters * fm / 6.0;
            case s52::DepthUnit::FEET:
                return valMeters * fm;
            }
        }
        return valMeters;
    }
}