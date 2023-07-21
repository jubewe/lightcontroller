/*
This code is a firmware running on a master ESP32 module that allows the user to control
multiple DMX-Lights (RGB + W + STROBE) using a web interface. It also includes the capability
to communicate with other receiver ESP32 modules to control lights that are not directly connected
to the master ESP32 hosting the web interface.
The firmware facilitates seamless coordination between the master and receiver modules,
enhancing the flexibility and convenience of controlling multiple DMX-Lights.
*/

// ESP-NOW code adapted from https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/
// WebSocket code adapted from https://randomnerdtutorials.com/esp32-websocket-server-arduino/

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDMX.h>
#include <ESPmDNS.h>
#include <esp_now.h>
#include <Preferences.h>

Preferences preferences;

DMXESPSerial dmx;

// Replace with your network credentials
const char *ssid = "lightcontroller";
const char *password = "password";
const int wifi_channel = 1;
const int ssid_hidden = 0;

String config = "";

bool newData = false;

bool effect_flash = false;
bool effect_strobe = false;
int effect_speed = 100; // speed in ms between dmx write commands (only effects)
bool effect_strobe_state = false;
bool effect_flash_state = false;

// Add the MAC-Addresses of your receivers here
uint8_t broadcastAddressReceiver1[] = {0x58, 0xCF, 0x79, 0xB3, 0x90, 0xF8};
uint8_t broadcastAddressReceiver2[] = {0x58, 0xCF, 0x79, 0xB3, 0x92, 0x88};
uint8_t broadcastAddressReceiver3[] = {0x58, 0xCF, 0x79, 0xB3, 0x91, 0xEC};
esp_now_peer_info_t peerInfo1;
esp_now_peer_info_t peerInfo2;
esp_now_peer_info_t peerInfo3;

void initPeers()
{
    memcpy(peerInfo1.peer_addr, broadcastAddressReceiver1, 6);
    peerInfo1.channel = 0;
    peerInfo1.encrypt = false;
    memcpy(peerInfo2.peer_addr, broadcastAddressReceiver2, 6);
    peerInfo2.channel = 0;
    peerInfo2.encrypt = false;
    memcpy(peerInfo3.peer_addr, broadcastAddressReceiver3, 6);
    peerInfo3.channel = 0;
    peerInfo3.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo1) != ESP_OK)
    {
        // Serial.println("Failed to add peer 1");
    }
    if (esp_now_add_peer(&peerInfo2) != ESP_OK)
    {
        // Serial.println("Failed to add peer 2");
    }
    if (esp_now_add_peer(&peerInfo3) != ESP_OK)
    {
        // Serial.println("Failed to add peer 3");
    }
}

struct lightState
{
    int start_address;
    int red;
    int green;
    int blue;
    int white;
    int amber;
    int uv;
};

int fade = 0;

unsigned long previousMillisLED = 0;
unsigned long previousMillisEffect = 0;
int led_intv = 500;
int ledState = LOW;

#define redLEDPin 14
#define greenLEDPin 13
#define blueLEDPin 12

const int maxLights = 512/6;
lightState lightStates[maxLights];
lightState lightStatesOld[maxLights];
lightState DMX_Data;

int addrChanged[maxLights];
int numAddrChanged = 0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    // Serial.print("Last Packet Send Status:");
    // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    if (status == ESP_NOW_SEND_FAIL)
    {
        digitalWrite(redLEDPin, HIGH);
    }
}

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocketClient *globalClient = NULL;

// complete code in /frontend
const char index_html[] PROGMEM = R"rawliteral(<!-- Welcome to the Frontend --><!-- https://github.com/jubewe/lightcontroller --><!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta http-equiv="X-UA-Compatible"content="IE=edge"><meta name="viewport"content="width=device-width,initial-scale=1"><title>Lightcontroller</title></head><body><h3>Lightcontroller</h3><div id="lightcontrolsgroup"></div><div id="lightcontrols"></div></body><footer><h>This Controller was made by</h><a href="https://github.com/jubewe">Julian (Frontend)</a> <a href="https://github.com/jojo-06">Johannes (PCB + Backend)</a> <a href="https://github.com/jubewe/lightcontroller">Documentation (GitHub)</a></footer></html><script>const url=new URL(document.baseURI),splitter="---",presetAddressSplitter="-",presetChannelSplitter=",",presetEntrySplitter=";";let elementsLoaded;const lightConfig="Spot Links Außen Hinten;1;6;Bühne,Links,Bühne Links,Bühne Hinten,Bühne Außen,Bühne Links Außen,Bühne Links Hinten---Spot Links Innen Vorne;31;6;Bühne,Links,Bühne Links,Bühne Vorne,Bühne Innen,Bühne Links Innen,Bühne Links Vorne---Spot Rechts Innen Vorne;37;6;Bühne,Rechts,Bühne Rechts,Bühne Vorne,Bühne Innen,Bühne Rechts Innen,Bühne Rechts Vorne---Spot Rechts Außen Hinten;19;6;Bühne,Rechts,Bühne Rechts,Bühne Hinten,Bühne Außen,Bühne Rechts Außen,Bühne Rechts Hinten---Spot Links Oben;49;6;Spots,Spots Links,Spots Oben,Links---Spot Links Unten;55;6;Spots,Spots Links,Spots Unten,Links---Spot Rechts Oben;61;6;Spots,Spots Rechts,Spots Oben,Rechts---Spot Rechts Unten;67;6;Spots,Spots Rechts,Spots Unten,Rechts";console.debug("Loaded config",lightConfig);const RGBchannels=[["Red","r","#ff0000",0],["Green","g","#00ff00",0],["Blue","b","#0000ff",0],["White","w","#ffffff",255],["Amber","a","#ffbf00",0],["UV","u","#5f4b8b",0]],rangeInputMin=0,rangeInputMax=255,rangeInputDefault=0,fadeRangeMin=0,fadeRangeMax=30,fadeRangeDefault=0,fadeDefaultTime=1,fadeLiveDefaultTime=0,channelsPerRGBLight=6;let groups={},liveUpdate=!1,groupToggle=!0,channelsToggle=!0,lightStates={channels:{},groups:{},master:""},fullOn=0,fullOff=0,quickStates={fullOn:{_all:0},fullOff:{_all:0}},lights=lightConfig;lights.split("---").forEach((e=>{let t=e.split(";")[3];0!==(t?.length??0)&&t.split(",").forEach((t=>{groups[encodeURI(t)]||(groups[encodeURI(t)]=[]),groups[encodeURI(t)].push(e)}))})),lights=lights.split("---").filter((e=>e.length>0)).join("---"),console.debug("Groups",groups);class functions{static request=(e,t)=>new Promise(((n,o)=>{/^http(s)*:\/{2}/i.test(e)||(e=url.origin+e),t=t??{},fetch(e,t).then((e=>{n(e.body)})).catch(o)}));static submitConfig=e=>{this.request("/config",{headers:{plain:"string"!=typeof e?JSON.stringify(e):e},method:"POST"}).then((()=>{console.debug("POST Config Success")})).catch((e=>{console.error("POST Config error",e)}))};static getWhite=e=>"ffffff"===e.replace(/^#/,"").toLowerCase()?255:0;static hexToRGBParsed=e=>functions.hexToRGB(e);static hexToRGB=e=>{let t={};return e.replace(/^#/,"").match(/\w{2}/g).forEach(((e,n)=>t[["r","g","b"][n]]=parseInt(e,16))),t};static RGBToHex=(e,t,n,o)=>{let s="#";return s+=functions.pad2(Math.abs(e).toString(16)),s+=functions.pad2(Math.abs(t).toString(16)),s+=functions.pad2(Math.abs(n).toString(16)),s};static pad2=e=>/^\d$/.test(e)?`${parseInt(e)<10?"0":""}${e}`:`${e.length<2?"0":""}${e}`;static getValuesFromChannels=e=>{let t={};return RGBchannels.forEach((n=>{t[n[1]]=document.getElementById(`${e}-channels-${n[1]}`).value})),t};static appendValuesFromChannels=e=>{let t=functions.getValuesFromChannels(e),[n,o,s]=[t.r,t.g,t.b],l=functions.RGBToHex(n,o,s);return document.getElementById(`${e}-color`).value=l,l};static appendHexToChannels=(e,t)=>{let n=t??document.getElementById(`${e}-color`).value,o=functions.hexToRGBParsed(n),s={};return Object.keys(o).forEach((t=>{document.getElementById(`${e}-channels-${t}`).value=o[t],s[t]=o[t]})),s};static appendToChannels=(e,t)=>{Object.keys(t).forEach((n=>{document.getElementById(`${e}-channels-${n}`).value=t[n]}))};static parseWSUpdateMessage=(e,t,n,o,s,l,a,c,i)=>{let r=[e,...[e,n,o,s,l,a,c,1e3*(i??0)].slice(1,7).map((e=>Math.floor(parseInt(e)*(1-(t??0)/100)))),1e3*i];return[].concat(r,Array(8).slice(0,8-r.length))};static updateLight=(e,t,n,o,s,l,a,c,i)=>{if(0===e.length)return;let r=this.parseWSUpdateMessage(e,t,n,o,s,l,a,c,i);console.debug("WS Update RGB",r.join(";")),ws?.send(r.join(";"))};static parseWSUpdateMessagenoRGB(e,t,n){return[e,Math.floor(255*(1-t/100)),...[...Array(5)].map((e=>0)),1e3*n]}static updateLightnoRGB=(e,t,n)=>{0!==e.length&&(console.debug("WS Update Non-RGB",[e,Math.floor(255*(1-t/100)),...[...Array(5)].map((e=>0)),1e3*n].join(";")),ws?.send(this.parseWSUpdateMessagenoRGB(e,t,n).join(";")))};static updateLightVal=(e,t,n)=>{if(0===e.length)return;let o=t;Array.isArray(o)||(o=[...Array(6)].map((e=>t))),console.debug("WS Update Val",[e,...o,1e3*n].join(";"));ws?.send([e,...o,1e3*n].join(";"))};static quickButtonActive=e=>{e.classList.add("quickActionButton-active")};static quickButtonInactive=e=>{e.classList.remove("quickActionButton-active")};static getQuickState=(e,t)=>quickStates[e]?.[t]??0;static appendHeld=e=>{let t=quickStates[e()];if(!t?.heldActions)return;let n=[],o=[];t.heldActions.reverse().forEach((e=>{let t=e[0],s=e[1];function l(e,t){0===e[0].length||t.filter((t=>t[0].join(";")===e[0].join(";")))[0]||t.push(e)}l(t,n),l(s,o)})),n.forEach((e=>{functions.updateLight(...e)})),o.forEach((e=>{functions.updateLightnoRGB(...e)})),t.heldActions=[]};static disableElem=e=>{e.disabled=!0};static enableElem=e=>{e.removeAttribute("disabled")};static getAddrChanNum=e=>{let t=lights.split("---").filter((t=>t.split(";")[1]===e.toString()));return t[0]?.split(";")[2]};static getLightByAddr=e=>lights.split("---").filter((t=>t.split(";")[1]===e.toString()))[0];static updateGroups=(e,t,n,o)=>{document.getElementById("controlsGroup")&&(document.getElementById("controlsGroup-color").value=n,document.getElementById("controlsGroup-dimm").value=e??0,document.getElementById("controlsGroup-fade").value=t??0,functions.appendToChannels("controlsGroup",o))};static isRGB=e=>"6"===this.getAddrChanNum(e);static appendFromStrings=e=>{let t=e.split(";").map((e=>e.split(","))),n=[...["r","g","b","w","a","u"].map((e=>`channels-${e}`)),"dimm","fade"];t.forEach((e=>function(e){let t,[o,s,l,a,c,i,r,d,u]=e,p=o.split("-");if("string"==typeof o&&!/^\d$/.test(o))if("master"===o.split(":")[0])p=lights.split("---").map((e=>e.split(";")[1])),t="controlsGroupAll";else p=o.split(":").at(-1).split("-"),t="controlsGroup";let h=void 0!==(t??void 0)||functions.isRGB(o),f=t??`controls${o}`,m=n.map((e=>`${f}-${e}`)).map((e=>functions.getElementByID(e)));if(m.forEach(((t,n)=>{t&&(t.value=e[n+1])})),h){functions.appendValuesFromChannels(f);let e=p,t=functions.RGBToHex(s,l,a),n=functions.getValuesFromChannels(f);e.length>0&&(functions.append(e.map((e=>functions.getLightByAddr(e))),d??0,u??0,t,n),functions.append("master",d??0,u??0,t,n,"controlsGroup"),functions.updateGroups(d,u,t,n))}}(e)))};static getElementByID=e=>document.getElementById(e)??void 0;static convertToArray=e=>e?Array.isArray(e)?e:[e]:[];static append=(e,t,n,o,s,l)=>{functions.convertToArray(e).map((e=>{let a=e?.split(";")?.[1]??e,c=l??`controls${a}`;if(!document.getElementById(c))return console.error("could not find controls",a,e);let i=parseInt(e.split(";")[2])>1;document.getElementById(`${c}-dimm`).value=t??0,document.getElementById(`${c}-fade`).value=n??0,i&&(document.getElementById(`${c}-color`).value=o,functions.appendToChannels(c,s))}))};static addKeysToObjectSync=(e,t,n)=>{let o=functions.convertToArray(t),s=e;for(let e=0;e<o.length-1;e++){let t=o[e];t in s||(s[t]={}),s=s[t]}return s[o[o.length-1]]=n,e};static getKeyFromObjectSync=(e,t)=>{let n=functions.convertToArray(t),o=e;for(let e=0;e<n.length;e++){if(!o?.hasOwnProperty(n[e]))return;o=o[n[e]]}return o};static deleteKeyFromObjectSync=(e,t)=>{let n=functions.convertToArray(t),o=e;for(let e=0;e<n.length-1;e++){if(!(n[e]in o))return;o=o[n[e]]}return delete o[n[n.length-1]],e};static localStorage=class{static#e="l";static init=()=>{this.setStorage({presets:{}})};static initIfNonexistent=()=>{this.getStorage()||this.init()};static getStorage=()=>localStorage.getItem(this.#e)?JSON.parse(localStorage.getItem(this.#e)):null;static setStorage=e=>{localStorage.setItem(this.#e,JSON.stringify(e))};static getKey=e=>{let t=this.getStorage();return functions.getKeyFromObjectSync(t,e)};static setKey=(e,t)=>{this.initIfNonexistent();let n=this.getStorage(),o=functions.addKeysToObjectSync(n,e,t);this.setStorage(o)};static deleteKey=e=>{let t=this.getStorage(),n=functions.deleteKeyFromObjectSync(t,e);this.setStorage(n)};static emptyCache=()=>{functions.localStorage.setKey("cache",{})}};static getPresets=()=>this.localStorage.getKey(["presets"])??{};static setPreset=(e,t)=>this.localStorage.setKey(["presets",encodeURI(e)],t);static deletePreset=e=>this.localStorage.deleteKey(["presets",encodeURI(e)]);static getPreset=e=>this.localStorage.getKey(["presets",encodeURI(e)])}class scripts{static loadLights=()=>{lightConfig.split("---").forEach(((e,t)=>{let n=0===e.length,[o,s,l,a]=e.split(";"),c=parseInt(l)>1,i=document.createElement("div");if(n||(i.id=`controls${s}`),i.classList.add("controlsContainer"),n)return i.classList.add("empty"),document.querySelector("#lightcontrols").appendChild(i);let r=document.createElement("div");r.classList.add("controlsContainerTop");let d=document.createElement("div");d.classList.add("controlsContainerBottom");let u=document.createElement("h");u.classList.add("controlsTitle"),n||(u.innerText=o);let p=document.createElement("h");p.classList.add("controlsGroupTitle"),n||(p.innerText=a.split(",").join(", "));let h=elements.controlsColorInput(i.id),f=elements.controlsChannelInputs(i.id),m=elements.controlsDimmerInput(i.id),g=elements.controlsFadeInput(i.id);h.onchange=()=>{E(),functions.appendHexToChannels(i.id)},f.onchange=()=>{E(),functions.appendValuesFromChannels(i.id)},m.onchange=()=>{E()},g.onchange=()=>{E()};let S=document.createElement("button");function E(e){let t=m.querySelector(".controlsDimmerInput").value,n=g.querySelector(".controlsFadeInput").value,o=1===functions.getQuickState("_all","fullOn")||1===functions.getQuickState("_all","fullOff"),l=1===functions.getQuickState("_all","isHeld");if(!c)return lightStates.channels[s]=[s,...Array(6).map((e=>"")),t,n].join(","),l?void quickStates._all.heldActions.push([[],[[s],t,n]]):void(!e&&!liveUpdate||o||functions.updateLightnoRGB(s,t,n));let a=h.querySelector(".controlsColorInput").value,r=(functions.hexToRGBParsed(a),functions.getValuesFromChannels(i.id)),{r:d,b:u,g:p,w:f,a:S,u:E}=r;lightStates.channels[s]=[s,...Object.values({r:d,b:u,g:p,w:f,a:S,u:E}),t,n].join(","),l?quickStates._all.heldActions.push([[[s],t,...Object.values(r),n],[]]):!e&&!liveUpdate||o||functions.updateLight(s,t,...Object.values(r),n)}S.innerText="Update",S.classList.add("controlsSubmit"),S.onclick=()=>{E(!0)},[u,p].map((e=>r.appendChild(e))),[...c?[h,f]:[],m,g,S].forEach((e=>d.appendChild(e))),[r,d].map((e=>i.appendChild(e))),document.querySelector("#lightcontrols").appendChild(i)}))};static loadLightGroups=()=>{let e=document.createElement("div");e.classList.add("groupControlsContainer"),e.id="controlsGroup";let t=document.createElement("h");t.innerText="Group Controls",t.classList.add("controlsTitle");let n=document.createElement("select");n.classList.add("controlsSelector"),Object.keys(groups).forEach((e=>{let t=document.createElement("option");t.innerText=decodeURI(e),t.value=e,n.appendChild(t)}));let o=elements.controlsColorInput(e.id),s=elements.controlsChannelInputs(e.id),l=()=>`_group_${n.value}`,a=elements.controlsQuickActions(e.id,l,d),c=elements.controlsDimmerInput(e.id),i=elements.controlsFadeInput(e.id),r=document.createElement("button");function d(t,s,a,r){let d=o.querySelector(".controlsColorInput").value,u=c.querySelector(".controlsDimmerInput").value,p=i.querySelector(".controlsFadeInput").value,h=n.value;s&&functions.appendHexToChannels(e.id,d),a&&(d=functions.appendValuesFromChannels(e.id));let f=functions.getValuesFromChannels(e.id),m=1===functions.getQuickState(l(),"fullOn")||1===functions.getQuickState(l(),"fullOff"),g=1===functions.getQuickState(l(),"isHeld"),S=groups[h].filter((e=>parseInt(e.split(";")[2])>1)).map((e=>e.split(";")[1])),E=groups[h].filter((e=>1===parseInt(e.split(";")[2]))).map((e=>e.split(";")[1])),{r:b,g:v,b:y,w:C,a:k,u:L}=f;if(lightStates.groups[l()]=[`group:${l()}:${[...S,...E].join("-")}`,...Object.values({r:b,g:v,b:y,w:C,a:k,u:L}),u,p].join(","),g)return quickStates[l()].heldActions.push([[S,u,...Object.values(f),p],[E,u,p]]);r?(functions.updateLightVal(S,r[0],0),functions.updateLightVal(E,r[1],0)):!t&&!liveUpdate||m||(S.length>0&&functions.updateLight(S.join(","),u,...Object.values(f),p),E.length>0&&functions.updateLightnoRGB(E.join(","),u,p)),functions.append(groups[h],u,p,d,f)}r.innerText="Update",r.classList.add("controlsSubmit"),r.onclick=()=>{d(!0)},o.onchange=()=>{d(!1,!0,!1)},s.onchange=()=>{d(!1,!1,!0)},c.onchange=()=>{d()},i.onchange=()=>{d()},[t,n,o,s,a,c,i,r].forEach((t=>e.appendChild(t))),document.querySelector("#lightcontrolsgroup").appendChild(e)};static loadLightAll=()=>{let e=document.createElement("div");e.classList.add("groupControlsContainer"),e.id="controlsGroupAll";let t=document.createElement("h");t.innerText="Master Controls",t.classList.add("controlsTitle");let n=elements.controlsColorInput(e.id),o=elements.controlsChannelInputs(e.id),s=()=>"_all",l=elements.controlsQuickActions(e.id,s,r),a=elements.controlsDimmerInput(e.id),c=elements.controlsFadeInput(e.id),i=document.createElement("button");function r(t,o,s,l){let i=n.querySelector(".controlsColorInput").value,r=a.querySelector(".controlsDimmerInput").value,d=c.querySelector(".controlsFadeInput").value;o&&functions.appendHexToChannels(e.id,i),s&&(i=functions.appendValuesFromChannels(e.id));let u=functions.getValuesFromChannels(e.id),p=1===functions.getQuickState("_all","fullOn")||1===functions.getQuickState("_all","fullOff"),h=1===functions.getQuickState("_all","isHeld"),f=lights.split("---").filter((e=>parseInt(e.split(";")[2])>1)).map((e=>e.split(";")[1])),m=lights.split("---").filter((e=>1===parseInt(e.split(";")[2]))).map((e=>e.split(";")[1])),{r:g,g:S,b:E,w:b,a:v,u:y}=u;if(lightStates.master=[`master:${[...f,...m].join("-")}`,...Object.values({r:g,g:S,b:E,w:b,a:v,u:y}),r,d].join(","),h)return quickStates._all.heldActions.push([[f,r,...Object.values(u),d],[m,r,d]]);l?(functions.updateLightVal(f,l[0],0),functions.updateLightVal(m,l[1],0)):!t&&!liveUpdate||p||(f.length>0&&functions.updateLight(f.join(","),r,...Object.values(u),d),m.length>0&&functions.updateLightnoRGB(m.join(","),r,d)),functions.updateGroups(r,d,i,u),functions.append(lights.split("---"),r,d,i,u)}i.innerText="Update",i.classList.add("controlsSubmit"),i.onclick=()=>{r(!0)},n.onchange=()=>{r(!1,!0,!1)},o.onchange=()=>{r(!1,!1,!0)},a.onchange=()=>{r()},c.onchange=()=>{r()},[t,n,o,l,a,c,i].forEach((t=>e.appendChild(t))),document.querySelector("#lightcontrolsgroup").appendChild(e)};static loadLiveUpdateToggle=()=>{let e=document.createElement("div");e.id="liveUpdateContainer";let t=document.createElement("h");t.id="liveUpdateTitle",t.innerText="Live Update";let n=document.createElement("input");n.type="checkbox",n.onchange=()=>{liveUpdate=n.checked,scripts.appendLiveUpdate()},[t,n].forEach((t=>e.appendChild(t))),document.querySelector("body").appendChild(e)};static appendGroupToggle=()=>{let e=document.createElement("div");e.id="groupToggleContainer";let t=document.createElement("h");t.id="groupToggleTitle",t.innerText="Show Groups";let n=document.createElement("input");n.type="checkbox",n.defaultChecked=!0,n.onchange=()=>{groupToggle=n.checked,scripts.appendGroupToggleUpdate()},[t,n].forEach((t=>e.appendChild(t))),document.querySelector("body").appendChild(e)};static appendChannelsToggle=()=>{let e=document.createElement("div");e.id="channelsToggleContainer";let t=document.createElement("h");t.id="groupToggleTitle",t.innerText="Show Channels";let n=document.createElement("input");n.type="checkbox",n.defaultChecked=!0,n.onchange=()=>{channelsToggle=n.checked,scripts.appendChannelsToggleUpdate()},[t,n].forEach((t=>e.appendChild(t))),document.querySelector("body").appendChild(e)};static appendLiveUpdate=()=>{document.querySelectorAll(".controlsSubmit").forEach((e=>{liveUpdate?e.classList.add("hidden"):e.classList.remove("hidden")})),document.querySelectorAll(".controlsFade").forEach((e=>{e.value=liveUpdate?0:1}))};static appendGroupToggleUpdate=()=>{document.querySelectorAll(".controlsGroupTitle").forEach((e=>{groupToggle?e.classList.remove("hidden"):e.classList.add("hidden")}))};static appendChannelsToggleUpdate=()=>{document.querySelectorAll(".controlsChannelsContainer").forEach((e=>{channelsToggle?e.classList.remove("hidden"):e.classList.add("hidden")}))};static appendPresetSelector=()=>{let e=document.createElement("div");e.id="presetContainer";let t=document.createElement("select");function n(e){if(""===e)return;console.debug("Selected preset",e);let t=functions.getPreset(e);if(!t)return console.error("preset not found by name",e);let n="";if(n+=Object.values(t.channels).join(";"),n+=Object.values(t.groups).join(";"),n+=t.master??Array(9).map((e=>"")).join(","),0===n.length)return console.error("Not appending preset since it's empty");functions.appendFromStrings(n)}t.id="presetSelector",(()=>{let e=document.createElement("option");e.innerText="- Select Preset -",e.value="",e.defaultSelected=!0,e.selected=!0,t.appendChild(e),t.onchange=e=>{n(t.value)}})();let o=document.createElement("button");(()=>{o.innerText="Save",o.onclick=()=>{let e=`preset${Object.keys(functions.getPresets()).length+1}`,t=window.prompt("Preset Name",e)??e;functions.setPreset(t,lightStates),this.appendPresets(t)}})();let s=document.createElement("button");(()=>{s.innerText="Delete",s.onclick=()=>{functions.deletePreset(t.value),this.appendPresets()}})();let l=document.createElement("button");l.innerText="Reload",l.onclick=()=>{n(t.value)};let a=document.createElement("button");(()=>{a.innerText="Rename",a.onclick=()=>{let e=t.value,n=functions.getPreset(e),o=window.prompt("New Preset name",e);0!==o.length&&e!==o&&(functions.deletePreset(e),functions.setPreset(o,n),this.appendPresets(o))}})();let c=document.createElement("button");(()=>{c.innerText="Import",c.onclick=()=>{let e,t=`preset${Object.keys(functions.getPresets()).length+1}`,n=window.prompt("Preset Name",t)??t,o=window.prompt("Preset");if(o){try{e=JSON.parse(o)}catch(e){return console.error("Preset not parseable as JSON")}functions.setPreset(n,e),this.appendPresets(n)}}})();let i=document.createElement("button");i.innerText="Export",i.onclick=()=>{let e=t.value,n=functions.getPreset(e);n&&navigator.clipboard.writeText(JSON.stringify(n))},[o,s,l,a,c,i].forEach((e=>{e.classList.add("presetButton")})),[t,elements.br(),o,s,l,a,elements.br(),c,i].forEach((t=>e.appendChild(t))),document.querySelector("body").appendChild(e),scripts.appendPresets()};static appendRestartRefreshButtons=()=>{let e=document.createElement("div");e.id="restartRefreshContainer";let t=document.createElement("button");t.id="refreshbutton",t.innerText="Refresh",t.onclick=()=>{ws.send("refresh")};let n=document.createElement("button");n.id="restartbutton",n.innerText="Restart",n.onclick=()=>{ws.send("restart")},[t,n].forEach((t=>e.appendChild(t))),document.querySelector("body").appendChild(e)};static appendPresets=e=>{let t=document.getElementById("presetSelector");[...t.childNodes].slice(1).forEach((e=>e.remove()));let n=functions.getPresets()??{};Object.keys(n).forEach((e=>{let n=document.createElement("option");n.innerText=e,n.value=e,t.appendChild(n)})),e&&(t.value=e)};static appendEffect=()=>{let e=document.createElement("div");e.id="effectContainer";let t=document.createElement("button"),n=document.createElement("button");t.innerText="Strobe",n.innerText="Flash";let o,s=[t,n],l=[["strobe_on","effect_off","effectButton-strobe-active"],["flash_on","effect_off","effectButton-flash-active"]];[t,n].forEach((t=>{t.setAttribute("state","0"),e.appendChild(t),t.classList.add("effectButton"),t.onclick=()=>{switch(t.getAttribute("state")){case"0":if(o)return console.error("effect already enabled");t.setAttribute("state","1"),t.classList.add(l[s.indexOf(t)][2]),s.filter((e=>e!==t)).forEach((e=>{e.setAttribute("disabled",!0)})),ws.send(l[s.indexOf(t)][0]),o=t;break;case"1":if(!o)return console.error("effect not enabled");t.setAttribute("state","0"),t.classList.remove(l[s.indexOf(t)][2]),s.filter((e=>e!==t)).forEach((e=>{e.removeAttribute("disabled")})),ws.send(l[s.indexOf(t)][1]),o=void 0}}})),document.querySelector("body").appendChild(e)}}class elements{static br=()=>document.createElement("br");static spacer=()=>document.createElement("spacer");static controlsColorInput=e=>{let t=document.createElement("div");t.classList.add("controlsColorPickerContainer");let n=document.createElement("h");n.innerText="Color Picker";let o=document.createElement("input");return e&&(o.id=`${e}-color`),o.classList.add("controlsColorInput"),o.type="color",o.defaultValue="#000000",[n,o].forEach((e=>t.appendChild(e))),t};static controlsChannelInputs=e=>{let t=document.createElement("div");t.classList.add("controlsChannelsContainer");let n=document.createElement("div"),o=document.createElement("h");o.innerText="Channels";let s=document.createElement("div");e&&(s.id=`${e}-channels`),s.classList.add("controlsChannelsInputContainer");let l=document.createElement("div");return e&&(l.id=`${e}-channel-titles`),l.classList.add("controlsChannelTitlesContainer"),RGBchannels.forEach((t=>{let n=document.createElement("input");e&&(n.id=`${s.id}-${t[1]}`),n.classList.add("controlsChannelInput",`controlsChannel-${t[1]}`,"vertical"),n.type="range",n.style.backgroundColor=t[2],n.min=0,n.max=255,n.defaultValue=t[3];let o=document.createElement("h");o.classList.add("controlsChannelTitle"),o.title=t[0],o.innerText=t[1],s.appendChild(n),l.appendChild(o)})),[o,s,l].forEach((e=>n.appendChild(e))),t.appendChild(n),t};static controlsDimmerInput=e=>{let t=document.createElement("div");t.classList.add("controlsDimmerContainer");let n=document.createElement("div"),o=document.createElement("h");o.innerText="Dimmer";let s=document.createElement("input");return e&&(s.id=`${e}-dimm`),s.classList.add("dimmer"),s.classList.add("controlsDimmerInput","horizontal"),s.type="range",s.min=0,s.max=100,s.defaultValue=0,[o,s].forEach((e=>n.appendChild(e))),t.appendChild(n),t};static controlsFadeInput=e=>{let t=document.createElement("div");t.classList.add("controlsFadeContainer");let n=document.createElement("div"),o=document.createElement("h");o.innerText="Fade";let s=document.createElement("input");return s.id=`${e}-fade`,s.classList.add("controlsFadeInput","horizontal"),s.type="range",s.min=0,s.max=30,s.defaultValue=0,[o,s].forEach((e=>n.appendChild(e))),t.appendChild(n),t};static controlsQuickActions=(e,t,n)=>{let o=()=>t?.()??"_all",s=document.createElement("div");s.classList.add("controlsQuickActionsContainer");let l=document.createElement("button"),a=document.createElement("button"),c=document.createElement("button");function i(e,t){switch(e){case"fullOn":quickStates[o()].fullOn=t,quickStates[o()].fullOff=2;break;case"fullOff":quickStates[o()].fullOff=t,quickStates[o()].fullOn=2;break;case"isHeld":quickStates[o()].isHeld=t}}function r(e){quickStates[o()]||(quickStates[o()]={fullOn:0,fullOff:0,isHeld:0,heldActions:[]});let s=["fullOn","fullOff","isHeld"],r=[l,a,c],d=r.indexOf(e.target),u=[...r].filter((t=>t!==e.target)),p=r[d],h=s[d],f=functions.getQuickState(o(),h);switch(f){case 0:case 2:switch(functions.quickButtonActive(p),i(h,1),s[d]){case"fullOn":case"fullOff":n(!1,!1,!1,[[0,0,0,[255,0][d],0,0],[[255,0][d],0,0,0,0,0]])}break;case 1:if(functions.quickButtonInactive(p),i(h,0),"isHeld"===s[d])functions.appendHeld(t)}switch(s[r.indexOf(p)]){case"fullOn":case"fullOff":return void u.filter((e=>e!==c)).forEach((e=>{functions.quickButtonInactive(e)}));case"isHeld":return void u.forEach((e=>{[functions.disableElem,functions.enableElem][f]?.(e),functions.quickButtonInactive(e),i(s[r.indexOf(e)],2)}))}}return l.innerText="Full On",a.innerText="Full Off",c.innerText="Hold",[l,a,c].map((e=>{e.classList.add("quickActionButton"),s.appendChild(e),e.onclick=r})),s}}elementsLoaded=new Promise((e=>{scripts.loadLights(),scripts.loadLightGroups(),scripts.loadLightAll(),scripts.loadLiveUpdateToggle(),scripts.appendGroupToggle(),scripts.appendChannelsToggle(),scripts.appendPresetSelector(),scripts.appendRestartRefreshButtons(),scripts.appendEffect(),e(),elementsLoaded=void 0}));const ws=new WebSocket(`ws://${window.location.hostname}/ws`);try{function handleWSMessage(e){let t=e.data;t.startsWith("refresh")&&(functions.appendFromStrings(t.replace(/^refresh;/,"")),console.debug("WS: Refresh appended"))}ws.onopen=()=>{console.log("WS Opened")},ws.onmessage=async e=>{elementsLoaded&&await elementsLoaded,handleWSMessage(e)},ws.onerror=e=>{console.error("WS Error",e)},ws.onclose=()=>{window.alert("WebSocket connection lost - further updates will not be sent\n\nTo reconnect, make sure the lightcontroller is turned on and you are connected via WiFi")}}catch(e){console.error(e)}</script><style>*{scrollbar-width:thin;font-family:"Trebuchet MS","Lucida Sans Unicode","Lucida Grande","Lucida Sans",Arial,sans-serif;border-radius:10px;text-align:center;user-select:none;padding:3px}.hidden{display:none!important}button,input[type=checkbox]{cursor:pointer}@media screen and (-webkit-min-device-pixel-ratio:0){.controlsChannelInput{overflow:hidden;appearance:none;-webkit-appearance:none;background-color:#fff;border:1px solid}.controlsChannelInput::-webkit-slider-runnable-track{height:10px;appearance:none;-webkit-appearance:none;color:#fff;background-color:transparent;margin-top:-1px}.controlsChannelInput::-webkit-slider-thumb{width:10px;-webkit-appearance:none;height:10px;border:1px solid #fff;border-radius:100%;background:0 0;box-shadow:396px 0 2px 400px #000}}.horizontal{cursor:w-resize}.vertical{cursor:n-resize}.controlsChannelInput{border:1px solid #fff;background-color:#000}.controlsChannelInput::-moz-range-progress{background-color:#fcca46}.controlsChannelInput::-moz-range-track{background-color:#000}body{background-color:#233d4d;transition:ease-in 2s;color:#f1faee;padding:3px}#lightcontrols,#lightcontrolsgroup{display:flex;place-items:center;flex-wrap:wrap;justify-content:center;align-items:flex-start}#lightcontrolsgroup{align-items:stretch}.controlsContainer{display:flex;flex:1 0 24%}.controlsContainer,.groupControlsContainer{max-width:23%;padding:6px;margin:2px;width:25%;gap:3px;border:1px solid;background-color:#1d2d44;display:flex;flex-direction:column;align-items:center;justify-content:space-evenly}.groupControlsContainer{max-width:none;width:40%}spacer{display:flex;height:10px}.controlsTitle{font-size:large;height:50%}.controlsDimmerInput,.controlsFadeInput{width:80%}.controlsColorInput,.controlsSubmit{border:none;outline:0}.controlsSelector{border:1px solid transparent}.controlsColorInput{background-color:transparent;box-sizing:content-box;padding:0;height:30px}.controlsSubmit{padding:4px}.controlsGroupTitle{font-size:small;overflow-y:auto;height:50%}.controlsChannelsContainer,.controlsColorPickerContainer,.controlsDimmerContainer,.controlsFadeContainer,.controlsQuickActionsContainer{display:flex;flex-wrap:wrap;justify-content:center;align-items:center;border:1px solid;flex-direction:column;width:100%}.controlsDimmerContainer>div,.controlsFadeContainer>div{display:flex;flex-direction:column;justify-content:center;align-items:center;width:90%}.controlsQuickActionsContainer{flex-direction:row}@media screen and (max-width:700px){.controlsChannelsContainer{display:none}}.controlsChannelsContainer{flex-direction:column;flex-wrap:nowrap}.controlsChannelsInputContainer{display:flex;flex-wrap:wrap;justify-content:center;flex-direction:column;transform:rotate(-90deg);row-gap:1px;justify-content:center;aspect-ratio:1/1.2}.controlsChannelTitle{cursor:help}.controlsChannelTitlesContainer{display:flex;justify-content:space-evenly}.controlsColorPickerContainer{flex-direction:column;justify-content:center}#liveUpdateContainer{background-color:#38a3a5;z-index:10;position:absolute;left:10px;top:40px}#groupToggleContainer{background-color:#38a3a5;z-index:10;position:absolute;left:10px;top:10px}#presetContainer{z-index:10;position:absolute;right:10px;top:5px}#presetContainer select{width:100%}#presetContainer button{border:transparent;margin:2px}#presetContainer select{border:transparent;background-color:#38a3a5}#channelsToggleContainer{background-color:#38a3a5;z-index:10;position:absolute;left:150px;top:10px}.effectButton,.quickActionButton{display:flex;border-radius:6px;border:1px solid #f1faee;margin:2px;background-color:#220901;color:#f1faee}.effectButton:disabled,.quickActionButton:disabled{color:grey}@keyframes blink{0%,49%{background-color:red;color:#000}50%,99%{background-color:#220901;color:red}}.quickActionButton-active{background-color:#a1c181;animation:blink 1s infinite}.controlsContainerBottom,.controlsContainerTop{display:flex;flex-direction:column;align-items:center;margin:0;padding:0;width:100%;gap:2px}.controlsContainerTop{height:45%}.controlsContainerTop{height:55%}.empty{background:0 0;border:transparent}#restartRefreshContainer{position:absolute;top:5px;left:0;right:0}#restartRefreshContainer button{border:transparent;margin:3px;background-color:red}#effectContainer{display:flex;position:absolute;flex-wrap:nowrap;top:5px;left:65vw}@keyframes effectStrobeActive{0%,30%{background-color:red;color:#000}33%,63%{background-color:#0f0;color:#000}100%,66%{background-color:#00f;color:#fff}}@keyframes effectFlashActive{0%{background-color:#000;color:#fff}100%{background-color:#fff;color:#000}}.effectButton-strobe-active{animation:effectStrobeActive 1s infinite}.effectButton-flash-active{animation:effectFlashActive .2s infinite}</style>)rawliteral";

void notifyClientsRefresh()
{
    String states;
    for (int i; i < maxLights; i++)
    {
        states = states + ";" + lightStates[i].start_address + "," + lightStates[i].red + "," + lightStates[i].green + "," + lightStates[i].blue + "," + lightStates[i].white + "," + lightStates[i].amber + "," + lightStates[i].uv;
    }
    String msg = "refresh" + states;
    // ws.textAll(String(ledState));
    // Serial.println(msg);
    if (globalClient != NULL && globalClient->status() == WS_CONNECTED)
    {
        globalClient->text(msg);
    }
}

void splitStringToIntegers(const String &data, int addr[], int &r, int &g, int &b, int &w, int &am, int &uv)
{
    // Temporary variables to store intermediate values
    String tempAddr, tempR, tempG, tempB, tempW, tempAM, tempUV;

    // Serial.println("data " + data);

    // Find the position of semicolons in the string
    int semicolon1 = data.indexOf(';');
    int semicolon2 = data.indexOf(';', semicolon1 + 1);
    int semicolon3 = data.indexOf(';', semicolon2 + 1);
    int semicolon4 = data.indexOf(';', semicolon3 + 1);
    int semicolon5 = data.indexOf(';', semicolon4 + 1);
    int semicolon6 = data.indexOf(';', semicolon5 + 1);
    int semicolon7 = data.indexOf(';', semicolon6 + 1);

    // Extract individual substrings between semicolons
    tempAddr = data.substring(0, semicolon1) + ",";
    tempR = data.substring(semicolon1 + 1, semicolon2);
    tempG = data.substring(semicolon2 + 1, semicolon3);
    tempB = data.substring(semicolon3 + 1, semicolon4);
    tempW = data.substring(semicolon4 + 1, semicolon5);
    tempAM = data.substring(semicolon5 + 1, semicolon6);
    tempUV = data.substring(semicolon6 + 1, semicolon7);
    fade = data.substring(semicolon7 + 1).toInt();

    // Convert the substrings to integers
    r = tempR.toInt();
    g = tempG.toInt();
    b = tempB.toInt();
    w = tempW.toInt();
    am = tempAM.toInt();
    uv = tempUV.toInt();

    int startIndex = 0;
    int endIndex = 0;
    int length = tempAddr.length();
    // Serial.println("tempAddr " + tempAddr);
    for (int i = 0; i < maxLights; i++)
    {
        addrChanged[i] = 0;
    }
    numAddrChanged = 0;
    for (int j = 0; j < length; j++)
    {
        if (tempAddr.charAt(j) == ',' || j == length)
        {
            String numberString = tempAddr.substring(startIndex, endIndex + 1);
            int addr = numberString.toInt();

            addrChanged[numAddrChanged] = addr;
            // Serial.println("addr changed:" + String(addr));
            numAddrChanged++;

            int index = -1;

            for (int i = 0; i < maxLights; i++)
            {
                if (lightStates[i].start_address == addr)
                {
                    index = i; // Found a match, store the index
                    break;
                }
                if (lightStates[i].start_address == 0)
                {
                    index = i; // empty, store the index
                    lightStates[index].start_address = addr;
                    break;
                }
            }
            // Serial.println("updating addr: " + String(addr));
            lightStates[index].red = r;
            lightStates[index].green = g;
            lightStates[index].blue = b;
            lightStates[index].white = w;
            lightStates[index].amber = am;
            lightStates[index].uv = uv;

            startIndex = j + 1;
        }

        else
        {
            endIndex = j;
        }
    }
}

void esp_now_send_DMX()
{
    esp_err_t result1 = esp_now_send(broadcastAddressReceiver1, (uint8_t *)&DMX_Data, sizeof(DMX_Data));
    esp_err_t result2 = esp_now_send(broadcastAddressReceiver2, (uint8_t *)&DMX_Data, sizeof(DMX_Data));
    esp_err_t result3 = esp_now_send(broadcastAddressReceiver3, (uint8_t *)&DMX_Data, sizeof(DMX_Data));

    // Serial.print("sending using ESP_NOW... ");
}

void outputDMX()
{
    for (int i = 0; i < maxLights; i++)
    {
        if (lightStates[i].start_address != 0)
        {
            int addr = lightStates[i].start_address;
            int r = lightStates[i].red;
            int g = lightStates[i].green;
            int b = lightStates[i].blue;
            int w = lightStates[i].white;
            int am = lightStates[i].amber;
            int uv = lightStates[i].uv;

            if (fade == 0 || addr != addrChanged[0])
            {
                // Serial.println(String(addr) + "not changed");
                dmx.write(addr, r);
                dmx.write(addr + 1, g);
                dmx.write(addr + 2, b);
                dmx.write(addr + 3, w);
                dmx.write(addr + 4, am);
                dmx.write(addr + 5, uv);
                dmx.update();

                DMX_Data.start_address = addr;
                DMX_Data.red = r;
                DMX_Data.green = g;
                DMX_Data.blue = b;
                DMX_Data.white = w;
                DMX_Data.amber = am;
                DMX_Data.uv = uv;
                esp_now_send_DMX();
            }
            else if (addr == addrChanged[0])
            {
                // Serial.println(String(addr) + "-" + String(addrChanged[numAddrChanged - 1]) + "fading");
                int rampDuration = fade;
                unsigned long startTime = millis();               // Get the current time
                unsigned long endTime = startTime + rampDuration; // Calculate the end time

                int r_old = lightStatesOld[i].red;
                int g_old = lightStatesOld[i].green;
                int b_old = lightStatesOld[i].blue;
                int w_old = lightStatesOld[i].white;
                int am_old = lightStatesOld[i].amber;
                int uv_old = lightStatesOld[i].uv;

                while (millis() <= endTime)
                {
                    // Calculate the progress of the ramp
                    float progress = float(millis() - startTime) / rampDuration;
                    // Serial.println("progress: " + String(progress));
                    // Serial.println("red: " + String(r_old + (r - r_old) * progress));

                    for (int j = 0; j < numAddrChanged; j++)
                    {
                        // Calculate the current brightness value
                        int red = r_old + (r - r_old) * progress;
                        int green = g_old + (g - g_old) * progress;
                        int blue = b_old + (b - b_old) * progress;
                        int white = w_old + (w - w_old) * progress;
                        int am = am_old + (am - am_old) * progress;
                        int uv = uv_old + (uv - uv_old) * progress;
                        // Serial.println("test0");
                        dmx.write(addrChanged[j], red);
                        dmx.write(addrChanged[j] + 1, green);
                        dmx.write(addrChanged[j] + 2, blue);
                        dmx.write(addrChanged[j] + 3, white);
                        dmx.write(addrChanged[j] + 4, am);
                        dmx.write(addrChanged[j] + 5, uv);
                        // Serial.println("test1");
                        vTaskDelay(1);
                        DMX_Data.start_address = addrChanged[j];
                        DMX_Data.red = red;
                        DMX_Data.green = green;
                        DMX_Data.blue = blue;
                        DMX_Data.white = white;
                        DMX_Data.amber = am;
                        DMX_Data.uv = uv;
                        esp_now_send_DMX();
                    }
                    dmx.update();

                    // Serial.println("test2");
                }
            }
            lightStatesOld[i].red = r;
            lightStatesOld[i].green = g;
            lightStatesOld[i].blue = b;
            lightStatesOld[i].white = w;
            lightStatesOld[i].amber = am;
            lightStatesOld[i].uv = uv;
        }
    }
    //notifyClientsRefresh();
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        // Serial.println((char *)data);

        // [addr, dimm, r, g, b, w, str]

        // Temporary variables to store intermediate values
        String data_str = (char *)data;

        // Find the position of semicolons in the string
        int semicolon1 = data_str.indexOf(';');

        // Extract individual substrings between semicolons
        String command = data_str.substring(0, semicolon1);

        if (command == "refresh")
        {
            notifyClientsRefresh();
        }
        else if (command == "restart")
        {
            // Serial.println("restarting");
            if (globalClient != NULL && globalClient->status() == WS_CONNECTED)
            {
                globalClient->text("restarting");
            }
            ESP.restart();
        }
        else if (command == "strobe_on")
        {
            // Serial.println("strobe effect activated");
            effect_strobe = true;
        }
        else if (command == "flash_on")
        {
            // Serial.println("flash effect activated");
            effect_flash = true;
        }
        else if (command == "effect_off")
        {
            // Serial.println("all effects disabled");
            effect_strobe = false;
            effect_flash = false;
        }
        else
        {
            int addr[3]; // number of different addresses
            int dimm, r, g, b, w, am, uv;
            int address_size;

            splitStringToIntegers((char *)data, addr, r, g, b, w, am, uv);
            newData = true;
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        // Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        led_intv = 100;
        globalClient = client;
        notifyClientsRefresh();
        break;
    case WS_EVT_DISCONNECT:
        // Serial.printf("WebSocket client #%u disconnected\n", client->id());
        led_intv = 500;
        globalClient = NULL;
        break;
    case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}

void initWebSocket()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}

String processor(const String &var)
{
    // Serial.println(var);

    if (var == "CONFIG")
        return "";
    return String();
}

void setup()
{
    // initialize dmx
    dmx.init(512);
    // debugging
    while (false)
    {
        for (int i = 0; i < maxLights; i++)
        {
            if (lightStates[i].start_address != 0)
            {
                int addr = lightStates[i].start_address;
                dmx.write(addr, 0);
                dmx.write(addr + 1, 0);
                dmx.write(addr + 2, 0);
                dmx.write(addr + 4, 0);
                dmx.write(addr + 5, 0);

                DMX_Data.start_address = addr;
                DMX_Data.red = 0;
                DMX_Data.green = 0;
                DMX_Data.blue = 0;
                DMX_Data.amber = 0;
                DMX_Data.uv = 0;
                if (effect_flash_state)
                {
                    dmx.write(addr + 3, 255);
                    DMX_Data.white = 255;
                    delay(100);
                }
                else
                {
                    dmx.write(addr + 3, 0);
                    DMX_Data.white = 0;
                    delay(50);
                }
                effect_flash_state = !effect_flash_state;

                dmx.update();

                esp_now_send_DMX();
            }
        }
        /*dmx.write(1, random(0, 255));
        dmx.write(2, random(0, 255));
        dmx.write(3, random(0, 255));
        dmx.write(4, random(0, 255));
        dmx.write(5, random(0, 255));
        dmx.write(6, random(0, 255));
        dmx.write(7, random(0, 255));
        dmx.write(8, random(0, 255));
        dmx.update();
        */
    }
    outputDMX();
    pinMode(redLEDPin, OUTPUT);
    pinMode(greenLEDPin, OUTPUT);
    pinMode(blueLEDPin, OUTPUT);
    digitalWrite(redLEDPin, HIGH);
    delay(2000);
    digitalWrite(redLEDPin, LOW);

    preferences.begin("config", false);

    config = preferences.getString("config", config);

    // Serial port for debugging purposes
    // Serial.begin(115200);
    delay(500);
    // Serial.println("Setting up WiFi");

    WiFi.mode(WIFI_AP_STA);

    // Remove the password parameter, if you want the AP (Access Point) to be open
    WiFi.softAP(ssid, password, wifi_channel, ssid_hidden);

    IPAddress IP = WiFi.softAPIP();
    // Serial.print("AP IP address: ");
    // Serial.println(IP);
    // Serial.print("MAC address: ");
    // Serial.println(WiFi.macAddress());
    initWebSocket();

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", index_html, processor); });

    server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    if (request->hasArg("plain")) {
      String data = request->arg("plain");
      //Serial.println("Received new config: " + data);
      preferences.putString("config", data);
    }
    request->send(200); });

    // Start server
    server.begin();

    if (esp_now_init() != ESP_OK)
    {
        // Serial.println("Error initializing ESP-NOW");
    }
    esp_now_register_send_cb(OnDataSent);

    initPeers();
}

void loop()
{
    ws.cleanupClients();
    unsigned long currentMillis = millis();
    if (newData == true)
    {
        digitalWrite(blueLEDPin, HIGH);
        outputDMX();
        digitalWrite(blueLEDPin, LOW);
        digitalWrite(redLEDPin, LOW);

        newData = false;
    }
    if (currentMillis - previousMillisLED >= led_intv)
    {
        previousMillisLED = currentMillis;
        if (ledState == LOW)
        {
            ledState = HIGH;
        }
        else
        {
            ledState = LOW;
        }
        digitalWrite(greenLEDPin, ledState);
    }
    if (currentMillis - previousMillisEffect >= 100)
    {
        if (effect_strobe == true)
        {
            for (int i = 0; i < maxLights; i++)
            {
                if (lightStates[i].start_address != 0)
                {
                    int r;
                    int g;
                    int b;
                    int am;
                    int uv;
                    if (effect_strobe_state == 0)
                    {
                        int r_change = random(0, 2);  // Random value to determine if r should be at full brightness
                        int g_change = random(0, 2);  // Random value to determine if g should be at full brightness
                        int b_change = random(0, 2);  // Random value to determine if b should be at full brightness
                        int am_change = random(0, 2); // Random value to determine if am should be at full brightness
                        int uv_change = random(0, 2); // Random value to determine if uv should be at full brightness

                        r = (r_change == 1) ? random(0, 256) : 0;
                        g = (g_change == 1) ? random(0, 256) : 0;
                        b = (b_change == 1) ? random(0, 256) : 0;
                        am = (am_change == 1) ? random(0, 256) : 0;
                        uv = (uv_change == 1) ? random(0, 256) : 0;
                    }

                    effect_strobe_state = !effect_strobe_state;

                    int addr = lightStates[i].start_address;
                    dmx.write(addr, r);
                    dmx.write(addr + 1, g);
                    dmx.write(addr + 2, b);
                    dmx.write(addr + 3, 0);
                    dmx.write(addr + 4, am);
                    dmx.write(addr + 5, uv);
                    dmx.update();

                    DMX_Data.start_address = addr;
                    DMX_Data.red = r;
                    DMX_Data.green = g;
                    DMX_Data.blue = b;
                    DMX_Data.white = 0;
                    DMX_Data.amber = am;
                    DMX_Data.uv = uv;
                    esp_now_send_DMX();
                }
            }
        }

        else if (effect_flash == true)
        {
            for (int i = 0; i < 50; i++)
            {
                if (lightStates[i].start_address != 0)
                {
                    int addr = lightStates[i].start_address;
                    dmx.write(addr, 0);
                    dmx.write(addr + 1, 0);
                    dmx.write(addr + 2, 0);
                    dmx.write(addr + 4, 0);
                    dmx.write(addr + 5, 0);

                    DMX_Data.start_address = addr;
                    DMX_Data.red = 0;
                    DMX_Data.green = 0;
                    DMX_Data.blue = 0;
                    DMX_Data.amber = 0;
                    DMX_Data.uv = 0;
                    if (effect_flash_state)
                    {
                        dmx.write(addr + 3, 255);
                        DMX_Data.white = 255;
                        delay(100);
                    }
                    else
                    {
                        dmx.write(addr + 3, 0);
                        DMX_Data.white = 0;
                        delay(50);
                    }
                    effect_flash_state = !effect_flash_state;

                    dmx.update();

                    esp_now_send_DMX();

                }
            }
        }
    }
}
