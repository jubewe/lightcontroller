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
const int led_intv = 500;
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
}

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// complete code in /frontend
const char index_html[] PROGMEM = R"rawliteral(<!-- Welcome to the Frontend --> <!-- https://github.com/jubewe/lightcontroller --> <!DOCTYPE html> <html lang="en"> <head> <meta charset="UTF-8"> <meta http-equiv="X-UA-Compatible" content="IE=edge"> <meta name="viewport" content="width=device-width, initial-scale=1.0"> <title>Lightcontroller</title> </head> <body> <h3>Lightcontroller</h3> <div id="lightcontrols"></div> <div id="lightcontrolsgroup"></div> </body> </html> <script> const url = new URL(document.baseURI); /* const lights = "{{CONFIG}}"; */  const lights = "Left Top;1;6;Spots,Spots Left,Left---Left Bottom;7;6;Spots,Spots Left,Left---Right Top;13;6;Spots,Spots Right,Right---Right Bottom;19;6;Spots,Spots Right,Right";  /* const lights = "Left Top;1;6;Spots,Spots Left,Left---Left Bottom;7;6;Spots,Spots Left,Left---Right Top;13;6;Spots,Spots Right,Right---Right Bottom;19;6;Spots,Spots Right,Right---Testing;25;1;desd"; */ console.debug("Loaded config", lights); const channels = [["Red", "r", "#ff0000", 0], ["Green", "g", "#00ff00", 0], ["Blue", "b", "#0000ff", 0], ["White", "w", "#ffffff", 255], ["Amber", "a", "#ffbf00", 0], ["UV", "u", "#5f4b8b", 0]]; const rangeInputMin = 0; const rangeInputMax = 255; const rangeInputDefault = 0; const fadeRangeMin = 0; const fadeRangeMax = 30; /* seconds, gets converted into milliseconds */ const fadeRangeDefault = 0; const fadeDefaultTime = 1; const fadeLiveDefaultTime = 0; const channelsPerRGBLight = 6; let groups = {}; let liveUpdate = false; lights.split("---").forEach(line => { let splits = line.split(";"); let groups_ = splits[3]; if (groups_.length === 0) return; groups_.split(",").forEach(group => { if (!groups[encodeURI(group)]) groups[encodeURI(group)] = []; groups[encodeURI(group)].push(line); }); }); /* const ws = new WebSocket(`ws://127.0.0.1:81`); */ const ws = new WebSocket(`ws://${window.location.hostname}/ws`); ws.onopen = () => { console.log("WS Opened") }; ws.onmessage = (m) => { console.log("WS Message", m) }; ws.onerror = (e) => { console.error("WS Error", e) }; class functions { static request = (u, options) => { return new Promise((resolve, reject) => { if (!/^http(s)*:\/{2}/i.test(u)) u = url.origin + u; options = (options ?? {}); fetch(u, options) .then(v => { resolve(v.body); }) .catch(reject); }); }; static submitConfig = (config) => { this.request("/config", { headers: { "plain": (typeof config !== "string" ? JSON.stringify(config) : config) }, method: "POST" }) .then(() => { console.debug("POST Config Success"); }) .catch(e => { console.error("POST Config error", e); }); }; static getWhite = (hex) => { if (hex.replace(/^#/, "").toLowerCase() === "ffffff") return 255; return 0; }; static hexToRGBParsed = (hex) => { /* let w = functions.getWhite(hex);  let rgb = functions.hexToRGB(hex); */ return functions.hexToRGB(hex); let r = { /* ...(w === 255 ? { r: 0, g: 0, b: 0 } : rgb), w: w */ }; return r; }; static hexToRGB = (hex) => { let r = {}; let hexParts = hex.replace(/^#/, "").match(/\w{2}/g); hexParts.forEach((a, i) => r[["r", "g", "b"][i]] = parseInt(a, 16)); return r; }; static RGBToHex = (r, g, b, w) => { let hex = "#"; /* if (w && (w == 255)) return "#ffffff"; */ hex += functions.pad2(Math.abs(r).toString(16)); hex += functions.pad2(Math.abs(g).toString(16)); hex += functions.pad2(Math.abs(b).toString(16)); return hex; }; static pad2 = (n) => { if (!/^\d$/.test(n)) return `${(n.length < 2 ? "0" : "")}${n}`; return `${(parseInt(n) < 10 ? "0" : "")}${n}`; }; static getValuesFromChannels = (containerID) => { let r = {}; channels.forEach(channel => { r[channel[1]] = document.getElementById(`${containerID}-channels-${channel[1]}`).value; }); return r; }; static appendValuesFromChannels = (containerID) => { let vals = functions.getValuesFromChannels(containerID); let [r, g, b] = [vals.r, vals.g, vals.b]; let hex = functions.RGBToHex(r, g, b); document.getElementById(`${containerID}-color`).value = hex; return hex; }; static appendHexToChannels = (containerID, hex) => { let hex_ = (hex ?? document.getElementById(`${containerID}-color`).value); let rgbw = functions.hexToRGBParsed(hex_); let r = {}; Object.keys(rgbw).forEach(channel => { document.getElementById(`${containerID}-channels-${channel}`).value = rgbw[channel]; r[channel] = rgbw[channel]; }); return r; }; static appendToChannels = (containerID, pickedChannels) => { Object.keys(pickedChannels).forEach(channel => { document.getElementById(`${containerID}-channels-${channel}`).value = pickedChannels[channel]; }); }; static updateLight = (addr, dimm, r, g, b, w, a, u, fade) => { let args = [addr, r, g, b, w, a, u, ((fade ?? 0) * 1000)]; let argsParsed = [addr, ...args.slice(1, 8).map(arg => (Math.floor(parseInt(arg) * (1 - ((dimm ?? 0) / 100)))))]; let data = [].concat(argsParsed, Array(8).slice(0, (8 - argsParsed.length))); ws.send(data.join(";")); }; static updateLightnoRGB = (addr, dimm, fade) => { ws.send([addr, (Math.floor(255 * (1 - (dimm / 100)))), ...[...Array(5)].map(a => 0), (fade * 1000)].join(";")); }; }; class scripts { static loadLights = () => { lights.split("---").forEach((line, i) => { let [lightName, lightDmxAddress, lightChannels, lightGroupName] = line.split(";"); let isRGB = (parseInt(lightChannels) > 1); let controlsContainer = document.createElement("div"); controlsContainer.id = `controls${lightDmxAddress}`; controlsContainer.classList.add("controlsContainer"); let controlsTitle = document.createElement("h"); controlsTitle.classList.add("controlsTitle"); controlsTitle.innerText = lightName; let controlsGroupTitle = document.createElement("h"); controlsGroupTitle.classList.add("controlsGroupTitle"); controlsGroupTitle.innerText = lightGroupName.split(",").join(", "); let controlsColorInput = elements.controlsColorInput(controlsContainer.id); let controlsChannelInputs = elements.controlsChannelInputs(controlsContainer.id); let controlsDimmerInput = elements.controlsDimmerInput(controlsContainer.id); let controlsFadeInput = elements.controlsFadeInput(controlsContainer.id); controlsColorInput.onchange = () => { append(); functions.appendHexToChannels(controlsContainer.id) }; controlsChannelInputs.onchange = () => { append(); functions.appendValuesFromChannels(controlsContainer.id) }; controlsDimmerInput.onchange = () => { append() }; controlsFadeInput.onchange = () => { append() }; let controlsSubmit = document.createElement("button"); controlsSubmit.innerText = "Update"; controlsSubmit.classList.add("controlsSubmit"); controlsSubmit.onclick = () => { append(true) }; function append(update) { let pickedDimm = controlsDimmerInput.querySelector(".controlsDimmerInput").value; let pickedFade = controlsFadeInput.querySelector(".controlsFadeInput").value; if (!isRGB) { if (update || liveUpdate) functions.updateLightnoRGB(lightDmxAddress, pickedDimm, pickedFade); return; }; let pickedColorHex = controlsColorInput.querySelector(".controlsColorInput").value; let pickedColor = functions.hexToRGBParsed(pickedColorHex); let pickedChannels = functions.getValuesFromChannels(controlsContainer.id); if (update || liveUpdate) functions.updateLight(lightDmxAddress, pickedDimm, ...Object.values(pickedChannels), pickedFade); }; [controlsTitle, controlsGroupTitle, ...(isRGB ? [controlsColorInput, controlsChannelInputs] : []), controlsDimmerInput, controlsFadeInput, controlsSubmit].forEach(a => controlsContainer.appendChild(a)); document.querySelector("#lightcontrols").appendChild(controlsContainer); }); }; static loadLightGroups = () => { let groupControlsContainer = document.createElement("div"); groupControlsContainer.classList.add("groupControlsContainer"); groupControlsContainer.id = "controlsGroup"; let groupControlsTitle = document.createElement("h"); groupControlsTitle.innerText = "Group Controls"; groupControlsTitle.classList.add("controlsTitle"); let groupSelector = document.createElement("select"); Object.keys(groups).forEach(group => { let groupOption = document.createElement("option"); groupOption.innerText = decodeURI(group); groupOption.value = group; groupSelector.appendChild(groupOption); }); let groupControlsColorInput = elements.controlsColorInput(groupControlsContainer.id); let groupControlsChannelInputs = elements.controlsChannelInputs(groupControlsContainer.id); let groupControlsDimmerInput = elements.controlsDimmerInput(groupControlsContainer.id); let groupControlsFadeInput = elements.controlsFadeInput(groupControlsContainer.id); let groupControlsSubmit = document.createElement("button"); groupControlsSubmit.innerText = "Update"; groupControlsSubmit.classList.add("controlsSubmit"); groupControlsSubmit.onclick = () => { append(true) }; groupControlsColorInput.onchange = () => { append(false, true, false) }; groupControlsChannelInputs.onchange = () => { append(false, false, true) }; groupControlsDimmerInput.onchange = () => { append() }; groupControlsFadeInput.onchange = () => { append() }; function append(update, fromPicker, fromChannels) { let pickedColorHex = groupControlsColorInput.querySelector(".controlsColorInput").value; let pickedDimm = groupControlsDimmerInput.querySelector(".controlsDimmerInput").value; let pickedFade = groupControlsFadeInput.querySelector(".controlsFadeInput").value; let pickedGroup = groupSelector.value; if (fromPicker) functions.appendHexToChannels(groupControlsContainer.id, pickedColorHex); if (fromChannels) pickedColorHex = functions.appendValuesFromChannels(groupControlsContainer.id); let pickedChannels = functions.getValuesFromChannels(groupControlsContainer.id); if (update || liveUpdate) functions.updateLight(groups[pickedGroup].filter(a => parseInt(a.split(";")[2]) > 1).map(a => a.split(";")[1]).join(","), pickedDimm, ...Object.values(pickedChannels), pickedFade); groups[pickedGroup].map(a => a.split(";")[1]).forEach(light => { let l = lights.split("---").filter(a => a.split(";")[1] === light)[0]?.split(";"); let i = l?.[1]; if (i === undefined) return; if (!document.getElementById(`controls${i}`)) return; let isRGB = (parseInt(l[2]) > 1); document.getElementById(`controls${i}-dimm`).value = pickedDimm; document.getElementById(`controls${i}-fade`).value = pickedFade; if (!isRGB) return; document.getElementById(`controls${i}-color`).value = pickedColorHex; functions.appendToChannels(`controls${i}`, pickedChannels); }); }; [groupControlsTitle, groupSelector, groupControlsColorInput, groupControlsChannelInputs, groupControlsDimmerInput, groupControlsFadeInput, groupControlsSubmit].forEach(a => groupControlsContainer.appendChild(a)); document.querySelector("#lightcontrolsgroup").appendChild(groupControlsContainer); }; static loadLightAll = () => { let allControlsContainer = document.createElement("div"); allControlsContainer.classList.add("groupControlsContainer"); allControlsContainer.id = "controlsGroupAll"; let allControlsTitle = document.createElement("h"); allControlsTitle.innerText = "Master Controls"; allControlsTitle.classList.add("controlsTitle"); let allControlsColorInput = elements.controlsColorInput(allControlsContainer.id); let allControlsChannelInputs = elements.controlsChannelInputs(allControlsContainer.id); let allControlsDimmerInput = elements.controlsDimmerInput(allControlsContainer.id); let allControlsFadeInput = elements.controlsFadeInput(allControlsContainer.id); let allControlsSubmit = document.createElement("button"); allControlsSubmit.innerText = "Update"; allControlsSubmit.classList.add("controlsSubmit"); allControlsSubmit.onclick = () => { append(true) }; allControlsColorInput.onchange = () => { append(false, true, false) }; allControlsChannelInputs.onchange = () => { append(false, false, true) }; allControlsDimmerInput.onchange = () => { append() }; allControlsFadeInput.onchange = () => { append() }; function append(update, fromPicker, fromChannels) { let pickedColorHex = allControlsColorInput.querySelector(".controlsColorInput").value; let pickedDimm = allControlsDimmerInput.querySelector(".controlsDimmerInput").value; let pickedFade = allControlsFadeInput.querySelector(".controlsFadeInput").value; if (fromPicker) functions.appendHexToChannels(allControlsContainer.id, pickedColorHex); if (fromChannels) pickedColorHex = functions.appendValuesFromChannels(allControlsContainer.id); let pickedChannels = functions.getValuesFromChannels(allControlsContainer.id); if (update || liveUpdate) functions.updateLight(lights.split("---").filter(a => parseInt(a.split(";")[2]) > 1).map(a => a.split(";")[1]).join(","), pickedDimm, ...Object.values(pickedChannels), pickedFade); lights.split("---").map(a => a.split(";")).map(light => { let i = light[1]; if (!document.getElementById(`controls${i}`)) return; let isRGB = (parseInt(light[2]) > 1); document.getElementById(`controls${i}-dimm`).value = pickedDimm; document.getElementById(`controls${i}-fade`).value = pickedFade; if (!isRGB) return; document.getElementById(`controls${i}-color`).value = pickedColorHex; functions.appendToChannels(`controls${i}`, pickedChannels); }); if (!document.getElementById(`controlsGroup`)) return; document.getElementById(`controlsGroup-color`).value = pickedColorHex; document.getElementById(`controlsGroup-dimm`).value = pickedDimm; document.getElementById(`controlsGroup-fade`).value = pickedFade; functions.appendToChannels(`controlsGroup`, pickedChannels); }; [allControlsTitle, allControlsColorInput, allControlsChannelInputs, allControlsDimmerInput, allControlsFadeInput, allControlsSubmit].forEach(a => allControlsContainer.appendChild(a)); document.querySelector("#lightcontrolsgroup").appendChild(allControlsContainer); }; static loadLiveUpdate = () => { let liveUpdateContainer = document.createElement("div"); liveUpdateContainer.id = "liveUpdateContainer"; let liveUpdateTitle = document.createElement("h"); liveUpdateTitle.id = "liveUpdateTitle"; liveUpdateTitle.innerText = "Live Update"; let liveUpdateCheckbox = document.createElement("input"); liveUpdateCheckbox.type = "checkbox"; liveUpdateCheckbox.onchange = () => { liveUpdate = liveUpdateCheckbox.checked; scripts.appendLiveUpdate() }; [liveUpdateTitle, liveUpdateCheckbox].forEach(a => liveUpdateContainer.appendChild(a)); document.querySelector("body").appendChild(liveUpdateContainer); }; static appendLiveUpdate = () => { let updateButtons = document.querySelectorAll(".controlsSubmit"); updateButtons.forEach(a => { if (liveUpdate) a.classList.add("hidden"); else a.classList.remove("hidden"); }); let updateFades = document.querySelectorAll(".controlsFade"); updateFades.forEach(a => { if (liveUpdate) a.value = fadeLiveDefaultTime; else a.value = fadeDefaultTime; }); }; }; class elements { static br = () => { return document.createElement("br") }; static spacer = () => { return document.createElement("spacer") }; static controlsColorInput = (containerID) => { let controlsColorPickerContainer = document.createElement("div"); controlsColorPickerContainer.classList.add("controlsColorPickerContainer"); let controlsColorPickerTitle = document.createElement("h"); controlsColorPickerTitle.innerText = "Color Picker"; let controlsColorPicker = document.createElement("input"); if (containerID) controlsColorPicker.id = `${containerID}-color`; controlsColorPicker.classList.add("controlsColorInput"); controlsColorPicker.type = "color"; controlsColorPicker.defaultValue = "#000000"; [controlsColorPickerTitle, controlsColorPicker].forEach(a => controlsColorPickerContainer.appendChild(a)); return controlsColorPickerContainer; }; static controlsChannelInputs = (containerID) => { let channelsContainer = document.createElement("div"); channelsContainer.classList.add("controlsChannelsContainer"); let channelsContainerInner = document.createElement("div"); let channelsTitle = document.createElement("h"); channelsTitle.innerText = "Channels"; let channelsInputContainer = document.createElement("div"); if (containerID) channelsInputContainer.id = `${containerID}-channels`; channelsInputContainer.classList.add("controlsChannelsInputContainer"); let channelTitlesContainer = document.createElement("div"); if (containerID) channelTitlesContainer.id = `${containerID}-channel-titles`; channelTitlesContainer.classList.add("controlsChannelTitlesContainer"); channels.forEach(channel => { let channelInput = document.createElement("input"); if (containerID) channelInput.id = `${channelsInputContainer.id}-${channel[1]}`; channelInput.classList.add("controlsChannelInput", `controlsChannel-${channel[1]}`, "vertical"); channelInput.type = "range"; channelInput.style.backgroundColor = channel[2]; channelInput.min = rangeInputMin; channelInput.max = rangeInputMax; channelInput.defaultValue = channel[3]; let channelTitle = document.createElement("h"); channelTitle.classList.add("controlsChannelTitle"); channelTitle.title = channel[0]; channelTitle.innerText = channel[1]; channelsInputContainer.appendChild(channelInput); channelTitlesContainer.appendChild(channelTitle); }); [channelsTitle, channelsInputContainer, channelTitlesContainer].forEach(a => channelsContainerInner.appendChild(a)); channelsContainer.appendChild(channelsContainerInner); return channelsContainer; }; static controlsDimmerInput = (containerID) => { let controlsDimmerContainer = document.createElement("div"); controlsDimmerContainer.classList.add("controlsDimmerContainer"); let controlsDimmerContainerInner = document.createElement("div"); let controlsDimmerTitle = document.createElement("h"); controlsDimmerTitle.innerText = "Dimmer"; let controlsDimmerInput = document.createElement("input"); if (containerID) controlsDimmerInput.id = `${containerID}-dimm`; controlsDimmerInput.classList.add("dimmer"); controlsDimmerInput.classList.add("controlsDimmerInput", "horizontal"); controlsDimmerInput.type = "range"; controlsDimmerInput.min = rangeInputMin; controlsDimmerInput.max = 100; controlsDimmerInput.defaultValue = rangeInputDefault; [controlsDimmerTitle, controlsDimmerInput].forEach(a => controlsDimmerContainerInner.appendChild(a)); controlsDimmerContainer.appendChild(controlsDimmerContainerInner); return controlsDimmerContainer; }; static controlsFadeInput = (containerID) => { let fadeContainer = document.createElement("div"); fadeContainer.classList.add("controlsFadeContainer"); let fadeContainerInner = document.createElement("div"); let fadeTitle = document.createElement("h"); fadeTitle.innerText = "Fade"; let fadeInput = document.createElement("input"); fadeInput.id = `${containerID}-fade`; fadeInput.classList.add("controlsFadeInput", "horizontal"); fadeInput.type = "range"; fadeInput.min = fadeRangeMin; fadeInput.max = fadeRangeMax; fadeInput.defaultValue = fadeRangeDefault; [fadeTitle, fadeInput].forEach(a => fadeContainerInner.appendChild(a)); fadeContainer.appendChild(fadeContainerInner); return fadeContainer; }; }; window.onload = () => { scripts.loadLights(); scripts.loadLightGroups(); scripts.loadLightAll(); scripts.loadLiveUpdate() }; </script> <style> * { scrollbar-width: thin; font-family: "Trebuchet MS", "Lucida Sans Unicode", "Lucida Grande", "Lucida Sans", Arial, sans-serif; border-radius: 10px; word-break: normal; line-break: anywhere; text-align: center; user-select: none; padding: 3px; } .hidden { display: none; } @media screen and (-webkit-min-device-pixel-ratio:0) { .controlsChannelInput { overflow: hidden; /* width: 80px; */ appearance: none; -webkit-appearance: none; background-color: #fff; border: 1px solid; } .controlsChannelInput::-webkit-slider-runnable-track { height: 10px; appearance: none; -webkit-appearance: none; color: #fff; background-color: transparent; margin-top: -1px; } .controlsChannelInput::-webkit-slider-thumb { width: 10px; -webkit-appearance: none; height: 10px; border: 1px solid white; border-radius: 100%; background: transparent; /* box-shadow: -87px 0 2px 90px #fcca46; */ box-shadow: 396px 0 2px 400px #000000; } } .horizontal { cursor: w-resize; } .vertical { cursor: n-resize; } .controlsChannelInput { border: 1px solid white; background-color: #000000; } .controlsChannelInput::-moz-range-progress { background-color: #fcca46; } .controlsChannelInput::-moz-range-track { background-color: #000000; } /* .dimmer { background-color: white; } */ body { background-color: #233d4d; transition: ease-in 2s; color: #f1faee; padding: 3px; } #lightcontrols, #lightcontrolsgroup { display: flex; place-items: center; flex-wrap: wrap; justify-content: center; align-items: stretch; } .controlsContainer { display: flex; flex: 1 0 24%; } .controlsContainer, .groupControlsContainer { max-width: 23%; padding: 6px; margin: 2px; width: 25%; gap: 3px; /* display: inline-block; */ border: 1px solid; background-color: #1d2d44; display: flex; flex-direction: column; align-items: center; justify-content: space-evenly; } /* #lightcontrolsgroup { display: flex; justify-content: center; } */ .groupControlsContainer { max-width: none; width: 40%; } spacer { display: flex; height: 10px; } .controlsTitle { font-size: large; } .controlsDimmerInput, .controlsFadeInput { width: 80%; } .controlsColorInput, .controlsSubmit { border: none; outline: none; } .controlsColorInput { background-color: transparent; box-sizing: content-box; padding: 0; } .controlsSubmit { padding: 4px; } .controlsGroupTitle { font-size: small; } .controlsColorPickerContainer, .controlsChannelsContainer, .controlsDimmerContainer, .controlsFadeContainer { display: flex; flex-wrap: wrap; justify-content: center; align-items: center; border: 1px solid; flex-direction: column; width: 100%; } .controlsDimmerContainer>div, .controlsFadeContainer>div { display: flex; flex-direction: column; justify-content: center; align-items: center; width: 90%; } .controlsChannelsContainer { flex-direction: column; flex-wrap: nowrap; /* height: 218px; */ } .controlsColorPickerContainer { min-width: 205px; } .controlsChannelsInputContainer { display: flex; flex-wrap: wrap; justify-content: center; flex-direction: column; transform: rotate(-90deg); /* row-gap: 3px; */ row-gap: 7.5px; min-width: 160px; /* width: min-content; */ } .controlsChannelTitle { /* display: flex; */ cursor: help; } .controlsChannelTitlesContainer { display: flex; gap: 10px; justify-content: space-evenly; } .controlsColorPickerContainer { flex-direction: column; justify-content: center; } #liveUpdateContainer { background-color: #38a3a5; z-index: 10; position: absolute; right: 10px; top: 10px; } </style>)rawliteral";

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
    Serial.print("Peer 1: ");
    if (result1 == ESP_OK)
    {
        Serial.println("Sent with success");
    }
    else
    {
        Serial.println("Error sending the data");
    }

    Serial.print("Peer 2: ");
    if (result2 == ESP_OK)
    {
        Serial.println("Sent with success");
    }
    else
    {
        Serial.println("Error sending the data");
    }
    delay(10);
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
                        dmx.write(addrChanged[j], red);
                        dmx.write(addrChanged[j] + 1, green);
                        dmx.write(addrChanged[j] + 2, blue);
                        dmx.write(addrChanged[j] + 3, white);
                        dmx.write(addrChanged[j] + 4, am);
                        dmx.write(addrChanged[j] + 5, uv);
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
        outputDMX();
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
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
    delay(2000);

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

  if (currentMillis - previousMillis >= led_intv) {
    previousMillis = currentMillis;
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }
    digitalWrite(greenLEDPin, ledState);
  }
}