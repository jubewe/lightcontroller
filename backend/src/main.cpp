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

// Add the MAC-Addresses of your receivers here
uint8_t broadcastAddressReceiver1[] = {0x58, 0xCF, 0x79, 0xB3, 0x90, 0xF8};
uint8_t broadcastAddressReceiver2[] = {0x58, 0xCF, 0x79, 0xB3, 0xA5, 0xBA};

esp_now_peer_info_t peerInfo1;
esp_now_peer_info_t peerInfo2;

void initPeers()
{
    memcpy(peerInfo1.peer_addr, broadcastAddressReceiver1, 6);
    peerInfo1.channel = 0;
    peerInfo1.encrypt = false;
    memcpy(peerInfo2.peer_addr, broadcastAddressReceiver2, 6);
    peerInfo2.channel = 0;
    peerInfo2.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo1) != ESP_OK)
    {
        Serial.println("Failed to add peer 1");
    }
    if (esp_now_add_peer(&peerInfo2) != ESP_OK)
    {
        Serial.println("Failed to add peer 2");
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

unsigned long previousMillis = 0;
int led_intv = 500;
int ledState = LOW;

#define redLEDPin 14
#define greenLEDPin 13
#define blueLEDPin 12

const int maxLights = 85; // 512/6
lightState lightStates[maxLights];
lightState lightStatesOld[maxLights];
lightState DMX_Data;

int addrChanged[maxLights];
int numAddrChanged = 0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
    if (status == ESP_NOW_SEND_FAIL)
    {
        digitalWrite(redLEDPin, HIGH);
    }
}

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// complete code in /frontend
const char index_html[] PROGMEM = R"rawliteral(<!-- Welcome to the Frontend --><!-- https://github.com/jubewe/lightcontroller --><!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta http-equiv="X-UA-Compatible"content="IE=edge"><meta name="viewport"content="width=device-width,initial-scale=1"><title>Lightcontroller</title></head><body><h3>Lightcontroller</h3><div id="lightcontrols"></div><div id="lightcontrolsgroup"></div></body><footer><h>This Controller was made by</h><a href="https://github.com/jubewe">Julian (Frontend)</a> <a href="https://github.com/jojo-06">Johannes (PCB + Backend)</a> <a href="https://github.com/jubewe/lightcontroller">Documentation (GitHub)</a></footer></html><script>const url=new URL(document.baseURI),splitter="---",lights="Bühne Links Außen Hinten;1;1;Bühne,Links,Bühne Links,Bühne Hinten,Bühne Außen,Bühne Links Außen,Bühne Links Hinten---Bühne Links Innen Hinten;7;1;Bühne,Links,Bühne Links,Bühne Hinten,Bühne Innen,Bühne Links Innen,Bühne Links Hinten---Bühne Rechts Innen Hinten;13;1;Bühne,Rechts,Bühne Rechts,Bühne Hinten,Bühne Innen,Bühne Rechts Innen,Bühne Rechts Hinten---Bühne Rechts Außen Hinten;19;1;Bühne,Rechts,Bühne Rechts,Bühne Hinten,Bühne Außen,Bühne Rechts Außen,Bühne Rechts Hinten---Bühne Links Außen Vorne;25;1;Bühne,Links,Bühne Links,Bühne Vorne,Bühne Außen,Bühne Links Außen,Bühne Links Vorne---Bühne Links Innen Vorne;31;1;Bühne,Links,Bühne Links,Bühne Vorne,Bühne Innen,Bühne Links Innen,Bühne Links Vorne---Bühne Rechts Innen Vorne;37;1;Bühne,Rechts,Bühne Rechts,Bühne Vorne,Bühne Innen,Bühne Rechts Innen,Bühne Rechts Vorne---Bühne Rechts Außen Vorne;43;1;Bühne,Rechts,Bühne Rechts,Bühne Vorne,Bühne Außen,Bühne Rechts Außen,Bühne Rechts Vorne---Spot Links Oben;49;6;Spots,Spots Links,Spots Oben,Links---Spot Links Unten;55;6;Spots,Spots Links,Spots Unten,Links---Spot Rechts Oben;61;6;Spots,Spots Rechts,Spots Oben,Rechts---Spot Rechts Unten;67;6;Spots,Spots Rechts,Spots Unten,Rechts";console.debug("Loaded config",lights);const channels=[["Red","r","#ff0000",0],["Green","g","#00ff00",0],["Blue","b","#0000ff",0],["White","w","#ffffff",255],["Amber","a","#ffbf00",0],["UV","u","#5f4b8b",0]],rangeInputMin=0,rangeInputMax=255,rangeInputDefault=0,fadeRangeMin=0,fadeRangeMax=30,fadeRangeDefault=0,fadeDefaultTime=1,fadeLiveDefaultTime=0,channelsPerRGBLight=6;let groups={},liveUpdate=!1,groupToggle=!0,fullOn=0,fullOff=0,quickStates={fullOn:{_all:0},fullOff:{_all:0}};lights.split("---").forEach((e=>{let t=e.split(";")[3];0!==t.length&&t.split(",").forEach((t=>{groups[encodeURI(t)]||(groups[encodeURI(t)]=[]),groups[encodeURI(t)].push(e)}))})),console.debug("Groups",groups);const ws=new WebSocket(`ws://${window.location.hostname}/ws`);try{ws.onopen=()=>{console.log("WS Opened")},ws.onmessage=e=>{console.log("WS Message",e)},ws.onerror=e=>{console.error("WS Error",e)}}catch(e){console.error(e)}class functions{static request=(e,t)=>new Promise(((n,l)=>{/^http(s)*:\/{2}/i.test(e)||(e=url.origin+e),t=t??{},fetch(e,t).then((e=>{n(e.body)})).catch(l)}));static submitConfig=e=>{this.request("/config",{headers:{plain:"string"!=typeof e?JSON.stringify(e):e},method:"POST"}).then((()=>{console.debug("POST Config Success")})).catch((e=>{console.error("POST Config error",e)}))};static getWhite=e=>"ffffff"===e.replace(/^#/,"").toLowerCase()?255:0;static hexToRGBParsed=e=>functions.hexToRGB(e);static hexToRGB=e=>{let t={};return e.replace(/^#/,"").match(/\w{2}/g).forEach(((e,n)=>t[["r","g","b"][n]]=parseInt(e,16))),t};static RGBToHex=(e,t,n,l)=>{let o="#";return o+=functions.pad2(Math.abs(e).toString(16)),o+=functions.pad2(Math.abs(t).toString(16)),o+=functions.pad2(Math.abs(n).toString(16)),o};static pad2=e=>/^\d$/.test(e)?`${parseInt(e)<10?"0":""}${e}`:`${e.length<2?"0":""}${e}`;static getValuesFromChannels=e=>{let t={};return channels.forEach((n=>{t[n[1]]=document.getElementById(`${e}-channels-${n[1]}`).value})),t};static appendValuesFromChannels=e=>{let t=functions.getValuesFromChannels(e),[n,l,o]=[t.r,t.g,t.b],s=functions.RGBToHex(n,l,o);return document.getElementById(`${e}-color`).value=s,s};static appendHexToChannels=(e,t)=>{let n=t??document.getElementById(`${e}-color`).value,l=functions.hexToRGBParsed(n),o={};return Object.keys(l).forEach((t=>{document.getElementById(`${e}-channels-${t}`).value=l[t],o[t]=l[t]})),o};static appendToChannels=(e,t)=>{Object.keys(t).forEach((n=>{document.getElementById(`${e}-channels-${n}`).value=t[n]}))};static parseWSUpdateMessage=(e,t,n,l,o,s,c,a,i)=>{let r=[e,...[e,n,l,o,s,c,a,1e3*(i??0)].slice(1,7).map((e=>Math.floor(parseInt(e)*(1-(t??0)/100)))),1e3*i];return[].concat(r,Array(8).slice(0,8-r.length))};static updateLight=(e,t,n,l,o,s,c,a,i)=>{let r=this.parseWSUpdateMessage(e,t,n,l,o,s,c,a,i);console.debug("WS Update RGB",r.join(";")),ws?.send(r.join(";"))};static parseWSUpdateMessagenoRGB(e,t,n){return[e,Math.floor(255*(1-t/100)),...[...Array(5)].map((e=>0)),1e3*n]}static updateLightnoRGB=(e,t,n)=>{console.debug("WS Update Non-RGB",[e,Math.floor(255*(1-t/100)),...[...Array(5)].map((e=>0)),1e3*n].join(";")),ws?.send(this.parseWSUpdateMessagenoRGB(e,t,n).join(";"))};static updateLightVal=(e,t,n)=>{let l=t;Array.isArray(l)||(l=[...Array(6)].map((e=>t))),console.debug("WS Update Val",[e,...l,1e3*n].join(";"));ws?.send([e,...l,1e3*n].join(";"))};static quickButtonActive=e=>{e.classList.add("quickActionButton-active")};static quickButtonInactive=e=>{e.classList.remove("quickActionButton-active")};static getQuickState=(e,t)=>quickStates[e]?.[t]??0;static appendHeld=e=>{let t=quickStates[e()];if(!t?.heldActions)return;let n=[],l=[];t.heldActions.reverse().forEach((e=>{let t=e[0],o=e[1];function s(e,t){0===e[0].length||t.filter((t=>t[0].join(";")===e[0].join(";")))[0]||t.push(e)}s(t,n),s(o,l)})),n.forEach((e=>{functions.updateLight(...e)})),l.forEach((e=>{functions.updateLightnoRGB(...e)})),t.heldActions=[]};static disableElem=e=>{e.disabled=!0};static enableElem=e=>{e.removeAttribute("disabled")}}class scripts{static loadLights=()=>{lights.split("---").forEach(((e,t)=>{let[n,l,o,s]=e.split(";"),c=parseInt(o)>1,a=document.createElement("div");a.id=`controls${l}`,a.classList.add("controlsContainer");let i=document.createElement("div");i.classList.add("controlsContainerTop");let r=document.createElement("div");r.classList.add("controlsContainerBottom");let u=document.createElement("h");u.classList.add("controlsTitle"),u.innerText=n;let d=document.createElement("h");d.classList.add("controlsGroupTitle"),d.innerText=s.split(",").join(", ");let p=elements.controlsColorInput(a.id),h=elements.controlsChannelInputs(a.id),m=elements.controlsDimmerInput(a.id),f=elements.controlsFadeInput(a.id);p.onchange=()=>{B(),functions.appendHexToChannels(a.id)},h.onchange=()=>{B(),functions.appendValuesFromChannels(a.id)},m.onchange=()=>{B()},f.onchange=()=>{B()};let g=document.createElement("button");function B(e){let t=m.querySelector(".controlsDimmerInput").value,n=f.querySelector(".controlsFadeInput").value,o=1===functions.getQuickState("_all","fullOn")||1===functions.getQuickState("_all","fullOff"),s=1===functions.getQuickState("_all","isHeld");if(!c)return s?void quickStates._all.heldActions.push([[],[[l],t,n]]):void(!e&&!liveUpdate||o||functions.updateLightnoRGB(l,t,n));let i=p.querySelector(".controlsColorInput").value,r=(functions.hexToRGBParsed(i),functions.getValuesFromChannels(a.id));s?quickStates._all.heldActions.push([[[l],t,...Object.values(r),n],[]]):!e&&!liveUpdate||o||functions.updateLight(l,t,...Object.values(r),n)}g.innerText="Update",g.classList.add("controlsSubmit"),g.onclick=()=>{B(!0)},[u,d].map((e=>i.appendChild(e))),[...c?[p,h]:[],m,f,g].forEach((e=>r.appendChild(e))),[i,r].map((e=>a.appendChild(e))),document.querySelector("#lightcontrols").appendChild(a)}))};static loadLightGroups=()=>{let e=document.createElement("div");e.classList.add("groupControlsContainer"),e.id="controlsGroup";let t=document.createElement("h");t.innerText="Group Controls",t.classList.add("controlsTitle");let n=document.createElement("select");n.classList.add("controlsSelector"),Object.keys(groups).forEach((e=>{let t=document.createElement("option");t.innerText=decodeURI(e),t.value=e,n.appendChild(t)}));let l=elements.controlsColorInput(e.id),o=elements.controlsChannelInputs(e.id),s=()=>`_group_${n.value}`,c=elements.controlsQuickActions(e.id,s,u),a=elements.controlsDimmerInput(e.id),i=elements.controlsFadeInput(e.id),r=document.createElement("button");function u(t,o,c,r){let u=l.querySelector(".controlsColorInput").value,d=a.querySelector(".controlsDimmerInput").value,p=i.querySelector(".controlsFadeInput").value,h=n.value;o&&functions.appendHexToChannels(e.id,u),c&&(u=functions.appendValuesFromChannels(e.id));let m=functions.getValuesFromChannels(e.id),f=1===functions.getQuickState(s(),"fullOn")||1===functions.getQuickState(s(),"fullOff"),g=1===functions.getQuickState(s(),"isHeld"),B=groups[h].filter((e=>parseInt(e.split(";")[2])>1)).map((e=>e.split(";")[1])),E=groups[h].filter((e=>1===parseInt(e.split(";")[2]))).map((e=>e.split(";")[1]));if(g)return quickStates[s()].heldActions.push([[B,d,...Object.values(m),p],[E,d,p]]);r?(functions.updateLightVal(B,r[0],0),functions.updateLightVal(E,r[1],0)):!t&&!liveUpdate||f||(B.length>0&&functions.updateLight(B.join(","),d,...Object.values(m),p),E.length>0&&functions.updateLightnoRGB(E.join(","),d,p)),groups[h].map((e=>e.split(";")[1])).forEach((e=>{let t=lights.split("---").filter((t=>t.split(";")[1]===e))[0]?.split(";"),n=t?.[1];if(void 0===n)return;if(!document.getElementById(`controls${n}`))return;let l=parseInt(t[2])>1;document.getElementById(`controls${n}-dimm`).value=d,document.getElementById(`controls${n}-fade`).value=p,l&&(document.getElementById(`controls${n}-color`).value=u,functions.appendToChannels(`controls${n}`,m))}))}r.innerText="Update",r.classList.add("controlsSubmit"),r.onclick=()=>{u(!0)},l.onchange=()=>{u(!1,!0,!1)},o.onchange=()=>{u(!1,!1,!0)},a.onchange=()=>{u()},i.onchange=()=>{u()},[t,n,l,o,c,a,i,r].forEach((t=>e.appendChild(t))),document.querySelector("#lightcontrolsgroup").appendChild(e)};static loadLightAll=()=>{let e=document.createElement("div");e.classList.add("groupControlsContainer"),e.id="controlsGroupAll";let t=document.createElement("h");t.innerText="Master Controls",t.classList.add("controlsTitle");let n=elements.controlsColorInput(e.id),l=elements.controlsChannelInputs(e.id),o=()=>"_all",s=elements.controlsQuickActions(e.id,o,r),c=elements.controlsDimmerInput(e.id),a=elements.controlsFadeInput(e.id),i=document.createElement("button");function r(t,l,o,s){let i=n.querySelector(".controlsColorInput").value,r=c.querySelector(".controlsDimmerInput").value,u=a.querySelector(".controlsFadeInput").value;l&&functions.appendHexToChannels(e.id,i),o&&(i=functions.appendValuesFromChannels(e.id));let d=functions.getValuesFromChannels(e.id),p=1===functions.getQuickState("_all","fullOn")||1===functions.getQuickState("_all","fullOff"),h=1===functions.getQuickState("_all","isHeld"),m=lights.split("---").filter((e=>parseInt(e.split(";")[2])>1)).map((e=>e.split(";")[1])),f=lights.split("---").filter((e=>1===parseInt(e.split(";")[2]))).map((e=>e.split(";")[1]));if(h)return quickStates._all.heldActions.push([[m,r,...Object.values(d),u],[f,r,u]]);s?(functions.updateLightVal(m,s[0],0),functions.updateLightVal(f,s[1],0)):!t&&!liveUpdate||p||(m.length>0&&functions.updateLight(m.join(","),r,...Object.values(d),u),f.length>0&&functions.updateLightnoRGB(f.join(","),r,u)),lights.split("---").map((e=>e.split(";"))).map((e=>{let t=e[1];if(!document.getElementById(`controls${t}`))return;let n=parseInt(e[2])>1;document.getElementById(`controls${t}-dimm`).value=r,document.getElementById(`controls${t}-fade`).value=u,n&&(document.getElementById(`controls${t}-color`).value=i,functions.appendToChannels(`controls${t}`,d))})),document.getElementById("controlsGroup")&&(document.getElementById("controlsGroup-color").value=i,document.getElementById("controlsGroup-dimm").value=r,document.getElementById("controlsGroup-fade").value=u,functions.appendToChannels("controlsGroup",d))}i.innerText="Update",i.classList.add("controlsSubmit"),i.onclick=()=>{r(!0)},n.onchange=()=>{r(!1,!0,!1)},l.onchange=()=>{r(!1,!1,!0)},c.onchange=()=>{r()},a.onchange=()=>{r()},[t,n,l,s,c,a,i].forEach((t=>e.appendChild(t))),document.querySelector("#lightcontrolsgroup").appendChild(e)};static loadLiveUpdateToggle=()=>{let e=document.createElement("div");e.id="liveUpdateContainer";let t=document.createElement("h");t.id="liveUpdateTitle",t.innerText="Live Update";let n=document.createElement("input");n.type="checkbox",n.onchange=()=>{liveUpdate=n.checked,scripts.appendLiveUpdate()},[t,n].forEach((t=>e.appendChild(t))),document.querySelector("body").appendChild(e)};static loadGroupToggle=()=>{let e=document.createElement("div");e.id="groupToggleContainer";let t=document.createElement("h");t.id="groupToggleTitle",t.innerText="Show groups";let n=document.createElement("input");n.type="checkbox",n.defaultChecked=!0,n.onchange=()=>{groupToggle=n.checked,scripts.appendGroupToggleUpdate()},[t,n].forEach((t=>e.appendChild(t))),document.querySelector("body").appendChild(e)};static appendLiveUpdate=()=>{document.querySelectorAll(".controlsSubmit").forEach((e=>{liveUpdate?e.classList.add("hidden"):e.classList.remove("hidden")})),document.querySelectorAll(".controlsFade").forEach((e=>{e.value=liveUpdate?0:1}))};static appendGroupToggleUpdate=()=>{document.querySelectorAll(".controlsGroupTitle").forEach((e=>{groupToggle?e.classList.remove("hidden"):e.classList.add("hidden")}))}}class elements{static br=()=>document.createElement("br");static spacer=()=>document.createElement("spacer");static controlsColorInput=e=>{let t=document.createElement("div");t.classList.add("controlsColorPickerContainer");let n=document.createElement("h");n.innerText="Color Picker";let l=document.createElement("input");return e&&(l.id=`${e}-color`),l.classList.add("controlsColorInput"),l.type="color",l.defaultValue="#000000",[n,l].forEach((e=>t.appendChild(e))),t};static controlsChannelInputs=e=>{let t=document.createElement("div");t.classList.add("controlsChannelsContainer");let n=document.createElement("div"),l=document.createElement("h");l.innerText="Channels";let o=document.createElement("div");e&&(o.id=`${e}-channels`),o.classList.add("controlsChannelsInputContainer");let s=document.createElement("div");return e&&(s.id=`${e}-channel-titles`),s.classList.add("controlsChannelTitlesContainer"),channels.forEach((t=>{let n=document.createElement("input");e&&(n.id=`${o.id}-${t[1]}`),n.classList.add("controlsChannelInput",`controlsChannel-${t[1]}`,"vertical"),n.type="range",n.style.backgroundColor=t[2],n.min=0,n.max=255,n.defaultValue=t[3];let l=document.createElement("h");l.classList.add("controlsChannelTitle"),l.title=t[0],l.innerText=t[1],o.appendChild(n),s.appendChild(l)})),[l,o,s].forEach((e=>n.appendChild(e))),t.appendChild(n),t};static controlsDimmerInput=e=>{let t=document.createElement("div");t.classList.add("controlsDimmerContainer");let n=document.createElement("div"),l=document.createElement("h");l.innerText="Dimmer";let o=document.createElement("input");return e&&(o.id=`${e}-dimm`),o.classList.add("dimmer"),o.classList.add("controlsDimmerInput","horizontal"),o.type="range",o.min=0,o.max=100,o.defaultValue=0,[l,o].forEach((e=>n.appendChild(e))),t.appendChild(n),t};static controlsFadeInput=e=>{let t=document.createElement("div");t.classList.add("controlsFadeContainer");let n=document.createElement("div"),l=document.createElement("h");l.innerText="Fade";let o=document.createElement("input");return o.id=`${e}-fade`,o.classList.add("controlsFadeInput","horizontal"),o.type="range",o.min=0,o.max=30,o.defaultValue=0,[l,o].forEach((e=>n.appendChild(e))),t.appendChild(n),t};static controlsQuickActions=(e,t,n)=>{let l=()=>t?.()??"_all",o=document.createElement("div");o.classList.add("controlsQuickActionsContainer");let s=document.createElement("button"),c=document.createElement("button"),a=document.createElement("button");function i(e,t){switch(e){case"fullOn":quickStates[l()].fullOn=t,quickStates[l()].fullOff=2;break;case"fullOff":quickStates[l()].fullOff=t,quickStates[l()].fullOn=2;break;case"isHeld":quickStates[l()].isHeld=t}}function r(e){quickStates[l()]||(quickStates[l()]={fullOn:0,fullOff:0,isHeld:0,heldActions:[]});let o=["fullOn","fullOff","isHeld"],r=[s,c,a],u=r.indexOf(e.target),d=[...r].filter((t=>t!==e.target)),p=r[u],h=o[u],m=functions.getQuickState(l(),h);switch(m){case 0:case 2:switch(functions.quickButtonActive(p),i(h,1),o[u]){case"fullOn":case"fullOff":n(!1,!1,!1,[[0,0,0,[255,0][u],0,0],[[255,0][u],0,0,0,0,0]])}break;case 1:if(functions.quickButtonInactive(p),i(h,0),"isHeld"===o[u])functions.appendHeld(t)}switch(o[r.indexOf(p)]){case"fullOn":case"fullOff":return void d.filter((e=>e!==a)).forEach((e=>{functions.quickButtonInactive(e)}));case"isHeld":return void d.forEach((e=>{[functions.disableElem,functions.enableElem][m]?.(e),functions.quickButtonInactive(e),i(o[r.indexOf(e)],2)}))}}return s.innerText="Full On",c.innerText="Full Off",a.innerText="Hold",[s,c,a].map((e=>{e.classList.add("quickActionButton"),o.appendChild(e),e.onclick=r})),o}}window.onload=()=>{scripts.loadLights(),scripts.loadLightGroups(),scripts.loadLightAll(),scripts.loadLiveUpdateToggle(),scripts.loadGroupToggle()}</script><style>*{scrollbar-width:thin;font-family:"Trebuchet MS","Lucida Sans Unicode","Lucida Grande","Lucida Sans",Arial,sans-serif;border-radius:10px;text-align:center;user-select:none;padding:3px}.hidden{display:none}@media screen and (-webkit-min-device-pixel-ratio:0){.controlsChannelInput{overflow:hidden;appearance:none;-webkit-appearance:none;background-color:#fff;border:1px solid}.controlsChannelInput::-webkit-slider-runnable-track{height:10px;appearance:none;-webkit-appearance:none;color:#fff;background-color:transparent;margin-top:-1px}.controlsChannelInput::-webkit-slider-thumb{width:10px;-webkit-appearance:none;height:10px;border:1px solid #fff;border-radius:100%;background:0 0;box-shadow:396px 0 2px 400px #000}}.horizontal{cursor:w-resize}.vertical{cursor:n-resize}.controlsChannelInput{border:1px solid #fff;background-color:#000}.controlsChannelInput::-moz-range-progress{background-color:#fcca46}.controlsChannelInput::-moz-range-track{background-color:#000}body{background-color:#233d4d;transition:ease-in 2s;color:#f1faee;padding:3px}#lightcontrols,#lightcontrolsgroup{display:flex;place-items:center;flex-wrap:wrap;justify-content:center;align-items:stretch}.controlsContainer{display:flex;flex:1 0 24%}.controlsContainer,.groupControlsContainer{max-width:23%;padding:6px;margin:2px;width:25%;gap:3px;border:1px solid;background-color:#1d2d44;display:flex;flex-direction:column;align-items:center;justify-content:space-evenly}.groupControlsContainer{max-width:none;width:40%}spacer{display:flex;height:10px}.controlsTitle{font-size:large;height:50%}.controlsDimmerInput,.controlsFadeInput{width:80%}.controlsColorInput,.controlsSubmit{border:none;outline:0}.controlsSelector{border:1px solid transparent}.controlsColorInput{background-color:transparent;box-sizing:content-box;padding:0;height:30px}.controlsSubmit{padding:4px}.controlsGroupTitle{font-size:small;overflow-y:auto;height:50%}.controlsChannelsContainer,.controlsColorPickerContainer,.controlsDimmerContainer,.controlsFadeContainer,.controlsQuickActionsContainer{display:flex;flex-wrap:wrap;justify-content:center;align-items:center;border:1px solid;flex-direction:column;width:100%}.controlsDimmerContainer>div,.controlsFadeContainer>div{display:flex;flex-direction:column;justify-content:center;align-items:center;width:90%}.controlsQuickActionsContainer{flex-direction:row}@media screen and (max-width:700px){.controlsChannelsContainer{display:none}}.controlsChannelsContainer{flex-direction:column;flex-wrap:nowrap}.controlsChannelsInputContainer{display:flex;flex-wrap:wrap;justify-content:center;flex-direction:column;transform:rotate(-90deg);row-gap:1px;justify-content:center;aspect-ratio:1/1.2}.controlsChannelTitle{cursor:help}.controlsChannelTitlesContainer{display:flex;justify-content:space-evenly}.controlsColorPickerContainer{flex-direction:column;justify-content:center}#liveUpdateContainer{background-color:#38a3a5;z-index:10;position:absolute;right:10px;top:10px}#groupToggleContainer{background-color:#38a3a5;z-index:10;position:absolute;left:10px;top:10px}.quickActionButton{display:flex;border-radius:6px;border:1px solid #f1faee;margin:2px;background-color:#220901;color:#f1faee}.quickActionButton:disabled{color:grey}@keyframes blink{0%,49%{background-color:red;color:#000}50%,99%{background-color:#220901;color:red}}.quickActionButton-active{background-color:#a1c181;animation:blink 1s infinite}.controlsContainerBottom,.controlsContainerTop{display:flex;flex-direction:column;align-items:center;margin:0;padding:0;width:100%;gap:2px}.controlsContainerTop{height:45%}.controlsContainerTop{height:55%}</style>)rawliteral";

void notifyClientsRefresh()
{
    String states;
    for (int i; i < maxLights; i++)
    {
        states = states + ";" + lightStates[i].start_address + "," + lightStates[i].red + "," + lightStates[i].green + "," + lightStates[i].blue + "," + lightStates[i].white + "," + lightStates[i].amber + "," + lightStates[i].uv;
    }
    String msg = "refresh;" + states;
    ws.textAll(String(ledState));
}

void splitStringToIntegers(const String &data, int addr[], int &r, int &g, int &b, int &w, int &am, int &uv)
{
    // Temporary variables to store intermediate values
    String tempAddr, tempR, tempG, tempB, tempW, tempAM, tempUV;

    Serial.println("data " + data);

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
    Serial.println("tempAddr " + tempAddr);
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
            Serial.println("addr changed:" + String(addr));
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
            Serial.println("updating addr: " + String(addr));
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
    Serial.print("sending... ");
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
                Serial.println(String(addr) + "not changed");
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
                Serial.println(String(addr) + "-" + String(addrChanged[numAddrChanged - 1]) + "fading");
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
                    Serial.println("progress: " + String(progress));
                    Serial.println("red: " + String(r_old + (r - r_old) * progress));

                    for (int j = 0; j < numAddrChanged; j++)
                    {
                        // Calculate the current brightness value
                        int red = r_old + (r - r_old) * progress;
                        int green = g_old + (g - g_old) * progress;
                        int blue = b_old + (b - b_old) * progress;
                        int white = w_old + (w - w_old) * progress;
                        int am = am_old + (am - am_old) * progress;
                        int uv = uv_old + (uv - uv_old) * progress;
                        Serial.println("test0");
                        dmx.write(addrChanged[j], red);
                        dmx.write(addrChanged[j] + 1, green);
                        dmx.write(addrChanged[j] + 2, blue);
                        dmx.write(addrChanged[j] + 3, white);
                        dmx.write(addrChanged[j] + 4, am);
                        dmx.write(addrChanged[j] + 5, uv);
                        Serial.println("test1");
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

                    Serial.println("test2");
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
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        Serial.println((char *)data);

        // [addr, dimm, r, g, b, w, str]

        // Temporary variables to store intermediate values
        String data_str = (char *)data;

        // Find the position of semicolons in the string
        int semicolon1 = data_str.indexOf(';');

        // Extract individual substrings between semicolons
        String command = data_str.substring(0, semicolon1);

        if (command = "refresh")
        {
            notifyClientsRefresh();
        }
        else if (command = "restart"){
            ESP.restart();
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
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        led_intv = 100;
        notifyClientsRefresh();
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        led_intv = 500;
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
    Serial.println(var);

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
        dmx.write(1, random(0, 255));
        dmx.write(2, random(0, 255));
        dmx.write(3, random(0, 255));
        dmx.write(4, random(0, 255));
        dmx.write(5, random(0, 255));
        dmx.write(6, random(0, 255));
        dmx.write(7, random(0, 255));
        dmx.write(8, random(0, 255));
        dmx.update();
        delay(10);
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
    Serial.begin(115200);
    delay(500);
    Serial.println("Setting up WiFi");

    WiFi.mode(WIFI_AP_STA);

    // Remove the password parameter, if you want the AP (Access Point) to be open
    WiFi.softAP(ssid, password, wifi_channel, ssid_hidden);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    initWebSocket();

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", index_html, processor); });

    server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request)
              {
    if (request->hasArg("plain")) {
      String data = request->arg("plain");
      Serial.println("Received new config: " + data);
      preferences.putString("config", data);
    }
    request->send(200); });

    // Start server
    server.begin();

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
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
    if (currentMillis - previousMillis >= led_intv)
    {
        previousMillis = currentMillis;
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
}
