let activeRequest = undefined;
let currentUploadRequest;
class Fpr{
    constructor(name,value){
        this.name=name
        this.value=value;
    }
};
class ShopLogin{
    constructor(){
        this.user=undefined
        this.password=undefined
        this.key=undefined
        this.fpr=undefined
        this.dongleFpr=undefined
        this.systemName=undefined;
        this.dongleSystemName=undefined; //dongle name if received in a list request
        this.localDongleName=undefined; //dongle name retrieved locally
        this.hasDongle=false;
        this.chartsXml=undefined
        this.knownSystems=[]; 
    }
    isLoggedIn(){
        return this.key !== undefined && this.user !== undefined;
    }
    login(user,password,key){
        this.user=user;
        this.password=password;
        this.key=key;
    }
    logout(){
        this.user=undefined;
        this.password=undefined;
        this.key=undefined;
        this.systemName=undefined;
        this.dongleSystemName=undefined;
        this.chartsXml=undefined;
        this.knownSystems=[];
    }
    updateLocalDongleName(name,forceUnset){
        if (name || forceUnset){
            this.localDongleName=name;
        }
        this.dongleSystemName=undefined;
        if (! this.localDongleName) return;
        for (let i=0;i<this.knownSystems.length;i++){
            if (this.knownSystems[i] === this.localDongleName){
                this.dongleSystemName=this.localDongleName;
            }
        }
    }
    setKnownSystems(names){
        this.knownSystems=names;
        this.updateLocalDongleName();
    }
}
let getPredefinedSystemName=()=>{
    fetch("/shop/predefinedname")
    .then((r)=>r.json())
    .then((json)=>{
        if (json.status === 'OK' && json.data && json.data.name){
            avnavutil.forEl("#shopSystemName",(el)=>el.value=json.data.name);
        }
    })
    .catch((e)=>{
       console.log(e); 
    })
}
let showBusy=(txt)=>{
    if (! txt){
        avnavutil.setHidden(".overlay",true);
        return;
    }
    avnavutil.forEl("#overlayText",(el)=>el.textContent=txt);
    avnavutil.setHidden(".overlay",false);
}
let shopRequest=(task,parameters)=>{
    let url="/shop/api?version=r.1.0.0&taskId="+encodeURIComponent(task);
    for (let k in parameters){
        url+="&"+encodeURIComponent(k)+"="+encodeURIComponent(parameters[k]);
    }
    return fetch(url)
        .then(resp => resp.text())
        .then(str => new window.DOMParser().parseFromString(str, "text/xml"));
}
let shopLogin=new ShopLogin();

const shopErrors={
    '3d': 'empty username',
    '3e': 'invalid username',
    '3f': 'empty password',
    '3g': 'wrong password',
    '8l': 'no system name for this device',
    '8h': 'something has changed in the device assigned to this system name',
    '8j': 'there is already a system name for this device',
    //our own errors
    'AV1': 'internal error',
    'AV2': 'communication error',
    'AV3': 'invalid shop url',
    'AV4': 'unable to connect to the shop (no internet?)',
    //plain numbers - seems to be n:...
    '2' : 'server in maintenance mode',
    '4' : 'user does not exist',
    '5' : 'invalid version of the plugin',
    '6' : 'invalid user/email name or password',
    '10': 'system name is disabled',
    '20': 'chart already assigned to this system',

};
class ShopError{
    constructor(code,text){
        this.code=code;
        if (text){
            this.text=text;
        }
        else{
            let lookup=code.replace(/:.*/,'');
            this.text=shopErrors[lookup];
        }
    }
    toString(){
        let rt='';
        if (this.text) rt+=this.text+"\n";
        if (this.code !== undefined) rt+="Code: "+this.code;
        return rt;
    }
};
let getShopResultError=(xml)=>{
    let response=xml.getElementsByTagName("response")[0];
    if (! response){
        return new ShopError(0,"empty response");
    }
    let code=response.getElementsByTagName("result")[0];
    if (code === undefined || code === null || code === ""){
        return new ShopError(0, "unknown response");
    }
    code=code.textContent;
    if (code == 1) return;
    return new ShopError(code);
}
let shopAssignSystem=(inputId,forDongle)=>{
    if (! shopLogin.isLoggedIn()) return;
    if (forDongle && shopLogin.dongleFpr === undefined){
        alert("unable to get an fpr for the dongle");
        return;
    }
    if (!forDongle && shopLogin.fpr === undefined){
        alert("unable to get an fpr");
        return;
    } 
    let systemName=undefined;
    avnavutil.forEl("#"+inputId,(el)=>systemName=el.value);
    if (! systemName) {
        alert(inputId+" must not be empty");
        return;
    }
    if (! confirm("Ready to assign "+systemName+" to this system?\nThis cannot be undone.")){
        return;
    }
    showBusy("registering");
    shopRequest("xfpr",{
        username:shopLogin.user,
        key:shopLogin.key,
        xfprName:forDongle?shopLogin.dongleFpr.name:shopLogin.fpr.name,
        xfpr:forDongle?shopLogin.dongleFpr.value:shopLogin.fpr.value,
        systemName:systemName
    })
    .then((xml)=>{
        let err=getShopResultError(xml);
        if (err) throw new Error(err.toString());
        identifySystem(forDongle);
    })
    .catch((e)=>{
        alert(e);
        showBusy();
    })
}
let updateShopStatus=()=>{
    if (shopLogin.isLoggedIn()){
        avnavutil.setHidden(".onlyLoggedIn",false);
        avnavutil.setEnabled(".notLoggedIn",false);
        avnavutil.setEnabled(".loggedIn",true);
        avnavutil.forEl(".loginStatus",(el)=>el.textContent="logged in");            
    }
    else{
        avnavutil.setHidden(".onlyLoggedIn",true);
        avnavutil.setEnabled(".loggedIn",false);
        avnavutil.setEnabled(".notLoggedIn",true);
        avnavutil.forEl(".loginStatus",(el)=>el.textContent="not logged in");
        showChartList(undefined,false);
        showChartList(undefined,true);

    }
    let systemText="";
    if (shopLogin.systemName){
        avnavutil.forEl(".shopChartList .noDongle .shopSystemName",(el)=>el.textContent=shopLogin.systemName);
        avnavutil.setHidden(".registerSystem .noDongle",true);
        systemText+="System: "+shopLogin.systemName;
    }
    else{
        systemText+="System: not registered";
        avnavutil.setHidden(".registerSystem .noDongle",false);
        avnavutil.setEnabled("#shopAssignSystemName",true);
    }
    avnavutil.setHidden(".dongle",!shopLogin.hasDongle);
    if (shopLogin.hasDongle){
        if (shopLogin.dongleSystemName){
            avnavutil.forEl(".shopChartList .dongle .shopSystemName",(el)=>el.textContent=shopLogin.dongleSystemName);
            avnavutil.setHidden(".registerSystem .dongle",true);
            systemText+=", Dongle: "+shopLogin.dongleSystemName;
        }
        else{
            systemText+=", Dongle: not registered";
            avnavutil.setHidden(".registerSystem .dongle",false);
            avnavutil.setEnabled("#shopAssignSystemNameDongle",true);
        }
    }
    avnavutil.forEl(".systemStatus",(el)=>el.textContent=systemText);
}
let fetchFpr=(forDongle)=>{
    let url="/shop/fpr";
        if (forDongle) url+="?dongle=true";
        return fetch(url)
            .then((res)=>res.json())
            .then((json)=>{
                if (json.status != "OK" || ! json.data) throw new Error("invalid status: "+(json.info || json.status));
                if (forDongle){
                    shopLogin.dongleFpr=new Fpr(json.data.name,json.data.value.toUpperCase());
                }
                else{
                    shopLogin.fpr=new Fpr(json.data.name,json.data.value.toUpperCase());
                }
                return true;
            })
            .catch((e)=>{
                console.log("unable to get fpr ",forDongle,e);
                return false;
            })
}
let identifySystem = () => {
    if (!shopLogin.isLoggedIn()) return false;
    showBusy("identify");
    return shopRequest("identifySystem", {
        username: shopLogin.user,
        key: shopLogin.key,
        xfprName: shopLogin.fpr.name,
        xfpr: shopLogin.fpr.value
    })
        .then(xml => {
            showBusy();
            let rt = false;
            let err = getShopResultError(xml);
            if (err) {
                //TODO: check for errors other then 8l
            }
            else {
                let systemName = xml.getElementsByTagName("response")[0].getElementsByTagName("systemName")[0].textContent;
                if (systemName) {
                    rt = true;
                    shopLogin.systemName = systemName;
                }
            }
            updateShopStatus();
            return rt;
        })

}
let tryLogin=()=>{
    let user=avnavutil.getValue("#shopUser");
    let pass=avnavutil.getValue("#shopPassword");
    if (! user || ! pass){
        alert("user or password must not be empty");
        return;
    }
    showBusy("login");
    shopRequest("login",{
        username:user,
        password:pass
    })
    .then(xml=>{
        let err=getShopResultError(xml);
        if (err) throw new Error(err.toString());
        let key=xml.getElementsByTagName("response")[0].getElementsByTagName("key")[0].textContent;
        if (! key){
            throw Error("no key from shop");
        }
        shopLogin.login(user,pass,key);
        getChartList()
        .then(()=> {
            updateShopStatus();
            return identifySystem();
        });
    })
    .catch((e)=>{
        alert("Login failed:"+e);
        shopLogin.logout();
        showBusy();
        updateShopStatus();
    });
}

let startShopDownload=(uuid,edition,forDongle)=>{
    if (! shopLogin.isLoggedIn()) return false;
    if (forDongle && ! shopLogin.hasDongle) return false;
    let systemName=forDongle?shopLogin.dongleSystemName:shopLogin.systemName;
    if (!systemName) return false;
    showBusy("get chart url");
    shopRequest("request",{
        assignedSystemName:systemName,
        slotUuid: uuid,
        requestedFile: "base",
        requestedVersion: edition,
        username: shopLogin.user,
        key: shopLogin.key
    })
    .then((xml)=>{
        showBusy();
        let err=getShopResultError(xml);
        if (err) throw new Error(err.toString());
        let url=avnavutil.getXmlVal(xml,"response.file.link");
        let keyUrl=avnavutil.getXmlVal(xml,"response.file.chartKeysLink");
        if (!url || ! keyUrl) throw new Error("missing url or keyUrl");
        avnavutil.forEl("#shopUrl",(el)=>el.textContent=url);
        avnavutil.forEl("#keyUrl",(el)=>el.textContent=keyUrl);
        return fetch("/upload/downloadShop?chartUrl=" + encodeURIComponent(url) + "&keyUrl=" + encodeURIComponent(keyUrl))
            .then((res) => res.json())
            .then((jsonData) => {
                if (jsonData.status !== "OK" || ! jsonData.data) {
                    throw new Error(jsonData.info||jsonData.status||"no status");
                }
                else {
                    console.log("download triggered for"+jsonData.data.request);
                    activeRequest = jsonData.data.request;
                }
            })
    })
    .catch((e)=>{
        alert("unable to fetch shop info: "+e);
        showBusy();
    })

};

let localChartSets=[];
let showChartList=(xml,forDongle)=>{
    let content=forDongle?".shopChartList .dongle .shopChartListContent":".shopChartList .noDongle .shopChartListContent";
    avnavutil.forEl(content,(el)=>el.textContent="");
    let charts=xml?xml.getElementsByTagName("response")[0].getElementsByTagName("chart"):[];
    if (charts.length < 1){
        return;
    }
    let listBase=document.querySelector(content);
    if (! listBase) return;
    let foundCharts=[];
    for (let i=0;i<charts.length;i++){
        let chart=charts[i];
        let expired=avnavutil.getXmlVal(chart,"expired");
        if (expired == 1) continue;
        let name=avnavutil.getXmlVal(chart,"chartName");
        let edition=avnavutil.getXmlVal(chart,"edition");
        let id=avnavutil.getXmlVal(chart,"chartId");
        let quantities=chart.getElementsByTagName("quantity");
        if (quantities.length < 1) continue;
        for (let qi=0;qi<quantities.length;qi++){
            let quantity=quantities[qi];
            let slots=quantity.getElementsByTagName("slot");
            if (slots.length < 1) break;
            for (let si=0;si<slots.length;si++){
                let assignedName=avnavutil.getXmlVal(slots[si],"assignedSystemName");
                if ((forDongle && assignedName == shopLogin.dongleSystemName)
                    || (!forDongle && assignedName == shopLogin.systemName ) ){
                        //found an assigned set
                        let chartEntry={
                            name:name,
                            edition:edition,
                            id:id
                        };
                        chartEntry.slotUuid=avnavutil.getXmlVal(slots[si],"slotUuid");
                        foundCharts.push(chartEntry);
                    }
            }
        }
    }
    if (foundCharts.length < 1){
        avnavutil.forEl(content,(el)=>el.textContent="");
    }
    else{
        for(let ce in foundCharts){
            //find local set
            let localCharts=[];
            let chartEntry=foundCharts[ce];
            for (let k in localChartSets){
                let set=localChartSets[k];
                if (!set.info) continue;
                if (set.info.id == chartEntry.id){
                    localCharts.push(set.info);
                }
            }
            let el=avnavutil.addEl("div","shopChartEntry",listBase);
            let row=avnavutil.addEl("div","row",el);
            let btText=undefined;
            let lbText=undefined;
            if (localCharts.length < 1){
                btText="Download";
            }
            else{
                let hasSameVersion=false;
                for (let li in localCharts){
                    if (localCharts[li].version == chartEntry.edition){
                        hasSameVersion=true;
                        break;
                    }
                }
                if (hasSameVersion){
                    lbText="Existing";
                }
                else{
                    btText="Update";
                }
            }
            if (lbText !== undefined){
                avnavutil.addEl("span","label",row,lbText);
            }
            else{
                let bt=avnavutil.addEl("button","downloadChart",row,btText);
                bt.disabled=!canInstall;
                bt.addEventListener("click",(ev)=>{
                    startShopDownload(chartEntry.slotUuid,chartEntry.edition,forDongle);
                })
            }
            avnavutil.addEl("span","value",row,chartEntry.name);
            avnavutil.addEl("span","value",row,chartEntry.edition);
        }
    }
}
let getChartList=()=>{
    if (! shopLogin.isLoggedIn()) return false;
    showBusy("chartlist");
    return shopRequest("getlist",{
        username: shopLogin.user,
        key: shopLogin.key
    })
    .then((xml)=>{
        let err=getShopResultError(xml);
        if (err) throw new Error(err.toString());
        shopLogin.chartsXml=xml
        let systems=xml.getElementsByTagName("response")[0].getElementsByTagName("systemName");
        if (systems.length > 0){
            let names=[];
            for (let i=0;i<systems.length;i++){
                names.push(systems[i].textContent);
            }
            shopLogin.setKnownSystems(names);
        }
        showBusy();
        return xml;
    });
}
let buildSet = (status) => {
    let url = window.location.protocol + "//" + window.location.host + "/charts/" + status.info.name;
    return `<div class="chart"> 
                    <div><span class="label">disabled?</span><span class="rpar">${status.disabledBy}</span></div>
                    <div><span class="label">title</span><span class="rpar">${status.info.title}</span></div>
                    <div><span class="label">key</span><span class="rpar">${status.info.name}</span></div>
                    <div><span class="label">version</span><span class="rpar">${status.info.version}</span></div>
                    <div><span class="label">directory</span><span class="rpar">${status.info.directory}</span></div>
                    <div><span class="label">number</span><span class="rpar">${status.charts.numCharts}</span></div>
                    <div><span class="label">valid</span><span class="rpar">${status.numValidCharts}</span></div>
                    <div><span class="label">url</span><span class="rpar">${url}</span></div>
                    <div><span class="label">avnav.xml</span><span class="rpar"><a href="${url}/avnav.xml">${url}/avnav.xml</a></span></div>
                    <div><button data-name="${status.info.name}" onClick="deleteSet('${status.info.name}')">Delete</button></div>
                    </div>`;
}
let translateState = (state) => {
    switch (state) {
        case 0: return "waiting";
        case 1: return "downloading";
        case 2: return "uploading";
        case 3: return "unpacking";
        case 4: return "testing";
        case 5: return "parsing";
        case 6: return "error";
        case 7: return "interrupted";
        case 8: return "finished";
            return "unknown";
    }
}
let deleteSet = (t) => {
    if (confirm("Really delete " + t)) {
        console.log("deleteSet", t);
        fetch("/upload/deleteSet?name=" + encodeURIComponent(t))
            .then((res) => res.json())
            .then((json) => {
                if (json.status !== "OK") {
                    alert("delete failed: " + json.info);
                }
            })
            .catch((err) => alert(err));
    }
};
let checkDongle=()=>{
    fetch("/shop/donglepresent")
    .then((res)=>res.json())
    .then((json)=>{
        if (json.status === 'OK' && json.data){
            let oldState=shopLogin.hasDongle;
            shopLogin.hasDongle=json.data.present;
            if (shopLogin.hasDongle !== oldState || (shopLogin.hasDongle && ! shopLogin.localDongleName)){
                if (shopLogin.hasDongle){
                    fetchFpr(true)
                    .then(()=>{})
                    .catch((e)=>console.log("error dongle fpr",e));
                    fetch("/shop/donglename")
                    .then((res)=>res.json())
                    .then((json)=>{
                        if (json.status === 'OK' && json.data){
                            avnavutil.forEl("#shopSystemNameDongle",(el)=>el.value=json.data.name);
                            avnavutil.forEl("#dongleName",(el)=>el.textContent=json.data.name);
                            shopLogin.updateLocalDongleName(json.data.name);
                        }
                        else{
                            shopLogin.updateLocalDongleName(undefined,true);
                            console.log("unable to get dongle name",json.status,json.info);
                            avnavutil.forEl("#dongleName",(el)=>el.textContent="????");
                        }
                        updateShopStatus();
                    })
                    .catch((e)=>{
                        console.log("unable to get dongle name",e);
                        avnavutil.forEl("#dongleName",(el)=>el.textContent="????");
                        shopLogin.updateLocalDongleName(undefined,true);
                        updateShopStatus();
                    })
                }
                else{
                    updateShopStatus();
                    avnavutil.forEl("#dongleName",(el)=>el.textContent="not present");
                }
            }
        }
    })
    .catch((e)=>console.log("unable to fetch donglepresent: ",e));
}
window.onload = () => {
    let fi = document.getElementById("fileInput");
    fi.addEventListener("change", (ev) => {
        console.log("file change");
        let fileObject = ev.target;
        if (!fileObject.files || fileObject.files.length < 1) {
            return;
        }
        let file = fileObject.files[0];
        if (!file.name.match(/\.zip$/i)) {
            alert("only files .zip are allowed");
            return;
        }
        let stEl = document.getElementById("uploadStatus");
        stEl.textContent = "initiated";
        let handler = {};
        handler.errorhandler = (error) => {
            currentUploadRequest = undefined;
            alert(error);
        };
        handler.starthandler = (xhdr) => {
            currentUploadRequest = xhdr;
            stEl.textContent = "started";
            console.log("upload started...");
        };
        handler.progresshandler = (ev) => {
            console.log("upload progress...");
            let percent = Math.floor(ev.loaded * 100 / ev.total);
            stEl.textContent = percent + "%";
        };
        handler.okhandler = (jsonData) => {
            stEl.textContent = "done";
            currentUploadRequest = undefined;
            //alert("upload ok "+jsonData.request);
            activeRequest = (jsonData.data.request !== undefined) ? jsonData.data.request : undefined;
        };
        activeRequest = undefined;
        window.avnavutil.uploadFile("/upload/uploadzip", file, handler);
    });
    let bt = document.getElementById("cancelRequest");
    bt.addEventListener("click", () => {
        if (activeRequest === undefined) return;
        fetch("/upload/cancelRequest?requestId=" + activeRequest)
            .then((res) => res.json())
            .then((jsonData) => {
                if (jsonData.status !== "OK") {
                    alert(jsonData.status);
                }
                else {
                    console.log("cancelled...");
                }
            })
            .catch((err) => console.log("cancel error: " + err));
    });
    let cu = document.getElementById("cancelUpload");
    cu.addEventListener("click", () => {
        if (!currentUploadRequest) return;
        currentUploadRequest.abort();
    });
    
    avnavutil.setButtonClick("#shopLogin",(ev)=>{
       tryLogin(); 
    });
    avnavutil.setButtonClick("#shopLogout",(ev)=>{
        shopLogin.logout();
        updateShopStatus();
    });
    avnavutil.setButtonClick("#shopRefresh",(ev)=>{
        getChartList()
        .then((rs)=>identifySystem())
        .then((rs)=>{
            showChartList(shopLogin.chartsXml,false);
            showChartList(shopLogin.chartsXml,true);
            updateShopStatus();
        })
        .catch((e)=>{
            showChartList(undefined,false);
            showChartList(undefined,true);
            updateShopStatus();
            alert("shopError",e);
        })
        getPredefinedSystemName();
    });
    let lastShopLogin=undefined;
    avnavutil.setButtonClick("#shopAssignSystemName",(ev)=>shopAssignSystem("shopSystemName",false));
    avnavutil.setButtonClick("#shopAssignSystemNameDongle",(ev)=>shopAssignSystem("shopSystemNameDongle",true));
    avnavutil.setButtonClick("#shopSite",(ev)=>{
        if (lastShopLogin !== undefined && shopLogin.isLoggedIn() && shopLogin.user != lastShopLogin){
            alert("we cannot login with a new username, do logout/login at the shop");
        }
        if (shopLogin.user !== undefined) lastShopLogin=shopLogin.user;
        avnavutil.forEl("#shopForm input[name=passwd]",(e)=>e.value=shopLogin.password);
        avnavutil.forEl("#shopForm input[name=email]",(e)=>e.value=shopLogin.user);
        avnavutil.forEl("#shopForm",(e)=>e.submit());
        
    });

    fetchStatus();
    updateShopStatus();
    getPredefinedSystemName();
    checkDongle();
    fetchFpr()
    .catch((e)=>alert("unable to get fpr",e));
};
window.setInterval(() => {
    fetch("/upload/requestStatus?requestId=" + activeRequest)
        .then((res) => res.json())
        .then((jsonData) => {
            let status = document.getElementById("requestStatus");
            let rerr = document.getElementById("requestError");
            let rid = document.getElementById("requestId");
            let rprogress = document.getElementById("requestProgress");
            if (jsonData.data === undefined || jsonData.data.id === undefined || jsonData.data.id < 0) {
                status.textContent = "???";
                rerr.textContent = "no active request";
                rid.textContent = "???";
                rprogress.textContent = "---";
            }
            else {
                activeRequest = jsonData.data.id;
                rid.textContent = jsonData.data.id;
                status.textContent = translateState(jsonData.data.state);
                rerr.textContent = jsonData.data.error;
                rprogress.textContent = jsonData.data.progress;
            }
        })
        .catch((err) => console.log("status error: " + err));
}, 1000);
let canInstall=false;
window.setInterval(() => {
    fetch("/upload/canInstall")
        .then((res) => res.json())
        .then((jsonData) => {
            let st = document.getElementById("canInstall");
            if (jsonData.status !== 'OK' || !jsonData.data) {
                st.textContent = "error: " + jsonData.status;
            }
            else {
                st.textContent = jsonData.data.canInstall;
                canInstall=jsonData.data.canInstall;
                avnavutil.setEnabled(".downloadChart",jsonData.data.canInstall);
                avnavutil.setEnabled("#fileInput",jsonData.data.canInstall);
            }
        })
        .catch((err) => console.log("can install error " + err));
}, 1000);

let fetchStatus=()=>{
    fetch("/status")
        .then((res) => res.json())
        .then((jsonData) => {
            let charts = "";
            let sets = jsonData.data.chartManager.chartSets;
            localChartSets=sets;
            for (let k in sets) {
                charts += buildSet(sets[k]);
            }
            let chartList = document.getElementById("chartList");
            chartList.innerHTML = charts;
            showChartList(shopLogin.chartsXml,false);
            showChartList(shopLogin.chartsXml,true);
        })
        .catch((err) => {
            let chartList = document.getElementById("chartList");
            chartList.innerHTML = "";
        })
};
window.setInterval(fetchStatus, 2000);
window.setInterval(checkDongle,5000);