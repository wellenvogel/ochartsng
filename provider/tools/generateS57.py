#! /usr/bin/env python3

import os
import sys
import csv

def log(txt):
    print("LOG: %s"%(txt))
def err(txt,doExit=True):
    print("ERROR: %s"%txt)
    if doExit:
        sys.exit(1)    


def generator(dct,cl,FIELDS):
    INTF=["Code","ID"]
    ENUMF={"Attributetype":"%s::Type"%cl,"Class":"%s::ClassType"%cl}
    rt="%s("%cl;
    first=True
    for f in FIELDS:
        v=dct.get(f)
        if v is None or v == '':
            if f in INTF:
                v="0"
            elif f in ENUMF.keys():
                v="U"    
            else:
                v=""
        if not first:
            rt+=","
        first=False            
        if f in INTF:
            rt+=v
        elif f in ENUMF.keys():
            rt+="%s::%s"%(ENUMF[f],v)
        else:
            rt+="\"%s\""%v
    rt+=")"
    return rt

def s57attvalues(dct):
    FIELDS=["Code","ID","Meaning"]
    return generator(dct,"S57AttributeValue",FIELDS)
class OCParam:
    def __init__(self):
        self.base="S57OBJECTCLASSES"
        self.classbase="S57ObjectClasses"
        self.itemclass="S57ObjectClass"
        self.builder=lambda dct: generator(dct,self.itemclass,self.FIELDS)
        self.keyf=lambda row: row["Code"] 
        self.genIds=True
        self.genNames=True
        self.FIELDS=["Code","ObjectClass","Acronym","Class"]

class ATTParam:
    def __init__(self) -> None:
        self.base="S57ATTRDESCRIPTIONS"
        self.classbase="S57AttrIds"
        self.itemclass="S57AttributeDescription"
        self.builder=lambda dct: generator(dct,self.itemclass,self.FIELDS)
        self.keyf=lambda row: row["Code"] 
        self.genIds=True
        self.genNames=True
        self.FIELDS=["Code","Attribute","Acronym","Attributetype"]
class ATTValues:
    def __init__(self) -> None:
        self.base="S57ATTRVALUEDESCRIPTIONS"
        self.classbase="S57AttrValues"
        self.itemclass="S57AttrValueDescription"
        self.builder=lambda dct: generator(dct,self.itemclass,self.FIELDS)
        self.keyf=lambda row: "{%s,%s}"%(row["Code"],row["ID"])
        self.genIds=False
        self.genNames=False
        self.FIELDS=["Code","ID","Meaning"]


def generateS57(inputFile, outputFile,param):
    ensureDir(outputFile)
    with open(outputFile,"w") as oh:
        oh.write("/*generated from %s*/\n"%inputFile)
        if p.genIds:
            oh.write("#ifndef _{p.base}_H\n".format(p=param))
            oh.write("#define _{p.base}_H\n".format(p=param))
            oh.write("#include \"S57.h\"\n")
            oh.write("class {p.classbase} : public {p.classbase}Base {{\n".format(p=param))
            oh.write("public:\n");
            with open(inputFile) as ih:
                reader=csv.DictReader(ih)
                for row in reader:
                    oh.write("    static const constexpr uint16_t %s=%s;\n"%(row['Acronym'],param.keyf(row)))
            oh.write("};\n")
            oh.write("#endif\n")
        oh.write("#ifdef {p.base}_IMPL\n".format(p=param))
        oh.write("const {p.itemclass}::IdMap * {p.classbase}Base::idMap=new {p.itemclass}::IdMap({{\n".format(p=param))
        with open(inputFile) as ih:
            reader=csv.DictReader(ih)
            first=True
            for row in reader:
                if not first:
                    oh.write(",\n")
                first=False    
                oh.write("    {%s,new %s}"%(param.keyf(row),param.builder(row)))
        oh.write("\n    });\n")
        if p.genNames:
            oh.write("const {p.itemclass}::NameMap *{p.classbase}Base::nameMap=new {p.itemclass}::NameMap({{\n".format(p=param))
            with open(inputFile) as ih:
                reader=csv.DictReader(ih)
                first=True
                for row in reader:
                    if not first:
                        oh.write(",\n")
                    first=False    
                    oh.write("    {\"%s\",%s}"%(row['Acronym'],param.keyf(row)))
            oh.write("\n    });\n")        
        oh.write("#endif\n")

def ensureDir(outputFile):
    dn=os.path.dirname(outputFile)
    if not os.path.isdir(dn):
        log("creating %s"%(dn))
        os.makedirs(dn)

def usage(txt=None):
    if txt is not None:
        err(txt,False)
    err("usage: %s classes|attributes|attrvalues infile outfile"%sys.argv[0])    


if __name__=='__main__':
    if len(sys.argv) != 4:
        usage()
    if sys.argv[1] == 'classes':
        p=OCParam()
        generateS57(sys.argv[2],sys.argv[3],p)
        sys.exit(0)
    if sys.argv[1] == 'attributes':
        p=ATTParam()
        generateS57(sys.argv[2],sys.argv[3],p)
        sys.exit(0)
    if sys.argv[1] == 'attrvalues':
        p=ATTValues()
        generateS57(sys.argv[2],sys.argv[3],p)
        sys.exit(0)    
    usage("invalid mode %s"%sys.argv[1])        