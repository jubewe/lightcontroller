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

String config = "SomeName0;0;group1\nSomeName1;1;group1";

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
const char index_html[] PROGMEM = R"rawliteral(<!-- Welcome to the Frontend --><!-- https://github.com/jubewe/lightcontroller --><!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta http-equiv="X-UA-Compatible"content="IE=edge"><meta name="viewport"content="width=device-width,initial-scale=1"><title>Lightcontroller</title></head><body><h3>Lightcontroller</h3><div id="lightcontrols"></div><div id="lightcontrolsgroup"></div></body></html><script>const url=new URL(document.baseURI),lights="Bühne Links Außen Hinten;1;1;Bühne,Links,Bühne Links,Bühne Hinten,Bühne Außen,Bühne Links Außen,Bühne Links Hinten---Bühne Links Innen Hinten;7;1;Bühne,Links,Bühne Links,Bühne Hinten,Bühne Innen,Bühne Links Innen,Bühne Links Hinten---Bühne Rechts Innen Hinten;13;1;Bühne,Rechts,Bühne Rechts,Bühne Hinten,Bühne Innen,Bühne Rechts Innen,Bühne Rechts Hinten---Bühne Rechts Außen Hinten;19;1;Bühne,Rechts,Bühne Rechts,Bühne Hinten,Bühne Außen,Bühne Rechts Außen,Bühne Rechts Hinten---Bühne Links Außen Vorne;25;1;Bühne,Links,Bühne Links,Bühne Vorne,Bühne Außen,Bühne Links Außen,Bühne Links Vorne---Bühne Links Innen Vorne;31;1;Bühne,Links,Bühne Links,Bühne Vorne,Bühne Innen,Bühne Links Innen,Bühne Links Vorne---Bühne Rechts Innen Vorne;37;1;Bühne,Rechts,Bühne Rechts,Bühne Vorne,Bühne Innen,Bühne Rechts Innen,Bühne Rechts Vorne---Bühne Rechts Außen Vorne;43;1;Bühne,Rechts,Bühne Rechts,Bühne Vorne,Bühne Außen,Bühne Rechts Außen,Bühne Rechts Vorne---Spot Links Oben;49;6;Spots,Spots Links,Spots Oben,Links---Spot Links Unten;55;6;Spots,Spots Links,Spots Unten,Links---Spot Rechts Oben;61;6;Spots,Spots Rechts,Spots Oben,Rechts---Spot Rechts Unten;67;6;Spots,Spots Rechts,Spots Unten,Rechts";console.debug("Loaded config",lights);const channels=[["Red","r","#ff0000",0],["Green","g","#00ff00",0],["Blue","b","#0000ff",0],["White","w","#ffffff",255],["Amber","a","#ffbf00",0],["UV","u","#5f4b8b",0]],rangeInputMin=0,rangeInputMax=255,rangeInputDefault=0,fadeRangeMin=0,fadeRangeMax=30,fadeRangeDefault=0,fadeDefaultTime=1,fadeLiveDefaultTime=0,channelsPerRGBLight=6;let groups={},liveUpdate=!1,groupToggle=!0;lights.split("---").forEach((e=>{let n=e.split(";")[3];0!==n.length&&n.split(",").forEach((n=>{groups[encodeURI(n)]||(groups[encodeURI(n)]=[]),groups[encodeURI(n)].push(e)}))}));const ws=new WebSocket(`ws://${window.location.hostname}/ws`);ws.onopen=()=>{console.log("WS Opened")},ws.onmessage=e=>{console.log("WS Message",e)},ws.onerror=e=>{console.error("WS Error",e)};class functions{static request=(e,n)=>new Promise(((t,o)=>{/^http(s)*:\/{2}/i.test(e)||(e=url.origin+e),n=n??{},fetch(e,n).then((e=>{t(e.body)})).catch(o)}));static submitConfig=e=>{this.request("/config",{headers:{plain:"string"!=typeof e?JSON.stringify(e):e},method:"POST"}).then((()=>{console.debug("POST Config Success")})).catch((e=>{console.error("POST Config error",e)}))};static getWhite=e=>"ffffff"===e.replace(/^#/,"").toLowerCase()?255:0;static hexToRGBParsed=e=>functions.hexToRGB(e);static hexToRGB=e=>{let n={};return e.replace(/^#/,"").match(/\w{2}/g).forEach(((e,t)=>n[["r","g","b"][t]]=parseInt(e,16))),n};static RGBToHex=(e,n,t,o)=>{let l="#";return l+=functions.pad2(Math.abs(e).toString(16)),l+=functions.pad2(Math.abs(n).toString(16)),l+=functions.pad2(Math.abs(t).toString(16)),l};static pad2=e=>/^\d$/.test(e)?`${parseInt(e)<10?"0":""}${e}`:`${e.length<2?"0":""}${e}`;static getValuesFromChannels=e=>{let n={};return channels.forEach((t=>{n[t[1]]=document.getElementById(`${e}-channels-${t[1]}`).value})),n};static appendValuesFromChannels=e=>{let n=functions.getValuesFromChannels(e),[t,o,l]=[n.r,n.g,n.b],s=functions.RGBToHex(t,o,l);return document.getElementById(`${e}-color`).value=s,s};static appendHexToChannels=(e,n)=>{let t=n??document.getElementById(`${e}-color`).value,o=functions.hexToRGBParsed(t),l={};return Object.keys(o).forEach((n=>{document.getElementById(`${e}-channels-${n}`).value=o[n],l[n]=o[n]})),l};static appendToChannels=(e,n)=>{Object.keys(n).forEach((t=>{document.getElementById(`${e}-channels-${t}`).value=n[t]}))};static updateLight=(e,n,t,o,l,s,c,a,r)=>{let i=[e,...[e,t,o,l,s,c,a,1e3*(r??0)].slice(1,8).map((e=>Math.floor(parseInt(e)*(1-(n??0)/100))))],d=[].concat(i,Array(8).slice(0,8-i.length));ws.send(d.join(";"))};static updateLightnoRGB=(e,n,t)=>{ws.send([e,Math.floor(255*(1-n/100)),...[...Array(5)].map((e=>0)),1e3*t].join(";"))}}class scripts{static loadLights=()=>{lights.split("---").forEach(((e,n)=>{let[t,o,l,s]=e.split(";"),c=parseInt(l)>1,a=document.createElement("div");a.id=`controls${o}`,a.classList.add("controlsContainer");let r=document.createElement("h");r.classList.add("controlsTitle"),r.innerText=t;let i=document.createElement("h");i.classList.add("controlsGroupTitle"),i.innerText=s.split(",").join(", ");let d=elements.controlsColorInput(a.id),u=elements.controlsChannelInputs(a.id),h=elements.controlsDimmerInput(a.id),p=elements.controlsFadeInput(a.id);d.onchange=()=>{g(),functions.appendHexToChannels(a.id)},u.onchange=()=>{g(),functions.appendValuesFromChannels(a.id)},h.onchange=()=>{g()},p.onchange=()=>{g()};let m=document.createElement("button");function g(e){let n=h.querySelector(".controlsDimmerInput").value,t=p.querySelector(".controlsFadeInput").value;if(!c)return void((e||liveUpdate)&&functions.updateLightnoRGB(o,n,t));let l=d.querySelector(".controlsColorInput").value,s=(functions.hexToRGBParsed(l),functions.getValuesFromChannels(a.id));(e||liveUpdate)&&functions.updateLight(o,n,...Object.values(s),t)}m.innerText="Update",m.classList.add("controlsSubmit"),m.onclick=()=>{g(!0)},[r,i,...c?[d,u]:[],h,p,m].forEach((e=>a.appendChild(e))),document.querySelector("#lightcontrols").appendChild(a)}))};static loadLightGroups=()=>{let e=document.createElement("div");e.classList.add("groupControlsContainer"),e.id="controlsGroup";let n=document.createElement("h");n.innerText="Group Controls",n.classList.add("controlsTitle");let t=document.createElement("select");Object.keys(groups).forEach((e=>{let n=document.createElement("option");n.innerText=decodeURI(e),n.value=e,t.appendChild(n)}));let o=elements.controlsColorInput(e.id),l=elements.controlsChannelInputs(e.id),s=elements.controlsDimmerInput(e.id),c=elements.controlsFadeInput(e.id),a=document.createElement("button");function r(n,l,a){let r=o.querySelector(".controlsColorInput").value,i=s.querySelector(".controlsDimmerInput").value,d=c.querySelector(".controlsFadeInput").value,u=t.value;l&&functions.appendHexToChannels(e.id,r),a&&(r=functions.appendValuesFromChannels(e.id));let h=functions.getValuesFromChannels(e.id);(n||liveUpdate)&&functions.updateLight(groups[u].filter((e=>parseInt(e.split(";")[2])>1)).map((e=>e.split(";")[1])).join(","),i,...Object.values(h),d),groups[u].map((e=>e.split(";")[1])).forEach((e=>{let n=lights.split("---").filter((n=>n.split(";")[1]===e))[0]?.split(";"),t=n?.[1];if(void 0===t)return;if(!document.getElementById(`controls${t}`))return;let o=parseInt(n[2])>1;document.getElementById(`controls${t}-dimm`).value=i,document.getElementById(`controls${t}-fade`).value=d,o&&(document.getElementById(`controls${t}-color`).value=r,functions.appendToChannels(`controls${t}`,h))}))}a.innerText="Update",a.classList.add("controlsSubmit"),a.onclick=()=>{r(!0)},o.onchange=()=>{r(!1,!0,!1)},l.onchange=()=>{r(!1,!1,!0)},s.onchange=()=>{r()},c.onchange=()=>{r()},[n,t,o,l,s,c,a].forEach((n=>e.appendChild(n))),document.querySelector("#lightcontrolsgroup").appendChild(e)};static loadLightAll=()=>{let e=document.createElement("div");e.classList.add("groupControlsContainer"),e.id="controlsGroupAll";let n=document.createElement("h");n.innerText="Master Controls",n.classList.add("controlsTitle");let t=elements.controlsColorInput(e.id),o=elements.controlsChannelInputs(e.id),l=elements.controlsDimmerInput(e.id),s=elements.controlsFadeInput(e.id),c=document.createElement("button");function a(n,o,c){let a=t.querySelector(".controlsColorInput").value,r=l.querySelector(".controlsDimmerInput").value,i=s.querySelector(".controlsFadeInput").value;o&&functions.appendHexToChannels(e.id,a),c&&(a=functions.appendValuesFromChannels(e.id));let d=functions.getValuesFromChannels(e.id);(n||liveUpdate)&&functions.updateLight(lights.split("---").filter((e=>parseInt(e.split(";")[2])>1)).map((e=>e.split(";")[1])).join(","),r,...Object.values(d),i),lights.split("---").map((e=>e.split(";"))).map((e=>{let n=e[1];if(!document.getElementById(`controls${n}`))return;let t=parseInt(e[2])>1;document.getElementById(`controls${n}-dimm`).value=r,document.getElementById(`controls${n}-fade`).value=i,t&&(document.getElementById(`controls${n}-color`).value=a,functions.appendToChannels(`controls${n}`,d))})),document.getElementById("controlsGroup")&&(document.getElementById("controlsGroup-color").value=a,document.getElementById("controlsGroup-dimm").value=r,document.getElementById("controlsGroup-fade").value=i,functions.appendToChannels("controlsGroup",d))}c.innerText="Update",c.classList.add("controlsSubmit"),c.onclick=()=>{a(!0)},t.onchange=()=>{a(!1,!0,!1)},o.onchange=()=>{a(!1,!1,!0)},l.onchange=()=>{a()},s.onchange=()=>{a()},[n,t,o,l,s,c].forEach((n=>e.appendChild(n))),document.querySelector("#lightcontrolsgroup").appendChild(e)};static loadLiveUpdate=()=>{let e=document.createElement("div");e.id="liveUpdateContainer";let n=document.createElement("h");n.id="liveUpdateTitle",n.innerText="Live Update";let t=document.createElement("input");t.type="checkbox",t.onchange=()=>{liveUpdate=t.checked,scripts.appendLiveUpdate()},[n,t].forEach((n=>e.appendChild(n))),document.querySelector("body").appendChild(e)};static loadGroupToggle=()=>{let e=document.createElement("div");e.id="groupToggleContainer";let n=document.createElement("h");n.id="groupToggleTitle",n.innerText="Show groups";let t=document.createElement("input");t.type="checkbox",t.defaultChecked=!0,t.onchange=()=>{groupToggle=t.checked,scripts.appendGroupToggleUpdate()},[n,t].forEach((n=>e.appendChild(n))),document.querySelector("body").appendChild(e)};static appendLiveUpdate=()=>{document.querySelectorAll(".controlsSubmit").forEach((e=>{liveUpdate?e.classList.add("hidden"):e.classList.remove("hidden")})),document.querySelectorAll(".controlsFade").forEach((e=>{e.value=liveUpdate?0:1}))};static appendGroupToggleUpdate=()=>{document.querySelectorAll(".controlsGroupTitle").forEach((e=>{groupToggle?e.classList.remove("hidden"):e.classList.add("hidden")}))}}class elements{static br=()=>document.createElement("br");static spacer=()=>document.createElement("spacer");static controlsColorInput=e=>{let n=document.createElement("div");n.classList.add("controlsColorPickerContainer");let t=document.createElement("h");t.innerText="Color Picker";let o=document.createElement("input");return e&&(o.id=`${e}-color`),o.classList.add("controlsColorInput"),o.type="color",o.defaultValue="#000000",[t,o].forEach((e=>n.appendChild(e))),n};static controlsChannelInputs=e=>{let n=document.createElement("div");n.classList.add("controlsChannelsContainer");let t=document.createElement("div"),o=document.createElement("h");o.innerText="Channels";let l=document.createElement("div");e&&(l.id=`${e}-channels`),l.classList.add("controlsChannelsInputContainer");let s=document.createElement("div");return e&&(s.id=`${e}-channel-titles`),s.classList.add("controlsChannelTitlesContainer"),channels.forEach((n=>{let t=document.createElement("input");e&&(t.id=`${l.id}-${n[1]}`),t.classList.add("controlsChannelInput",`controlsChannel-${n[1]}`,"vertical"),t.type="range",t.style.backgroundColor=n[2],t.min=0,t.max=255,t.defaultValue=n[3];let o=document.createElement("h");o.classList.add("controlsChannelTitle"),o.title=n[0],o.innerText=n[1],l.appendChild(t),s.appendChild(o)})),[o,l,s].forEach((e=>t.appendChild(e))),n.appendChild(t),n};static controlsDimmerInput=e=>{let n=document.createElement("div");n.classList.add("controlsDimmerContainer");let t=document.createElement("div"),o=document.createElement("h");o.innerText="Dimmer";let l=document.createElement("input");return e&&(l.id=`${e}-dimm`),l.classList.add("dimmer"),l.classList.add("controlsDimmerInput","horizontal"),l.type="range",l.min=0,l.max=100,l.defaultValue=0,[o,l].forEach((e=>t.appendChild(e))),n.appendChild(t),n};static controlsFadeInput=e=>{let n=document.createElement("div");n.classList.add("controlsFadeContainer");let t=document.createElement("div"),o=document.createElement("h");o.innerText="Fade";let l=document.createElement("input");return l.id=`${e}-fade`,l.classList.add("controlsFadeInput","horizontal"),l.type="range",l.min=0,l.max=30,l.defaultValue=0,[o,l].forEach((e=>t.appendChild(e))),n.appendChild(t),n}}window.onload=()=>{scripts.loadLights(),scripts.loadLightGroups(),scripts.loadLightAll(),scripts.loadLiveUpdate(),scripts.loadGroupToggle()}</script><style>*{scrollbar-width:thin;font-family:"Trebuchet MS","Lucida Sans Unicode","Lucida Grande","Lucida Sans",Arial,sans-serif;border-radius:10px;text-align:center;user-select:none;padding:3px}.hidden{display:none}@media screen and (-webkit-min-device-pixel-ratio:0){.controlsChannelInput{overflow:hidden;appearance:none;-webkit-appearance:none;background-color:#fff;border:1px solid}.controlsChannelInput::-webkit-slider-runnable-track{height:10px;appearance:none;-webkit-appearance:none;color:#fff;background-color:transparent;margin-top:-1px}.controlsChannelInput::-webkit-slider-thumb{width:10px;-webkit-appearance:none;height:10px;border:1px solid #fff;border-radius:100%;background:0 0;box-shadow:396px 0 2px 400px #000}}.horizontal{cursor:w-resize}.vertical{cursor:n-resize}.controlsChannelInput{border:1px solid #fff;background-color:#000}.controlsChannelInput::-moz-range-progress{background-color:#fcca46}.controlsChannelInput::-moz-range-track{background-color:#000}body{background-color:#233d4d;transition:ease-in 2s;color:#f1faee;padding:3px}#lightcontrols,#lightcontrolsgroup{display:flex;place-items:center;flex-wrap:wrap;justify-content:center;align-items:stretch}.controlsContainer{display:flex;flex:1 0 24%}.controlsContainer,.groupControlsContainer{max-width:23%;padding:6px;margin:2px;width:25%;gap:3px;border:1px solid;background-color:#1d2d44;display:flex;flex-direction:column;align-items:center;justify-content:space-evenly}.groupControlsContainer{max-width:none;width:40%}spacer{display:flex;height:10px}.controlsTitle{font-size:large}.controlsDimmerInput,.controlsFadeInput{width:80%}.controlsColorInput,.controlsSubmit{border:none;outline:0}.controlsColorInput{background-color:transparent;box-sizing:content-box;padding:0}.controlsSubmit{padding:4px}.controlsGroupTitle{font-size:small}.controlsChannelsContainer,.controlsColorPickerContainer,.controlsDimmerContainer,.controlsFadeContainer{display:flex;flex-wrap:wrap;justify-content:center;align-items:center;border:1px solid;flex-direction:column;width:100%}.controlsDimmerContainer>div,.controlsFadeContainer>div{display:flex;flex-direction:column;justify-content:center;align-items:center;width:90%}.controlsChannelsContainer{flex-direction:column;flex-wrap:nowrap}.controlsChannelsInputContainer{display:flex;flex-wrap:wrap;justify-content:center;flex-direction:column;transform:rotate(-90deg);row-gap:1px;justify-content:center;aspect-ratio:1/1.2}.controlsChannelTitle{cursor:help}.controlsChannelTitlesContainer{display:flex;justify-content:space-evenly}.controlsColorPickerContainer{flex-direction:column;justify-content:center}#liveUpdateContainer{background-color:#38a3a5;z-index:10;position:absolute;right:10px;top:10px}#groupToggleContainer{background-color:#38a3a5;z-index:10;position:absolute;left:10px;top:10px}</style>)rawliteral";

void notifyClients()
{
    // ws.textAll(String(ledState));
    // Serial.println(ledState);
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

        int addr[3]; // number of different addresses
        int dimm, r, g, b, w, am, uv;
        int address_size;

        splitStringToIntegers((char *)data, addr, r, g, b, w, am, uv);
        newData = true;
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
    // replace strings in the html
    Serial.println(var);
    if (var == "CONFIG")
    {
        Serial.println(config);
        return "SomeName0;0;group1---SomeName1;1;group1---SomeName2;10;group3";
    }
    return String();
}

void setup()
{
    // initialize dmx
    dmx.init(512);
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
