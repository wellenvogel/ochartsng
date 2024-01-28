window.avnavutil={
 /**
     * @param url {string}
     * @param file {File}
     * @param param parameter object
     *        all handlers get the param object as first parameter
     *        starthandler: will get the xhdr as second parameter - so it can be used for interrupts
     *        progresshandler: progressfunction
     *        okhandler: called when done
     *        errorhandler: called on error
     *        see https://mobiarch.wordpress.com/2012/08/21/html5-file-upload-with-progress-bar-using-jquery/
     */
  uploadFile: (url, file, param)=> {
    let type = "application/octet-stream";
    try {
        let xhr=new XMLHttpRequest();
        xhr.open('POST',url,true);
        xhr.setRequestHeader('Content-Type', type);
        xhr.addEventListener('load',(event)=>{
            if (xhr.status != 200){
                if (param.errorhandler) param.errorhandler(xhr.statusText);
                return;
            }
            let json=undefined;
            try {
                json = JSON.parse(xhr.responseText);
                if (! json.status || json.status != 'OK'){
                    if (param.errorhandler) param.errorhandler(json.info);
                    return;
                }
            }catch (e){
                if (param.errorhandler) param.errorhandler(e);
                return;
            }
            if (param.okhandler) param.okhandler(json);
        });
        xhr.upload.addEventListener('progress',(event)=>{
            if (param.progresshandler) param.progresshandler(event);
        });
        xhr.addEventListener('error',(event)=>{
            if (param.errorhandler) param.errorhandler("upload error");
        });
        if (param.starthandler) param.starthandler(xhr);
        xhr.send(file);
    } catch (e) {
        if (param.errorhandler) param.errorhandler(e);
    }
},
    addEl: (type, clazz, parent, text)=>{
    let el = document.createElement(type);
    if (clazz) {
        if (!(clazz instanceof Array)) {
            clazz = clazz.split(/  */);
        }
        clazz.forEach(function (ce) {
            el.classList.add(ce);
        });
    }
    if (text) el.textContent = text;
    if (parent) parent.appendChild(el);
    return el;
},
forEl:(query, callback,base)=>{
    if (! base) base=document;
    let all = base.querySelectorAll(query);
    for (let i = 0; i < all.length; i++) {
        callback(all[i]);
    }
},
getValue:(query,base)=>{
    let v=undefined;
    avnavutil.forEl(query,(el)=>{v=el.value;},base)
    return v;
    },  
setEnabled:(query,enabled,base)=>{
    avnavutil.forEl(query,(el)=>el.disabled=!enabled,base);
},
setHidden:(query,hidden,base)=>{
    avnavutil.forEl(query,(el)=>{
        if (hidden) el.classList.add("hidden");
        else el.classList.remove("hidden");
    });
},
setButtonClick:(query,click,base)=>{
    avnavutil.forEl(query,(el)=>{
        el.addEventListener("click",click);
    },base);
},
getXmlVal:(xml,name)=>{
    let parts=name.split(".");
    for (let p in parts){
        let np=parts[p];
        xml=xml.getElementsByTagName(np)[0];
        if (!xml) return;
    }
    if (xml) return xml.textContent;
}
};

