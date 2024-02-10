/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  S57Object
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

#include "S57Object.h"
#include "S52CondRules.h"
#include "S52TextParser.h"
#include "generated/S57AttributeIds.h"
#include "generated/S57ObjectClasses.h"
#include "RenderHelper.h"
#include "ObjectDescription.h"
S57BaseObject::S57BaseObject(ocalloc::PoolRef p,uint16_t id,uint16_t code,uint8_t prim)
    :featureId(id),
    featureTypeCode(code),
    featurePrimitive(prim),
    pool(p),
    attributes(p){
    }
S57Object::S57Object(ocalloc::PoolRef p,uint16_t id,uint16_t code,uint8_t prim):
    S57BaseObject(p,id,code,prim),soundigs(p),
    area(p),lines(p),polygons(p){}

static std::u32string SOUNDINGS_MAX=StringHelper::toUnicode(String("000000"));

//"OUTLW, 4,LITYW, 2,333.0, 190.0,  20.0,  25.0"
static s52::Arc parseArc(const s52::S52Data *s52data, const String& str){
    s52::Arc rt;
    StringVector parts=StringHelper::split(str,",");
    if (parts.size() != 8){
        LOG_DEBUG("invalid ARC %s",str);
        return rt;
    }
    rt.cOutline=s52data->convertColor(s52data->getColor(parts[0]));
    rt.cArc=s52data->convertColor(s52data->getColor(parts[2]));
    rt.outlineWidth=::atoi(parts[1].c_str());
    rt.arcWidth=::atoi(parts[3].c_str());
    rt.sectr1=::atof(parts[4].c_str());
    rt.sectr2=::atof(parts[5].c_str());
    rt.arcRadius=::atoi(parts[6].c_str());
    rt.sectorRadius=::atoi(parts[7].c_str());
    rt.valid=true;
    int width=(std::max(rt.arcRadius,rt.sectorRadius)+rt.outlineWidth)*2+10; //??
    int height=width;
    rt.relativeExtent.xmin=-width/2;
    rt.relativeExtent.xmax=width/2;
    rt.relativeExtent.ymin=-height/2;
    rt.relativeExtent.ymax=height/2;
    rt.relativeExtent.valid=true;
    return rt;
}
void S57Object::RenderObject::expandRule(const s52::S52Data *s52data,const s52::Rule *rule)
{
    try{
    if (rule->type == s52::RUL_TXT_TE){
        const s52::StringTERule *sr=rule->cast<s52::StringTERule>();
        s52::DisplayString str=s52::S52TextParser::parseTE(object->pool, s52data,rule->parameter.c_str(),sr->options,&(object->attributes));
        if (str.valid){
            expandedTexts.set(rule->key,str);
            pixelExtent.extend(str.relativeExtent);
        }
    }
    if (rule->type == s52::RUL_TXT_TX){
        const s52::StringTXRule *sr=rule->cast<s52::StringTXRule>();
        s52::DisplayString str=s52::S52TextParser::parseTX(object->pool,s52data,rule->parameter.c_str(),sr->options,&(object->attributes));
        if (str.valid){
            expandedTexts.set(rule->key,str);
            pixelExtent.extend(str.relativeExtent);
        }
    }
    }catch (AvException &e){
        LOG_DEBUG("exception while expanding rule %d(%s,RCID=%d): %s",rule->type,rule->parameter,lup->RCID, e.what());
    }
    if (rule->type == s52::RUL_SYM_PT)
    {
        const s52::SymbolRule *sr = rule->cast<s52::SymbolRule>();
        if (!sr->finalName.empty())
        {
            double orient=0;
            if (object->attributes.getDouble(S57AttrIds::ORIENT,orient)){
                //debug
                int x=orient;
            }
            s52::SymbolPtr symbol=s52data->getSymbol(sr->finalName,orient);
            if (symbol){
                pixelExtent.extend(symbol->relativeExtent);
            }
        }
    }
    if (rule->type == s52::RUL_SIN_SG){
        FontManager::Ptr fm=s52data->getFontManager(s52::S52Data::FONT_SOUND);
        int width=fm->getTextWidth(SOUNDINGS_MAX);
        int height=fm->getLineHeight();
        Coord::PixelBox ext;
        ext.xmin=-width/2;
        ext.xmax=width/2;
        ext.ymin=-height;
        ext.ymax=height;
        ext.valid=true;
        pixelExtent.extend(ext);
    }
    if (rule->type == s52::RUL_MUL_SG){
        //compute a pixel margin
        //simply take the lineheight
        //and the width of 5 digits
        //for now one font only...
        FontManager::Ptr fm=s52data->getFontManager(s52::S52Data::FONT_SOUND);
        xmargin=fm->getTextWidth(SOUNDINGS_MAX);
        ymargin=fm->getLineHeight();
    }
    if (rule->type == s52::RUL_ARC_2C){
        s52::Arc arc=parseArc(s52data,rule->parameter.c_str());
        if (arc.valid){
            pixelExtent.extend(arc.relativeExtent);
            arcs[rule->key]=arc;
        }
    }
    if (rule->type == s52::RUL_XC_CAT){
        displayCategory=s52::DisCat::DISPLAYBASE;
    }
}

void S57Object::RenderObject::expand(const s52::S52Data *s52data, s52::RuleCreator *creator,const s52::RuleConditions *conditions){
    condRules.clear();
    pixelExtent.valid=false;
    if (!lup)
        return;
    for (auto rit = lup->ruleList.begin(); rit != lup->ruleList.end(); rit++)
    {        
        if ((*rit) ->type == s52::RUL_CND_SY){
            ARuleList rules(pool);
            creator->rulesFromString(lup,(*rit)->parameter.c_str(),s52data,
                [&rules](const s52::Rule *r){rules.push_back(r);},
                true,conditions);
            for (auto crit=rules.begin();crit != rules.end();crit++){
                expandRule(s52data,*crit);
            }
            condRules.set((*rit)->key,rules);
        }
        else{
            expandRule(s52data,*rit);
        }
        
    }
}

void S57Object::RenderObject::RenderSingleRule(RenderContext &renderCtx, DrawingContext &ctx,  Coord::TileBox const &tile, const s52::Rule *rule) const
{
    auto renderSettings=renderCtx.s52Data->getSettings().get();
    if (rule->type == s52::RUL_CND_SY)
    {

    }
    if (rule->type == s52::RUL_ARE_CO || rule->type == s52::RUL_ARE_PA)
    {
        DrawingContext::ColorAndAlpha c=0;
        std::unique_ptr<DrawingContext::PatternSpec> pattern;
        if (rule->type == s52::RUL_ARE_CO){
            const s52::AreaRule *ar=rule->cast<s52::AreaRule>();
            c = DrawingContext::convertColor(ar->color.R, ar->color.G, ar->color.B);
        }
        else{
            const s52::SymAreaRule *ar=rule->cast<s52::SymAreaRule>();
            if (ar->symbol){
                pattern.reset(RenderHelper::createPatternSpec(ar->symbol,tile));
            }
            else{
                return;
            }
        }
        // AREA
        for (auto vlit = object->area.begin(); vlit != object->area.end(); vlit++)
        {
            auto vit=*vlit;
            /*
            if (!extent.intersects(vit->extent))
                continue;
                */
            uint8_t tc = vit->type;
            Coord::WorldXy pp3[3]; // triangle corners
            int inc = 1;
            int max = vit->numAreaPoints;
            switch (tc)
            {
            case 6: // PTG_TRIANGLE_FAN
                max = vit->numAreaPoints - 2;
                break;
            case 5: // PTG_TRIANGLE_STRIP
                max = vit->numAreaPoints - 2;
                break;
            case 4: // PTG_TRIANGLES
                inc = 3;
                break;
            default:
                max = 0; // ignore
                break;
            }
            for (int it = 0; it < max; it += inc)
            {
                switch (tc)
                {
                case 6: // PTG_TRIANGLE_FAN
                    pp3[0].x = vit->points[0].x;
                    pp3[0].y = vit->points[0].y;
                    pp3[1].x = vit->points[it + 1].x;
                    pp3[1].y = vit->points[it + 1].y;
                    pp3[2].x = vit->points[it + 2].x;
                    pp3[2].y = vit->points[it + 2].y;
                    break;
                case 5: // PTG_TRIANGLE_STRIP
                    pp3[0].x = vit->points[it].x;
                    pp3[0].y = vit->points[it].y;
                    pp3[1].x = vit->points[it + 1].x;
                    pp3[1].y = vit->points[it + 1].y;
                    pp3[2].x = vit->points[it + 2].x;
                    pp3[2].y = vit->points[it + 2].y;
                    break;
                case 4: // PTG_TRIANGLES
                    pp3[0].x = vit->points[it].x;
                    pp3[0].y = vit->points[it].y;
                    pp3[1].x = vit->points[it + 1].x;
                    pp3[1].y = vit->points[it + 1].y;
                    pp3[2].x = vit->points[it + 2].x;
                    pp3[2].y = vit->points[it + 2].y;
                    break;
                }
                ctx.drawTriangle(
                    tile.worldToPixel(pp3[0]),
                    tile.worldToPixel(pp3[1]),
                    tile.worldToPixel(pp3[2]),
                    c,
                    pattern.get());
            }
        }
        return;
    }
    if (rule->type == s52::RUL_SYM_PT)
    {
        const s52::SymbolRule *sr = rule->cast<s52::SymbolRule>();
        if (!sr->finalName.empty())
        {
            double orient=0;
            object->attributes.getDouble(S57AttrIds::ORIENT,orient);
            s52::SymbolPtr symbol = renderCtx.s52Data->getSymbol(sr->finalName,orient);
            if (symbol && symbol->buffer)
            {
                Coord::PixelXy pp = tile.worldToPixel(object->point);
                pp.x -= symbol->pivot_x;
                pp.y -= symbol->pivot_y;
                //avoid drawing a symbol if it would be outside the area box if this
                //is an area object
                if (object->geoPrimitive == s52::GEO_AREA){
                    Coord::PixelBox symbolExt=symbol->relativeExtent.getShifted(pp);
                    Coord::PixelBox areaExtend=Coord::worldExtentToPixel(object->extent,tile);
                    if (!areaExtend.includes(symbolExt)) {
                        return;    
                    }
                }
                ctx.drawSymbol(pp, symbol->width, symbol->height, symbol->buffer->data());
            }
        }
        return;
    }
    if (rule->type == s52::RUL_TXT_TE || rule->type == s52::RUL_TXT_TX){
        auto it=expandedTexts.find(rule->key);
        if (it == expandedTexts.end() || ! it->second.valid){
            return;
        }
        Coord::PixelXy pp=tile.worldToPixel(object->point);
        Coord::PixelBox ourExtent=it->second.relativeExtent.getShifted(pp);
        if (renderSettings->bDeClutterText){
            //declutter
            for (auto di=renderCtx.textBoxes.begin();di != renderCtx.textBoxes.end();di++){
                if (ourExtent.intersects(*di)){
                    //would overlap, TODO: settings
                    return;
                }
            }
        }
        //text render settings
        //see s52plib::TextRenderCheck
        if (! renderSettings->bShowS57Text){
            return;
        }
        if (object->isAton){
            if (object->featureTypeCode == S57ObjectClasses::LIGHTS){
                if (! renderSettings->bShowLightDescription) return;
            }
            else{
                if (! renderSettings->bShowAtonText) return;
            }
        }

        int group = 0;
        int fontSize=16;
        if (rule->type == s52::RUL_TXT_TE)
        {
            group = rule->cast<s52::StringTERule>()->options.grp;
            fontSize=rule->cast<s52::StringTERule>()->options.fontSize;
        }
        else
        {
            group = rule->cast<s52::StringTXRule>()->options.grp;
            fontSize=rule->cast<s52::StringTXRule>()->options.fontSize;
        }
        if (renderSettings->bShowS57ImportantTextOnly)
        {
            if (group < 20) return;
        }
        FontManager::Ptr fm=renderCtx.s52Data->getFontManager(s52::S52Data::FONT_TXT,fontSize);
        Coord::PixelBox te=RenderHelper::drawText(fm,ctx,it->second,pp);
        renderCtx.textBoxes.push_back(te);
        return;
    }
    if (rule->type == s52::RUL_MUL_SG){
        RenderHelper::renderSounding(renderCtx.s52Data,ctx,tile, &(object->soundigs));
        return;
    }
    if (rule->type == s52::RUL_SIN_SG){
        S57Object::Sounding sndg(object->point);
        sndg.depth=::atof(rule->parameter.c_str());
        std::vector<S57Object::Sounding> lst;
        lst.push_back(sndg);
        RenderHelper::renderSounding(renderCtx.s52Data,ctx,tile,&lst);
        return;
    }
    if (rule->type == s52::RUL_ARC_2C){
        auto it=arcs.find(rule->key);
        if (it != arcs.end()){
            RenderHelper::renderArc(renderCtx.s52Data,ctx,it->second,tile.worldToPixel(object->point));
        }
    }
    if (rule->type == s52::RUL_SIM_LN){
        StringVector parts=StringHelper::split(rule->parameter.str(),",");
        if (parts.size() < 3){
            return;
        }
        DrawingContext::ColorAndAlpha color = renderCtx.s52Data->convertColor(
            renderCtx.s52Data->getColor(parts[2]));
        int width=atoi(parts[1].c_str()); 
        if (width < 1) width=1;
        DrawingContext::Dash dash;
        DrawingContext::Dash *dashPtr=nullptr;
        if (parts[0] == "DOTT"){
            dashPtr=&dash;
            dash.draw=width;
            dash.gap=width;
        }
        if (parts[0] == "DASH"){
            dashPtr=&dash;
            dash.draw=3*width;
            dash.gap=width;
        }
        
        for (const auto &it:object->polygons){
            it.iterateSegments([&ctx,&tile,color,width,&dashPtr](Coord::WorldXy start, Coord::WorldXy end, bool isFirst){
                RenderHelper::renderLine(ctx,tile,color,start,end,width,dashPtr);  
            });
        } 
        return;
        
    }
    if (rule->type == s52::RUL_COM_LN){
        //return;
        const s52::SymbolLineRule *srule=rule->cast<s52::SymbolLineRule>();
        for (const auto &it:object->polygons){
            if (it.winding == S57Object::Polygon::WD_UN){
                int debug=1;
            }
            it.iterateSegments([&renderCtx,&ctx,&tile,&srule](Coord::WorldXy start, Coord::WorldXy end, bool isFirst){
                RenderHelper::renderSymbolLine(renderCtx.s52Data, ctx, tile, start, end, srule->symbol);
            },it.winding == S57Object::Polygon::WD_CW);            
        } 
        
    }
}
void S57Object::RenderObject::Render(RenderContext &renderCtx, DrawingContext &ctx, Coord::TileBox const &tile,const s52::RenderStep &step) const
{
    if (!lup)
        return;
    if (! shouldRenderScale(renderCtx.s52Data->getSettings().get(),renderCtx.scale)) return;       
    if (step == s52::RS_AREAS1 || step == s52::RS_AREASY){
        ;
    }
    else{
        if (lup->step != step) return;
    }        
    for (auto &&rit : lup->ruleList)
    {        
        if (rit->type == s52::RUL_CND_SY){
            auto it=condRules.find(rit->key);
            if (it == condRules.end() ) {
                continue; //rule not expanded
            }
            for (auto &&erit:it->second){
                if (step == s52::RS_AREAS1 &&  erit->type != s52::RUL_ARE_CO){
                    continue;
                }
                if (step == s52::RS_AREASY &&  erit->type != s52::RUL_ARE_PA){
                    continue;
                }
                RenderSingleRule(renderCtx,ctx,tile,erit);
            }
        }
        else
        {
            if (step == s52::RS_AREAS1 && rit->type != s52::RUL_ARE_CO)
            {
                continue;
            }
            if (step == s52::RS_AREASY && rit->type != s52::RUL_ARE_PA)
            {
                continue;
            }
            RenderSingleRule(renderCtx, ctx, tile,rit);
        }
    }
}

bool S57Object::RenderObject::Intersects(const Coord::PixelBox &pixelBox,Coord::TileBox const &tile) const{
    switch (object->geoPrimitive){
        case s52::GEO_AREA:
        case s52::GEO_LINE:
            return object->extent.intersects(tile);
            break;
        case s52::GEO_POINT:
            if (object->soundigs.size() > 0){
                Coord::PixelBox etx=Coord::worldExtentToPixel(object->extent,tile);
                return pixelBox.intersects(etx);
            }
            else{
                Coord::PixelXy drawPoint=tile.worldToPixel(object->point);
                if (pixelExtent.Valid()){
                    return pixelBox.intersects(pixelExtent.getShifted(drawPoint));
                }
                return pixelBox.intersects(drawPoint);
            }
            break;
    }
    return false;
}
S57Object::RenderObject::RenderObject(ocalloc::PoolRef p,S57Object::ConstPtr o)
    :object(o),
    expandedTexts(p),
    arcs(p),
    condRules(p),
    pool(p){
};

Coord::WorldXy S57Object::LineIndex::firstPoint() const{
    if (startSegment.valid) return startSegment.start;
    if (edgeNodes && edgeNodes->size() > 0) {
        if (forward) return edgeNodes->at(0);
        return edgeNodes->at(edgeNodes->size()-1);
    }
    if (endSegment.valid) return endSegment.start;
    return Coord::WorldXy();
}
Coord::WorldXy S57Object::LineIndex::lastPoint() const{
    if (endSegment.valid) return endSegment.end;
    if (edgeNodes && edgeNodes->size() > 0){
        if (forward) return edgeNodes->at(edgeNodes->size()-1);
        return edgeNodes->at(0);
    }
    if (startSegment.valid) return startSegment.end;
    return Coord::WorldXy();
}

void S57Object::LineIndex::iterateSegments(S57Object::LineIndex::SegmentIterator it, bool backward) const
{
    /**
     * if we iterate backwards we start from the end point of the end segment
     * and end at the start point of the start segment
     * for the edge node we have to see if their original direction is forward
     * (i.e. forward == true) - then we invert their direction for backward
     * 
     * for normal direction we use the start and end segment as they are and the 
     * edge nodes depending on their original direction
     * if it was forward we leave it as forward
     * otherwise we revert
    */
    bool isFirst = true;
    const LineSegment *first = backward ? &endSegment : &startSegment;
    const LineSegment *last = backward ? &startSegment : &endSegment;
    SegmentIterator itf = backward ? [it](Coord::WorldXy start, Coord::WorldXy end, bool first)
    {
        it(end, start, first);
    }
                                    : it;
    if (first->valid)
    {
        itf(first->start, first->end, isFirst);
        isFirst = false;
    }
    bool iterateBackwards = (forward == backward);

    if (edgeNodes != nullptr && edgeNodes->size() > 0)
    {
        if (iterateBackwards)
        {
            Coord::WorldXy current = edgeNodes->at(edgeNodes->size() - 1);
            for (int i = edgeNodes->size() - 2; i >= 0; i--)
            {
                Coord::WorldXy next = edgeNodes->at(i);
                it(current, next, isFirst);
                isFirst = false;
                current = next;
            }
        }
        else
        {
            Coord::WorldXy current = edgeNodes->at(0);
            for (int i = 1; i < edgeNodes->size(); i++)
            {
                Coord::WorldXy next = edgeNodes->at(i);
                it(current, next, isFirst);
                isFirst = false;
                current = next;
            }
        }
    }
    if (last->valid)
    {
        itf(last->start, last->end, isFirst);
    }
}

void S57Object::Polygon::iterateSegments(LineIndex::SegmentIterator i,bool backwards) const{
    //TODO: decide on adding a missing last segement if not complete
    if (! obj) return;
    if (backwards){
        for (int idx=endIndex;idx>=startIndex;idx--){
            if (idx >= 0 && idx < obj->lines.size()){
                auto line=obj->lines[idx];
                line.iterateSegments(i,backwards);
            }
        }
    }
    else{
        for (int idx=startIndex;idx<=endIndex;idx++){
            if (idx >= 0 && idx < obj->lines.size()){
                auto line=obj->lines[idx];
                line.iterateSegments(i,backwards);
            }
        }
    }
}

s52::DisCat S57Object::RenderObject::getDisplayCat() const{
    if (displayCategory != s52::DisCat::UNDEFINED){
        return displayCategory;
    }
    if (! lup){
        return s52::DisCat::UNDEFINED;
    }
    return lup->DISC;
}

bool S57Object::RenderObject::shouldRenderCat(const RenderSettings *rs) const{
    //refer to s52plib::ObjectRenderCheckCat
    s52::DisCat cat=getDisplayCat();
    if (cat == s52::DisCat::UNDEFINED) return false;
    bool rt=true;
    //  Meta object filter.
    // Applied when showing display category OTHER, and
    // only for objects whose decoded S52 display category (by LUP) is also OTHER
    if( rs->nDisplayCategory == s52::DisCat::OTHER ){
        if(s52::DisCat::OTHER == cat){
            if (object->featureTypeCode == S57ObjectClasses::M_QUAL){
                if (!rs->bShowMeta) rt=false; //TODO: contradicts own showQuality setting
            }
        }
    }
    else{
    // We want to filter out M_NSYS objects everywhere except "OTHER" category
        if( StringHelper::startsWith(lup->OBCL, "M_") ){
            if( ! rs->bShowMeta )
                rt=false;
        }
    }
    if (rs->nDisplayCategory == s52::DisCat::MARINERS_STANDARD)
    {
        // should we only rely on the object itself (set from cond sym)
        // or should we alos consider the LUP?
        if (displayCategory != s52::DisCat::DISPLAYBASE)
        {
            rt = rs->getVisibility(object->featureTypeCode);
        }
    }
    else
    {
        if (rs->nDisplayCategory == s52::DisCat::OTHER)
        {
            if ((s52::DisCat::DISPLAYBASE != cat) && (s52::DisCat::STANDARD != cat) && (s52::DisCat::OTHER != cat))
            {
                rt = false;
            }
        }

        else if (rs->nDisplayCategory == s52::DisCat::STANDARD)
        {
            if ((s52::DisCat::DISPLAYBASE != cat) && (s52::DisCat::STANDARD != cat))
            {
                rt = false;
            }
        }
        else if (rs->nDisplayCategory == s52::DisCat::DISPLAYBASE)
        {
            if (s52::DisCat::DISPLAYBASE != cat)
            {
                rt = false;
            }
        }
    }
    if (object->featureTypeCode == S57ObjectClasses::M_QUAL){
        rt=rs->bShowQuality;
    }
    if (object->featureTypeCode == S57ObjectClasses::SOUNDG){
        rt=rs->bShowSoundg;
    }
    if (object->featureTypeCode == S57ObjectClasses::LIGHTS){
        rt=rs->showLights;
    }
    static const std::vector<uint16_t> ANCHOR_FEATURES({S57ObjectClasses::ACHBRT,S57ObjectClasses::ACHARE,
        S57ObjectClasses::CBLSUB,S57ObjectClasses::PIPSOL,S57ObjectClasses::TUNNEL, S57ObjectClasses::SBDARE});
    for (auto it=ANCHOR_FEATURES.begin();it != ANCHOR_FEATURES.end();it++){
        if (object->featureTypeCode == *it && ! rs->showAnchorInfo) return false;
    }
    return rt;

}

bool S57Object::RenderObject::shouldRenderScale(const RenderSettings *rs,int scale) const{
    if (! rs->bUseSCAMIN) return true;
    if( ( s52::DisCat::DISPLAYBASE == getDisplayCat()) || ( s52::DisPrio::PRIO_GROUP1 == lup->DPRI )) {
        return true;
    }
    if (object->scamin > 0 && scale > object->scamin) return false;
    //TODO: special handling for "$TEXTS" ???
    return true;
}
class S57ObjectDescription : public ObjectDescription
{
    bool hasPoint=false;
    NameValueMap addOns;
    json::JSON out;
    void buildOverview(const S57BaseObject* o);
    void buildFull(const S57BaseObject* o);
public:
    String expandedText;
    S57ObjectDescription(const S57BaseObject* o,bool overview, const NameValueMap &addons=NameValueMap());
    virtual ~S57ObjectDescription(){}
    virtual bool isPoint() const{ return hasPoint;};
    virtual double computeDistance(const Coord::WorldXy &wp);
    virtual void toJson(json::JSON &js) const;
    virtual void jsonOverview(json::JSON &js) const;
    virtual void addValue(const String &k, const String &v){
        out[k]=v;
    }
};
//we ignore a couple of attributes if when checking for equality
//to avoid getting the same object twice from different charts
std::unordered_set<uint16_t> IGNORED_ATTRIBUTES={S57AttrIds::SCAMIN,S57AttrIds::SORIND,S57AttrIds::SORDAT,S57AttrIds::SIGSEQ,S57AttrIds::catgeo};
S57ObjectDescription::S57ObjectDescription(const S57BaseObject* cobject,bool overview, const NameValueMap &ad){
        type=T_OBJECT;
        point=cobject->point;
        hasPoint=cobject->geoPrimitive==s52::GEO_POINT && !cobject->isMultipoint();
        //1. prefer points
        //2. prefer lights
        if (hasPoint) {
            score=cobject->featureTypeCode == S57ObjectClasses::LIGHTS?3:2;
        }
        else{
            score=1;
        }
        for (auto a : ad){
            addOns.insert(a);
        }
        MD5 builder;
        builder.AddValue(type);
        builder.AddValue(cobject->featureTypeCode);
        builder.AddValue(cobject->geoPrimitive);
        builder.AddValue(hasPoint);
        builder.AddValue(point.x);
        builder.AddValue(point.y);
        for (const auto & [id,attr] : cobject->attributes){
            if (IGNORED_ATTRIBUTES.count(id)>0){
                continue;
            }
            builder.AddValue(id);
            attr.addToMd5(builder);
        }
        //we do not add addOns to MD5 for equality checks
        //so they will not be considered...
        md5.fromChar(builder.GetValue());
        if (overview) buildOverview(cobject);
        else buildFull(cobject);
}

double S57ObjectDescription::computeDistance(const Coord::WorldXy &wpt){
    double od=(wpt.x-point.x);
    double ad=(wpt.y-point.y);
    distance=std::sqrt(od*od+ad*ad);
    return distance;
}

static String getConvertedAttrValue(const S57BaseObject *object, uint16_t attrid, bool includeVals=true){
    auto ait=object->attributes.find(attrid);
    if (ait == object->attributes.end()) return "NVAL";
    const S57AttrValueDescription *description=nullptr;
    switch (ait->second.getType())
    {
    case s52::Attribute::T_INT:
        description=S57AttrValuesBase::getDescription(attrid,ait->second.getInt());
        if (description) {
            return description->getString(includeVals);
        }
        break;
    case s52::Attribute::T_STRING:
    {
        StringVector parts = StringHelper::split(ait->second.getString(), ",");
        String rt;
        for (const auto &pv : parts)
        {
            description = nullptr;
            int iv = ::atoi(pv.c_str());
            if (iv != 0)
            {
                description = S57AttrValuesBase::getDescription(attrid, iv);
            }
            if (!rt.empty())
                rt.push_back(',');
            if (description)
            {
                rt.append(description->getString(includeVals));
            }
            else
            {
                rt.append(pv);
            }
        }
        return rt;
    }
    }
    return ait->second.getString(true);
}

//attribute mappings for the overview that we create in jsonOverview
class AttributeEntry{
public:
    using IdVector=std::vector<uint16_t>;
    using Mapper=std::function<String(const AttributeEntry *entry, const S57BaseObject *object)>;
    IdVector attributes;
    IdVector features;
    String target;
    Mapper mapper;
    AttributeEntry(const String &target, const IdVector &features,const IdVector &attributes, Mapper m):
        mapper(m){
        this->target=target;
        this->attributes=attributes;
        this->features=features;
    }
    bool MatchesFeature(uint16_t featureId) const{
        return features.size() == 0 || (std::find(features.begin(),features.end(),featureId) != features.end());
    }
    bool MatchesAttribute(uint16_t attributeId) const{
        return (std::find(attributes.begin(),attributes.end(),attributeId) != attributes.end());
    }
    bool MatchesFeatureAndAttribute(uint16_t featureId,uint16_t attributeId) const{
        return MatchesFeature(featureId) && MatchesAttribute(attributeId);
    }
    String map(const S57BaseObject* object) const{
        return mapper(this, object);
    }
};

static String formatTopAttributes(const AttributeEntry *entry,const S57BaseObject *object){
    String rt;
    bool first=false;
    for (auto it=entry->attributes.begin() ; it != entry->attributes.end();it++){
        if (object->attributes.hasAttr(*it)){
            if (first){
                first=false;
            }
            else{
                rt.push_back(' ');
            }
            rt.append(getConvertedAttrValue(object,*it,false));
        }
    }
    return rt;    
}
static void addParamV(String &v,const S57BaseObject *object,uint16_t id, const String &suffix="", const String &prefix=""){
    if (!object->attributes.hasAttr(id)) return;
    v.append(prefix);
    v.append(getConvertedAttrValue(object,id,false));
    v.append(suffix);
}
static String formatTopLight(const AttributeEntry *entry,const S57BaseObject *object){
    String rt;
    addParamV(rt,object,S57AttrIds::LITCHR," ");
    addParamV(rt,object,S57AttrIds::LITCHR," ");
    addParamV(rt,object,S57AttrIds::COLOUR," ");
    addParamV(rt,object,S57AttrIds::SIGGRP," ");
    addParamV(rt,object,S57AttrIds::SIGPER,"s ");
    addParamV(rt,object,S57AttrIds::HEIGHT,"m "); //TODO: depth unit?
    addParamV(rt,object,S57AttrIds::VALNMR,"nm "); 
    addParamV(rt,object,S57AttrIds::SECTR1,"\u00B0 - ","(");
    addParamV(rt,object,S57AttrIds::SECTR2,"\u00B0)");
    if (!rt.empty()) rt.push_back(' ');
    return rt;
}
class AttributeMappings : public std::vector<AttributeEntry*>{
    public:
    using std::vector<AttributeEntry*>::vector;
    const AttributeEntry *findEntry(uint16_t featureId){
        for (auto it=begin() ; it != end();it++){
            if ((*it)->MatchesFeature(featureId)){
                return *it;
            }
        }
        return nullptr;
    }
};
static AttributeMappings mappings={ 
  new AttributeEntry("buoy",{S57ObjectClasses::BOYSPP,S57ObjectClasses::BOYCAR,S57ObjectClasses::BOYINB,S57ObjectClasses::BOYISD,S57ObjectClasses::BOYLAT,S57ObjectClasses::BOYSAW},
    {S57AttrIds::OBJNAM,S57AttrIds::BOYSHP,S57AttrIds::COLOUR}, formatTopAttributes),
  new AttributeEntry("top",{S57ObjectClasses::TOPMAR},
    {S57AttrIds::OBJNAM,S57AttrIds::COLOUR,S57AttrIds::COLPAT,S57AttrIds::HEIGHT}, formatTopAttributes),
  new AttributeEntry("light",{S57ObjectClasses::LIGHTS},{}, formatTopLight) 
};

void S57ObjectDescription::jsonOverview(json::JSON &js) const{
    for (const auto  &[k,v]:out.ObjectRange()){
        js[k]=v;
    }
}
void S57ObjectDescription::buildOverview(const S57BaseObject *cobject){
    const AttributeEntry *converter=mappings.findEntry(cobject->featureTypeCode);
    if (converter == nullptr) return;
    out[converter->target]=converter->map(cobject);
    Coord::LatLon lon=Coord::worldxToLon(point.x);
    Coord::LatLon lat=Coord::worldyToLat(point.y);
    out["nextTarget"]=json::Array(lon,lat);
    if (cobject->attributes.hasAttr(S57AttrIds::OBJNAM)){
        out["name"]=cobject->attributes.getString(S57AttrIds::OBJNAM,true);
    }
}
void S57ObjectDescription::buildFull(const S57BaseObject *cobject){
    uint16_t tc=cobject->featureTypeCode;
    out["type"]=(int)type;
    out["s57typeCode"]=tc;
    out["s57Id"]=cobject->featureId;
    const S57ObjectClass *clz=S57ObjectClassesBase::getObjectClass(tc);
    if (clz){
        out["s57featureName"]=clz->ObjectClass;
        out["s57featureAcronym"]=clz->Acronym;
    }
    Coord::LatLon lat=Coord::worldyToLat(point.y);
    out["lat"]=lat;
    Coord::LatLon lon=Coord::worldxToLon(point.x);
    out["lon"]=lon;
    if (isPoint()){
        out["nextTarget"]=json::Array(lon,lat);
    }
    if (! expandedText.empty()){
        out["expandedText"]=expandedText;
    }
    json::JSON attributes=json::Array();
    for (auto it=cobject->attributes.begin();it!=cobject->attributes.end();it++){
        const S57AttributeDescription *description=S57AttrIdsBase::getDescription(it->first);
        json::JSON values;
        values["id"]=it->first;
        if (it->first==S57AttrIds::catgeo){
            //instead of the real catgeo
            //we use the geo primitive of the object
            ;
        }
        else
        {
            if (description)
            {
                values["name"] = description->Attribute;
                values["acronym"] = description->Acronym;
            }
            else
            {
                values["name"] = "UNKNOWN";
            }
            values["rawValue"] = cobject->attributes.getString(it->first, true);
            values["value"] = getConvertedAttrValue(cobject, it->first, false);
        }
        if (it->first == S57AttrIds::OBJNAM){
            out["name"]=cobject->attributes.getString(it->first,true);
        }
        attributes.append(values); 
    }
    {
        json::JSON values;
        values["acronym"] = "CATGEO";
        values["name"] = "Geographic Category";
        values["rawValue"] = (int)cobject->geoPrimitive;
        switch (cobject->geoPrimitive)
        {
        case s52::GEO_AREA:
            values["value"] = "Area";
            break;
        case s52::GEO_LINE:
            values["value"] = "Line";
            break;
        case s52::GEO_POINT:
            values["value"] = "Point";
            break;
        default:
            values["value"] = "Other";
            break;
        }
        attributes.append(values);
    }
    for (const auto &ao: addOns){
        json::JSON values;
        values["name"]=ao.first;
        values["acronym"]=ao.first;
        values["rawValue"]=ao.second;
        values["value"]=ao.second;
        attributes.append(values);
    }
    out["attributes"]=attributes;
}
void S57ObjectDescription::toJson(json::JSON &js) const{
    for (const auto  &[k,v]:out.ObjectRange()){
        js[k]=v;
    }    
}



ObjectDescription::Ptr S57Object::RenderObject::getObjectDescription(
    RenderContext &context,
    DrawingContext &drawing,
    const Coord::TileBox &box,
    bool overview,
    S57Object::RenderObject::StringTranslator translator) const{
    if (!Intersects(context.tileExtent,box)) return ObjectDescription::Ptr();
    if (!shouldRenderScale(context.s52Data->getSettings().get(),context.scale)) return ObjectDescription::Ptr();
    //do we need some extended checks?
    bool checkDraw=false;
    Coord::WorldXy ref=box.midPoint();
    if (object->geoPrimitive == s52::GEO_AREA){
        if (object->area.size() > 0){
            checkDraw=true;
            //check if we render any area rule
            //otherwise we must do our own check...
            bool hasAreaRule=false;
            for (const auto &rule:lup->ruleList){
                if (rule->type == s52::RUL_ARE_CO || 
                    rule->type == s52::RUL_ARE_PA
                )
                hasAreaRule=true;
                if (rule->type == s52::RUL_CND_SY){
                    const auto icrules=condRules.find(rule->key);
                    if (icrules != condRules.end()){
                        for (const auto &crule:icrules->second){
                            if (crule->type == s52::RUL_ARE_CO || 
                                crule->type == s52::RUL_ARE_PA) {
                                hasAreaRule=true;
                                break;
                            }
                        }
                        if (hasAreaRule) break;
                    }
                }
                if (hasAreaRule) break;
            }
            //if we would not render the area, we use an outline check
            if (! hasAreaRule) checkDraw=false;
        }
        if (! checkDraw){
            if (object->lines.size()>0){
                //check if surrounded by lines
                //return if not
                bool included=false;
                for (const auto &polygon : object->polygons){
                    if (polygon.pointInPolygon(ref)){
                        included=true;
                        break;
                    }
                }
                if (! included){
                    return ObjectDescription::Ptr();
                }
            }
            //no area and no lines? - no extended check
        }
    }
    else{
        if (object->lines.size() > 0){
            checkDraw=true;
        }
    }
    if (checkDraw)
    {
        drawing.resetDrawn();
        static const std::vector<s52::RenderStep> 
            steps({s52::RenderStep::RS_AREAS1,
                   s52::RenderStep::RS_AREAS2,
                   s52::RenderStep::RS_LINES,
                   s52::RenderStep::RS_POINTS});
        for (auto step : steps)
        {
            Render(context, drawing, box, step);
            if (drawing.getDrawn())
                break;
        }
        if (!drawing.getDrawn())
        {
            return ObjectDescription::Ptr();
        }
        else
        {
            int debug = 1;
        }
    }
    NameValueMap addOns;
    Sounding nearest;
    bool hasSounding=false;
    if (object->soundigs.size()>0){
        //special handling for sounding - find the nearest sounding
        double distance=std::numeric_limits<double>().max();
        for (const auto &sounding:object->soundigs){
            double ndis=((double)(ref.x-sounding.x))
                * ((double)(ref.x-sounding.x)) 
                + ((double)(ref.y-sounding.y))
                *  ((double)(ref.y-sounding.y));
            if (ndis < distance){
                hasSounding=true;
                nearest=sounding;
                distance=ndis;
            }
        }
        if (hasSounding){
            addOns["DEPTH"]=std::to_string(nearest.depth);
        }   
    }
    S57ObjectDescription *rt=new S57ObjectDescription(object.get(),overview,addOns);
    if (hasSounding){
        rt->point=nearest; //it will not be considered in the MD5
                           //but this is ok as we only check the 
                           //object itself
                           //as the objects will be sorted by distance
                           //and all soundings are the same
                           //we end up with the nearest sounding to 
                           //be displayed
    }
    if (object->attributes.hasAttr(S57AttrIds::TXTDSC)){
        rt->expandedText=translator(object->attributes.getString(S57AttrIds::TXTDSC,true));
    }
    
    return ObjectDescription::Ptr(rt);
}

//#define DEBUG_LINES
#ifdef DEBUG_LINES
using PP=std::pair<Coord::WorldXy,Coord::WorldXy>;
#define DLStore(x,y) points.push_back(PP({x,y}));
#else
#define PP(x,y);
#define DLStore(x,y);
#endif
inline static bool inRange(const Coord::World &start, const Coord::World &end, const Coord::World &p){
    if (end >= start){
        if (p>=start && p<end) return true;
    }
    else{
        if (p>end && p<=start) return true;
    }
    return false;
}
bool S57Object::Polygon::pointInPolygon(const Coord::WorldXy &ref) const
{
    // see https://wrfranklin.org/Research/Short_Notes/pnpoly.html
    // https://stackoverflow.com/questions/217578/how-can-i-determine-whether-a-2d-point-is-within-a-polygon
    // and eSENCChartisPointInObjectBoundary
    Coord::WorldXy point;
    bool hasPoint = false;
    bool direct = false;
    double drefx = ref.x;
    double drefy = ref.y;
    int intersects=0;
    #ifdef DEBUG_LINES
    std::vector<PP> points;
    #else
    int points=0; //dummy
    #endif
    LineIndex::SegmentIterator iter = [ref, drefx, drefy, &intersects, &point, &hasPoint, &direct, &points](Coord::WorldXy start, Coord::WorldXy end, bool isFirst)
    {
        if (direct)
            return;
        if (hasPoint && end == point)
            return;
        DLStore(start, end);
        point = end;
        hasPoint = true;
        if (start.x < ref.x && end.x < ref.x)
            return;
        const Coord::World epsilon = 3; // y diffs below eps
                                        // nearly horizontal line- special handling
        if (std::abs(start.y - end.y) <= epsilon)
        {
            if (inRange(start.y, end.y, ref.y))
            {
                // between start & end y
                if (ref.x >= start.x)
                {
                    // we consider it to be on the line
                    direct = true;
                    return;
                }
                intersects++;
                return;
            }
            return;
        }
        if (inRange(start.y, end.y, ref.y))
        {
            double startx = start.x;
            double starty = start.y;
            double endy = end.y;
            double endx = end.x;
            if (drefx < ((endx - startx) * (drefy - starty) / (endy - starty) + startx))
            {
                intersects++;
            }
        }
    };
    iterateSegments(iter,false);
    if (!isComplete){
        //add a closing segment
        if (startIndex >= 0 && startIndex < obj->lines.size() && endIndex >= 0 && endIndex < obj->lines.size()){
            Coord::WorldXy end=obj->lines[startIndex].firstPoint();
            Coord::WorldXy start=obj->lines[endIndex].lastPoint();
            iter(start,end,false);
        }
    }
    #ifdef DEBUG_LINES    
    if (Logger::instance()->HasLevel(LOG_LEVEL_DEBUG))
    {
        std::stringstream str;
        str << "##Start" << obj->featureId << std::endl;
        for (const auto &p : points)
        {
            str << p.first.x << ' ' << p.first.y << ' ' << (p.second.x - p.first.x) << ' ' << (p.second.y - p.first.y) << std::endl;
        }
        str << "##End" << obj->featureId << std::endl;
        LOG_DEBUG("Polygon type=%d, id=%d\n%s", obj->featureTypeCode,
                  obj->featureId, str.str());
    }
    #endif
    return intersects & 1;
}