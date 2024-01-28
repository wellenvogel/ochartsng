/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test Helper
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
#include <gtest/gtest.h>
#include <StringHelper.h>
#include "TestHelper.h"
#include <sstream>

TEST(StringHelper,ReplaceInline){        
    String t1("ToBeReplaced BeReplaced");
    StringHelper::replaceInline(t1, "BeR", "NeverBeR");
    EXPECT_EQ(t1,String("ToNeverBeReplaced NeverBeReplaced"));
}
TEST(StringHelper,unsafeJson){
    String t2("\"unsafeJson\":aha\n");
    String sf=StringHelper::safeJsonString(t2);
    EXPECT_EQ(sf,String("\\\"unsafeJson\\\":aha "));
}
TEST(StringHelper,malicousFileName){
    String t3("malicous\\\\/File**?Name");
    String t3s=StringHelper::SanitizeString(t3);
    EXPECT_EQ(t3s,String("malicous___File___Name"));
}
String toHex(const String &in){
    std::stringstream out;
    const char *p=in.c_str();
    while (*p != 0){
        out << StringHelper::format("%02x",(unsigned char)(*p));
        p++;
    }
    return out.str();
}
TEST(StringHelper,safeHtmlString){
    String t4("<SomeStrange HtmlÖÄÜ");
    std::cerr << toHex(t4) <<std::endl;
    String t4s=StringHelper::safeHtmlString(t4);
    std::cerr << toHex(t4s) <<std::endl;
    EXPECT_EQ(t4s,String("&lt;SomeStrange HtmlÖÄÜ"));
}
TEST(StringHelper,toLowerNoUtf8){
    String t4("UpperCase MixedöÖ");
    std::cerr << toHex(t4) <<std::endl;
    String t4s=StringHelper::toLower(t4);
    std::cerr << toHex(t4s) <<std::endl;
    EXPECT_EQ(t4s,String("uppercase mixedöÖ"));
}
TEST(StringHelper,toLowerInline){
    String t4("UpperCase MixedöÖ");
    std::cerr << toHex(t4) <<std::endl;
    StringHelper::toLowerI(t4);
    std::cerr << toHex(t4) <<std::endl;
    EXPECT_EQ(t4,String("uppercase mixedöÖ"));
}
TEST(StringHelper,toLowerUtf8){
    String t4("UpperCaseÖÄÜ MixedöÖüÜ");
    std::cerr << toHex(t4) <<std::endl;
    String t4s=StringHelper::toLower(t4,true);
    std::cerr << toHex(t4s) <<std::endl;
    EXPECT_EQ(t4s,String("uppercaseöäü mixedööüü"));
}
TEST(StringHelper,toUpperNoUtf8){
    String t4("UpperCase MixedöÖ");
    std::cerr << toHex(t4) <<std::endl;
    String t4s=StringHelper::toUpper(t4);
    std::cerr << toHex(t4s) <<std::endl;
    EXPECT_EQ(t4s,String("UPPERCASE MIXEDöÖ"));
}
TEST(StringHelper,toUpperInline){
    String t4("UpperCase MixedöÖ");
    std::cerr << toHex(t4) <<std::endl;
    StringHelper::toUpperI(t4);
    std::cerr << toHex(t4) <<std::endl;
    EXPECT_EQ(t4,String("UPPERCASE MIXEDöÖ"));
}
TEST(StringHelper,toUpperUtf8){
    String t4("UpperCaseÖÄÜ MixedöÖüÜß");
    std::cerr << toHex(t4) <<std::endl;
    String t4s=StringHelper::toUpper(t4,true);
    std::cerr << toHex(t4s) <<std::endl;
    EXPECT_EQ(t4s,String("UPPERCASEÖÄÜ MIXEDÖÖÜÜß"));
}
TEST(StringHelper,format){
    String t5("Some (%d) test for float(%.1f) and String(%s)");
    String t5s=StringHelper::format(t5.c_str(), 1, 0.3, String("aha").c_str());
    EXPECT_EQ(t5s,String("Some (1) test for float(0.3) and String(aha)"));
}

TEST(StringHelper,split){
    String t6("Some string to be split");
    StringVector rs = StringHelper::split(t6, " ");
    EXPECT_EQ(rs.size(),5);
    EXPECT_EQ(rs[0],String("Some"));
    EXPECT_EQ(rs[4],String("split"));
}
TEST(StringHelper,splitLimit){
    String t6("Some string to be split");
    StringVector rs = StringHelper::split(t6, " ",1);
    EXPECT_EQ(rs.size(),2);
    EXPECT_EQ(rs[0],String("Some"));
    EXPECT_EQ(rs[1],String("string to be split"));
}

TEST(StringHelper,rtrim){
    String t("string with garbage  \n");
    String ts=StringHelper::rtrim(t);
    EXPECT_EQ(ts,String("string with garbage"));
}
TEST(StringHelper,beforeFirst){
    String t("string :part1 :part2");
    String ts=StringHelper::beforeFirst(t,":");
    EXPECT_EQ(ts,String("string "));
}
TEST(StringHelper,afterLast){
    String t("string :part1 :part2");
    String ts=StringHelper::afterLast(t,":");
    EXPECT_EQ(ts,String("part2"));
}
TEST(StringHelper,beforeFirstNotFound){
    String t("string :part1 :part2");
    String ts=StringHelper::beforeFirst(t,",");
    EXPECT_TRUE(ts.empty());
}
TEST(StringHelper,endsWith){
    String t("string with end");
    bool ts=StringHelper::endsWith(t," end");
    EXPECT_TRUE(ts);
    ts=StringHelper::endsWith(t,String(" end"));
    EXPECT_TRUE(ts);
}
TEST(StringHelper,endsWithNotFound){
    String t("string with end");
    bool ts=StringHelper::endsWith(t," en");
    EXPECT_FALSE(ts);
    ts=StringHelper::endsWith(t,String(" en"));
    EXPECT_FALSE(ts);
}

TEST(StringHelper,startsWith){
    String t("string with end");
    bool ts=StringHelper::startsWith(t,"string ");
    EXPECT_TRUE(ts);
    ts=StringHelper::startsWith(t,String("string "));
    EXPECT_TRUE(ts);
}
TEST(StringHelper,startsWithNotFound){
    String t("string with end");
    bool ts=StringHelper::startsWith(t," string");
    EXPECT_FALSE(ts);
    ts=StringHelper::startsWith(t,String(" string"));
    EXPECT_FALSE(ts);
}
TEST(StringHelper,urlEncode){
    String t("string with &and Ö and =");
    String ts=StringHelper::urlEncode(t);
    EXPECT_EQ(ts,String("string%20with%20%26and%20%C3%96%20and%20%3D"));
}
TEST(StringHelper,unescapeUri){
    String t("string%20with%20%26and%20%C3%96%20and%20%3D");
    String texp("string with &and Ö and =");
    String ts=StringHelper::unescapeUri(t);
    EXPECT_EQ(ts,texp);
}
TEST(StringHelper,unescapeUriPlus){
    String t("string+with+%26and+%C3%96+and+%3D");
    String texp("string with &and Ö and =");
    String ts=StringHelper::unescapeUri(t);
    EXPECT_EQ(ts,texp);
}
TEST(StringHelper,cloneData){
    String t("string to be cloned");
    char * data=StringHelper::cloneData(t);
    t.clear();
    t.append("new text");
    EXPECT_NE(String(data),String("new text"));
    EXPECT_EQ(String(data),String("string to be cloned"));
    delete data;
    EXPECT_EQ(t,String("new text"));
}
TEST(StringHelper,concat){
    StringVector t={
        String("p1"),
        String("p2"),
        String("p3")
    };
    String ts=StringHelper::concat(t,",");
    EXPECT_EQ(ts,String("p1,p2,p3"));
}
TEST(StringHelper,concatPtr){
    const char *t[]={
        "p1",
        "p2",
        "p3"
    };
    String ts=StringHelper::concat(t,3,",:");
    EXPECT_EQ(ts,String("p1,:p2,:p3"));
}
TEST(StringHelper,formatMultiple){
    int x=20;
    String t=StringHelper::format("String being longer then %d bytes - for sure!",x);
    EXPECT_STREQ(t.c_str(),"String being longer then 20 bytes - for sure!");
    x=30;
    String t2=StringHelper::format("String being longer then %d bytes - for sure!",x);
    EXPECT_STREQ(t2.c_str(),"String being longer then 30 bytes - for sure!");
}
TEST(StringHelper,formatFloat){
    float x=20.3333;
    String t=StringHelper::format("Float %0.3f %s",x,"test");
    EXPECT_STREQ(t.c_str(),"Float 20.333 test");    
}
TEST(StringHelper,formatDouble){
    double x=20.3333;
    String t=StringHelper::format("Float %0.3f",x);
    EXPECT_STREQ(t.c_str(),"Float 20.333");    
}
TEST(StringHelper,formatLong){
    int64_t x=20L;
    String t=StringHelper::format("LongLong %lld%s",x,"test");
    EXPECT_STREQ(t.c_str(),"LongLong 20test");    
}
TEST(StringHelper,formatUnknown){
    int64_t x=20L;
    String t=StringHelper::format("LongLong %llZ%s",x,"test");
    EXPECT_STREQ(t.c_str(),"LongLong 20test");    
}
TEST(StringHelper,formatPercent){
    int64_t x=20L;
    String t=StringHelper::format("%% LongLong %d%s",x,"test");
    EXPECT_STREQ(t.c_str(),"% LongLong 20test");    
}
TEST(StringHelper,StreamFormat1){
    std::stringstream s;
    StringHelper::StreamFormat(s,"str1:%s str2:%s",String("first"),"second");
    String res=s.str();
    EXPECT_EQ(res,String("str1:first str2:second"));
}
TEST(StringHelper,StreamFormatHex){
    std::stringstream s;
    StringHelper::StreamFormat(s,"hex:%08X",12);
    String res=s.str();
    EXPECT_EQ(res,String("hex:0000000C"));
}
TEST(StringHelper,StreamFormatMiss){
    std::stringstream s;
    StringHelper::StreamFormat(s,"str1:%s str2:%s str3:%s",String("first"),"second");
    String res=s.str();
    EXPECT_EQ(res,String("str1:first str2:second str3:%s"));
}
TEST(StringHelper,Trim1){
    String t1="'t1'";
    StringHelper::trimI(t1,'\'');
    EXPECT_STREQ(t1.c_str(),"t1");
}
TEST(StringHelper,Trim2){
    String t1="'t'1''";
    StringHelper::trimI(t1,'\'');
    EXPECT_STREQ(t1.c_str(),"t'1");
}
TEST(StringHelper,Trim){
    String t1="'t1'";
    EXPECT_STREQ(StringHelper::trim(t1,'\'').c_str(),"t1");
}
TEST(StringHelper,StreamArrayFormat1){
    std::stringstream s;
    StringHelper::StreamArrayFormat(s,"str1:%s str2:%s",StringVector({String("first"),"second"}));
    String res=s.str();
    EXPECT_EQ(res,String("str1:first str2:second"));
}
TEST(StringHelper,StreamArrayFormatMoreArgs){
    std::stringstream s;
    int next=StringHelper::StreamArrayFormat(s,"str1:%s str2:%s",StringVector({String("first"),"second","third"}));
    String res=s.str();
    EXPECT_EQ(res,String("str1:first str2:second"));
    EXPECT_EQ(next,2);
}
TEST(StringHelper,StreamArrayFormatLessArgs){
    std::stringstream s;
    int next=StringHelper::StreamArrayFormat(s,"str1:%s str2:%s",StringVector({String("first")}));
    String res=s.str();
    EXPECT_EQ(res,String("str1:first str2:%s"));
    EXPECT_EQ(next,1);
}

TEST(StringHelper,StreamArrayCb1){
    std::stringstream s;
    int next=StringHelper::StreamArrayFormat(s,"str1:%s int1:%03d float1:%04.1f float2:%04.1f",StringVector({"first","3","5.1","-6.2"}),StringHelper::DefaultArgCallback);
    String res=s.str();
    EXPECT_EQ(res,String("str1:first int1:003 float1:05.1 float2:-6.2"));
    EXPECT_EQ(next,4);
}