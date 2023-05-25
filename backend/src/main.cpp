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
uint8_t broadcastAddressReceiver1[] = {0xAC, 0x67, 0xB2, 0x2A, 0xD0, 0x68}; // AC:67:B2:2A:D0:68
uint8_t broadcastAddressReceiver2[] = {0xAC, 0x67, 0xB2, 0x2A, 0xD0, 0x68}; // AC:67:B2:2A:D0:68

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
typedef struct struct_message
{
    int addr[3];
    int r;
    int g;
    int b;
    int w;
    int s;
} struct_message;

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
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta http-equiv="X-UA-Compatible" content="IE=edge"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>Lightcontroller</title></head><body><h3>Lightcontroller</h3><div id="lightcontrols"></div><div id="lightcontrolsgroup"></div><div id="lightcontrolsall"></div></body></html><script>const url = new URL(document.baseURI);/* DMX Address Name;dmxAddressFixture (start) *//* const lights = "SomeName0;0;group1\nSomeName1;1;group1\nSomeName2;10;group3\nSomeName4;20;group2\nSomeName5;30"; */const lights = "Vorne Oben;1;group1---Vorne Mitte;7;group1---Vorne Rechts;14;group3";console.debug("Loaded config", lights);let groups = {};lights.split("---").forEach(line => {let splits = line.split(";");if (!groups[splits[2]]) groups[splits[2]] = [];groups[splits[2]].push(line);});const ws = new WebSocket(`ws://${window.location.hostname}/ws`);ws.onopen = () => {console.log("WS Opened");};ws.onmessage = (m) => {console.log("WS Message", m);};ws.onerror = (e) => {console.error("WS Error", e);};class functions {static request = (u, options) => {return new Promise((resolve, reject) => {if (!/^http(s)*:\/{2}/i.test(u)) u = url.origin + u;options = (options ?? {});fetch(u, options).then(v => {resolve(v.body);}).catch(reject);});};static submitConfig = (config) => {this.request("/config", {headers: {"plain": (typeof config !== "string" ? JSON.stringify(config) : config)},method: "POST"}).then(() => {console.debug("POST Config Success");}).catch(e => {console.error("POST Config error", e);});};static getWhite = (hex) => {if (hex.replace(/^#/, "") === "FFFFFF") return 255;return 0;};static hexToRGBParsed = (hex) => {let w = functions.getWhite(hex);let rgb = functions.hexToRGB(hex);let r = {...(w === 255 ? { r: 0, g: 0, b: 0 } : rgb),w: w};return r;};static hexToRGB = (hex) => {let r = {};let hexParts = hex.replace(/^#/, "").match(/\w{2}/g);hexParts.forEach((a, i) => r[["r", "g", "b"][i]] = parseInt(a, 16));return r;};static updateLight = (addr, dimm, r, g, b, w, str) => {let args = [addr, dimm, r, g, b, w, str];let data = [].concat(args, Array(7).slice(0, (7 - args.length)));ws.send(data.join(";"));};};class scripts {static loadLights = () => {lights.split("---").forEach((line, i) => {let [lightName, lightDmxAddress, lightGroupName] = line.split(";");let controlsContainer = document.createElement("div");controlsContainer.id = `controls${lightDmxAddress}`;controlsContainer.classList.add("controlsContainer");let controlsTitle = document.createElement("h");controlsTitle.classList.add("controlsTitle");controlsTitle.innerText = lightName;let controlsGroupTitle = document.createElement("h");controlsGroupTitle.classList.add("controlsGroupTitle");controlsGroupTitle.innerText = lightGroupName;let controlsColorInput = elements.controlsColorInput(controlsContainer.id);let controlsDimmerInput = elements.controlsDimmerInput(controlsContainer.id);let controlsSubmit = document.createElement("button");controlsSubmit.innerText = "Update";controlsSubmit.classList.add("controlsSubmit");controlsSubmit.onclick = () => {let pickedColorHex = controlsColorInput.value;let pickedColor = functions.hexToRGBParsed(pickedColorHex);let pickedDimm = controlsDimmerInput.value;functions.updateLight(lightDmxAddress, pickedDimm, pickedColor.r, pickedColor.g, pickedColor.b, pickedColor.w, 0);};[controlsTitle, controlsGroupTitle, controlsColorInput, controlsDimmerInput, controlsSubmit].forEach(a => controlsContainer.appendChild(a));document.querySelector("#lightcontrols").appendChild(controlsContainer);});};static loadLightGroups = () => {let groupControlsContainer = document.createElement("div");groupControlsContainer.classList.add("groupControlsContainer");groupControlsContainer.id = "controlsGroup";let groupControlsTitle = document.createElement("h");groupControlsTitle.innerText = "Group Controls";groupControlsTitle.classList.add("controlsTitle");let groupSelector = document.createElement("select");Object.keys(groups).forEach(group => {let groupOption = document.createElement("option");groupOption.innerText = group;groupOption.value = group;groupSelector.appendChild(groupOption);});let groupControlsColorInput = elements.controlsColorInput("controlsGroup");let groupControlsDimmerInput = elements.controlsDimmerInput("controlsGroup");let groupControlsSubmit = document.createElement("button");groupControlsSubmit.innerText = "Update";groupControlsSubmit.onclick = () => { append(true) };groupControlsSubmit.classList.add("controlsSubmit");groupControlsColorInput.onchange = () => { append() };groupControlsDimmerInput.onchange = () => { append() };function append(update) {let pickedColorHex = groupControlsColorInput.value;let pickedColor = functions.hexToRGBParsed(pickedColorHex);let pickedDimm = groupControlsDimmerInput.value;let pickedGroup = groupSelector.value;/* console.log("Append group", pickedColorHex, pickedColor, pickedDimm, pickedGroup); */if (update) functions.updateLight(groups[pickedGroup].map(a => a.split(";")[1]).join(","), pickedDimm, pickedColor.r, pickedColor.g, pickedColor.b, pickedColor.w, 0);groups[pickedGroup].map(a => a.split(";")[1]).forEach(light => {let lightAddress = lights.split("---").map(a => a.split(";")[1]).filter(a => a == light)[0];if (lightAddress === undefined) return;if (!document.getElementById(`controls${lightAddress}`)) return;document.getElementById(`controls${lightAddress}-color`).value = pickedColorHex;document.getElementById(`controls${lightAddress}-dimm`).value = pickedDimm;});};[groupControlsTitle, groupSelector, groupControlsColorInput, groupControlsDimmerInput, groupControlsSubmit].forEach(a => groupControlsContainer.appendChild(a));document.querySelector("#lightcontrolsgroup").appendChild(groupControlsContainer);};static loadLightAll = () => {let allControlsContainer = document.createElement("div");allControlsContainer.classList.add("groupControlsContainer");allControlsContainer.id = "controlsGroupAll";let allControlsTitle = document.createElement("h");allControlsTitle.innerText = "Master Controls";allControlsTitle.classList.add("controlsTitle");let allControlsColorInput = elements.controlsColorInput("controlsGroupAll");let allControlsDimmerInput = elements.controlsDimmerInput("controlsGroupAll");let allControlsSubmit = document.createElement("button");allControlsSubmit.innerText = "Update";allControlsSubmit.classList.add("controlsSubmit");allControlsSubmit.onclick = () => { append(true) };allControlsColorInput.onchange = () => { append() };allControlsDimmerInput.onchange = () => { append() };function append(update) {let pickedColorHex = allControlsColorInput.value;let pickedColor = functions.hexToRGBParsed(pickedColorHex);let pickedDimm = allControlsDimmerInput.value;/* console.log("Append all", pickedColorHex, pickedColor, pickedDimm); */if (update) functions.updateLight(lights.split("---").map(a => a.split(";")[1]).join(","), pickedDimm, pickedColor.r, pickedColor.g, pickedColor.b, pickedColor.w, 0);lights.split("---").map((light, i) => {if (!document.getElementById(`controls${i}`)) return;document.getElementById(`controls${i}-color`).value = pickedColorHex;document.getElementById(`controls${i}-dimm`).value = pickedDimm;});if (!document.getElementById(`controlsGroup`)) return;document.getElementById(`controlsGroup-color`).value = pickedColorHex;document.getElementById(`controlsGroup-dimm`).value = pickedDimm;};[allControlsTitle, allControlsColorInput, allControlsDimmerInput, allControlsSubmit].forEach(a => allControlsContainer.appendChild(a));document.querySelector("#lightcontrolsgroup").appendChild(allControlsContainer);};};class elements {static br = () => { return document.createElement("br") };static spacer = () => { return document.createElement("spacer") };static controlsColorInput = (containerId) => {let controlsColorInput = document.createElement("input");if (containerId) controlsColorInput.id = `${containerId}-color`;controlsColorInput.classList.add("controlsColorInput");controlsColorInput.type = "color";controlsColorInput.defaultValue = "#FFFFFF";return controlsColorInput;};static controlsDimmerInput = (containerId) => {let controlsDimmerInput = document.createElement("input");if (containerId) controlsDimmerInput.id = `${containerId}-dimm`;controlsDimmerInput.classList.add("controlsDimmerInput");controlsDimmerInput.type = "range";controlsDimmerInput.value = "0";controlsDimmerInput.min = "0";controlsDimmerInput.max = "255";controlsDimmerInput.defaultValue = "0";return controlsDimmerInput;};};window.onload = () => { scripts.loadLights(); scripts.loadLightGroups(); scripts.loadLightAll() };</script><style>* {scrollbar-width: thin;font-family: "Trebuchet MS", "Lucida Sans Unicode", "Lucida Grande", "Lucida Sans", Arial, sans-serif;border-radius: 5px;word-break: normal;line-break: anywhere;text-align: center;user-select: none;padding: 3px;}body {background-color: #233d4d;transition: ease-in 2s;color: #f1faee;padding: 3px;}#lightcontrols {display: flex;place-items: center;flex-wrap: wrap;justify-content: center;}.controlsContainer {display: flex;flex: 1 0 24%;}.controlsContainer,.groupControlsContainer {max-width: 23%;padding: 6px;margin: 2px;width: 25%;gap: 3px;/* display: inline-block; */border: 1px solid;background-color: #1d2d44;display: flex;flex-direction: column;align-items: center;justify-content: space-evenly;}#lightcontrolsgroup {display: flex;justify-content: center;}.groupControlsContainer {max-width: none;width: 40%;}spacer {display: flex;height: 10px;}.controlsTitle {font-size: large;}.controlsDimmerInput {display: inline-block;width: 80%;/* transform: rotate(90deg); */}.controlsColorInput,.controlsSubmit {border: none;}.controlsGroupTitle {font-size: small;}#lightcontrolsgroup div {vertical-align: top;}</style>)rawliteral";

void notifyClients()
{
    // ws.textAll(String(ledState));
    // Serial.println(ledState);
}

void splitStringToIntegers(const String &data, int addr[], int &dimm, int &r, int &g, int &b, int &w, int &am, int &uv)
{
    // Temporary variables to store intermediate values
    String tempAddr, tempDimm, tempR, tempG, tempB, tempW, tempAM, tempUV;

    Serial.println("data " + data);

    // Find the position of semicolons in the string
    int semicolon1 = data.indexOf(';');
    int semicolon2 = data.indexOf(';', semicolon1 + 1);
    int semicolon3 = data.indexOf(';', semicolon2 + 1);
    int semicolon4 = data.indexOf(';', semicolon3 + 1);
    int semicolon5 = data.indexOf(';', semicolon4 + 1);
    int semicolon6 = data.indexOf(';', semicolon5 + 1);
    int semicolon7 = data.indexOf(';', semicolon6 + 1);
    int semicolon8 = data.indexOf(';', semicolon7 + 1);

    // Extract individual substrings between semicolons
    tempAddr = data.substring(0, semicolon1) + ",";
    tempDimm = data.substring(semicolon1 + 1, semicolon2);
    tempR = data.substring(semicolon2 + 1, semicolon3);
    tempG = data.substring(semicolon3 + 1, semicolon4);
    tempB = data.substring(semicolon4 + 1, semicolon5);
    tempW = data.substring(semicolon5 + 1, semicolon6);
    tempAM = data.substring(semicolon6 + 1, semicolon7);
    tempUV = data.substring(semicolon7 + 1, semicolon8);
    fade = data.substring(semicolon8 + 1).toInt();

    // Convert the substrings to integers
    dimm = tempDimm.toInt();
    r = (tempR.toInt() * dimm) / 255;
    g = (tempG.toInt() * dimm) / 255;
    b = (tempB.toInt() * dimm) / 255;
    w = (tempW.toInt() * dimm) / 255;
    am = (tempAM.toInt() * dimm) / 255;
    uv = (tempUV.toInt() * dimm) / 255;

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
            Serial.println("addr: " + String(addr));
            Serial.println("red: " + String(r));
            Serial.println("green: " + String(g));
            Serial.println("blue: " + String(b) + " ...");
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
            DMX_Data.start_address = addr;
            DMX_Data.red = r;
            DMX_Data.green = g;
            DMX_Data.blue = b;
            DMX_Data.white = w;
            DMX_Data.amber = am;
            DMX_Data.uv = uv;

            /*
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
            }*/
            fade = 2;
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
            }
            else if (addr == addrChanged[0])
            {
                Serial.println(String(addr) + "-" + String(addrChanged[numAddrChanged-1]) + "fading");
                int rampDuration = fade * 1000;                   // miliseconds
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

                    for (int j = 0; j < numAddrChanged; j++)
                    {
                        // Calculate the current brightness value
                        dmx.write(addrChanged[j], r_old + (r - r_old) * progress);
                        dmx.write(addrChanged[j] + 1, g_old + (g - g_old) * progress);
                        dmx.write(addrChanged[j] + 2, b_old + (b - b_old) * progress);
                        dmx.write(addrChanged[j] + 3, w_old + (w - w_old) * progress);
                        dmx.write(addrChanged[j] + 4, am_old + (am - am_old) * progress);
                        dmx.write(addrChanged[j] + 5, uv_old + (uv - uv_old) * progress);
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

        splitStringToIntegers((char *)data, addr, dimm, r, g, b, w, am, uv);
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
}