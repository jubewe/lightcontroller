// Import required libraries
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDMX.h>
#include <ESPmDNS.h>

DMXESPSerial dmx;

// Replace with your network credentials
const char *ssid = "lightcontroller";
const char *password = "password";

bool ledState = 0;
const int ledPin = 2;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Lightcontroller</title>
</head>

<body>
    <h3>Lightcontroller</h3>
    <div id="lightcontrols"></div>
    <div id="lightcontrolsgroup"></div>
    <div id="lightcontrolsall"></div>
</body>

</html>
<script>
    const url = new URL(document.baseURI);

    // DMX Address Name;dmxAddressFixture (start)
    const lights = "SomeName0;0;group1\nSomeName1;1;group1";
    let groups = {};
    lights.split("\n").forEach(line => {
        let splits = line.split(";");
        if (!groups[splits[2]]) groups[splits[2]] = [];
        groups[splits[2]].push(line);
    });


    const ws = new WebSocket(`ws://${window.location.hostname}/ws`);
    ws.onopen = () => {
        console.log("WS Opened");
    };

    ws.onmessage = (m) => {
        console.log("WS Message", m);
    };

    ws.onerror = (e) => {
        console.error("WS Error", e);
    };

    class functions {
        static request = (u, options) => {
            return new Promise((resolve, reject) => {
                if (!/^http(s)*:\/{2}/i.test(u)) u = url.origin + u;
                options = (options ?? {});

                fetch(u, options)
                    .then(v => {
                        resolve(v.body);
                    })
                    .catch(reject);
            });
        };

        static getWhite = (hex) => {
            if (hex.replace(/^#/, "") === "FFFFFF") return 255;
            return 0;
        };

        static hexToRGBParsed = (hex) => {
            let w = functions.getWhite(hex);
            let rgb = functions.hexToRGB(hex);

            let r = {
                ...(w === 255 ? { r: 0, g: 0, b: 0 } : rgb),
                w: w
            };

            return r;
        };

        static hexToRGB = (hex) => {
            let r = {};
            let hexParts = hex.replace(/^#/, "").match(/\w{2}/g);
            hexParts.forEach((a, i) => r[["r", "g", "b"][i]] = parseInt(a, 16));
            return r;
        };

        static updateLight = (addr, dimm, r, g, b, w, str) => {
            let args = [addr, dimm, r, g, b, w, str];
            let data = [].concat(args, Array(7).slice(0, (7 - args.length)));
            console.log(data.join(";"));
            ws.send(data.join(";"));
        };
    };

    class scripts {
        static loadLights = () => {
            lights.split("\n").forEach((line, i) => {
                let [lightName, lightDmxAddress, lightGroupName] = line.split(";");

                let controlsContainer = document.createElement("div");
                controlsContainer.id = `controls${i}`;
                controlsContainer.classList.add("controlsContainer");

                let controlsTitle = document.createElement("h");
                controlsTitle.classList.add("controlsTitle");
                controlsTitle.innerText = lightName;

                let controlsGroupTitle = document.createElement("h");
                controlsGroupTitle.classList.add("controlsGroupTitle");
                controlsGroupTitle.innerText = lightGroupName;

                let controlsColorInput = elements.controlsColorInput(controlsContainer.id);
                let controlsDimmerInput = elements.controlsDimmerInput(controlsContainer.id);

                let controlsSubmit = document.createElement("button");
                controlsSubmit.innerText = "Update";
                controlsSubmit.onclick = () => {
                    let pickedColorHex = controlsColorInput.value;
                    let pickedColor = functions.hexToRGBParsed(pickedColorHex);
                    let pickedDimm = controlsDimmerInput.value;

                    functions.updateLight(lightDmxAddress, pickedDimm, pickedColor.r, pickedColor.g, pickedColor.b, pickedColor.w, 0);
                };

                [controlsTitle, elements.br(), controlsGroupTitle, elements.br(), elements.spacer(), controlsColorInput, elements.br(), controlsDimmerInput, elements.br(), controlsSubmit].forEach(a => controlsContainer.appendChild(a));

                document.querySelector("#lightcontrols").appendChild(controlsContainer);
            });
        };

        static loadLightGroups = () => {
            let groupControlsContainer = document.createElement("div");
            groupControlsContainer.classList.add("groupControlsContainer");
            groupControlsContainer.id = "controlsGroup";

            let groupControlsTitle = document.createElement("h");
            groupControlsTitle.innerText = "Group Controls";

            let groupSelector = document.createElement("select");
            Object.keys(groups).forEach(group => {
                let groupOption = document.createElement("option");
                groupOption.innerText = group;
                groupOption.value = group;
                groupSelector.appendChild(groupOption);
            });

            let groupControlsColorInput = elements.controlsColorInput("controlsGroup");
            let groupControlsDimmerInput = elements.controlsDimmerInput("controlsGroup");

            let groupControlsSubmit = document.createElement("button");
            groupControlsSubmit.innerText = "Update";
            groupControlsSubmit.onclick = () => { append(true) };
            groupControlsColorInput.onchange = () => { append() };
            groupControlsDimmerInput.onchange = () => { append() };

            function append(update) {
                let pickedColorHex = groupControlsColorInput.value;
                let pickedColor = functions.hexToRGBParsed(pickedColorHex);
                let pickedDimm = groupControlsDimmerInput.value;
                let pickedGroup = groupSelector.value;

                if (update) functions.updateLight(groups[pickedGroup].map(a => a.split(";")[1]).join(","), pickedDimm, pickedColor.r, pickedColor.g, pickedColor.b, pickedColor.w, 0);

                groups[pickedGroup].map(a => a.split(";")[1]).forEach(light => {
                    let lightIndex = lights.split("\n").map((a, i) => [a.split(";")[1], i]).filter(a => `${a[1]}` === light)[0]?.[1];
                    if (lightIndex === undefined) return;
                    if (!document.getElementById(`controls${lightIndex}`)) return;
                    document.getElementById(`controls${lightIndex}-color`).value = pickedColorHex;
                    document.getElementById(`controls${lightIndex}-dimm`).value = pickedDimm;
                });
            };

            [groupControlsTitle, elements.br(), elements.spacer(), groupSelector, elements.br(), elements.spacer(), groupControlsColorInput, elements.br(), groupControlsDimmerInput, elements.br(), groupControlsSubmit].forEach(a => groupControlsContainer.appendChild(a));

            document.querySelector("#lightcontrolsgroup").appendChild(groupControlsContainer);
        };

        static loadLightAll = () => {
            let allControlsContainer = document.createElement("div");
            allControlsContainer.classList.add("groupControlsContainer");
            allControlsContainer.id = "controlsGroupAll";

            let allControlsTitle = document.createElement("h");
            allControlsTitle.innerText = "Master Controls";

            let allControlsColorInput = elements.controlsColorInput("controlsGroupAll");
            let allControlsDimmerInput = elements.controlsDimmerInput("controlsGroupAll");

            let allControlsSubmit = document.createElement("button");
            allControlsSubmit.innerText = "Update";
            allControlsSubmit.onclick = () => { append(true) };
            allControlsColorInput.onchange = () => { append() };
            allControlsDimmerInput.onchange = () => { append() };

            function append(update) {
                let pickedColorHex = allControlsColorInput.value;
                let pickedColor = functions.hexToRGBParsed(pickedColorHex);
                let pickedDimm = allControlsDimmerInput.value;


                if (update) functions.updateLight(lights.split("\n").map(a => a.split(";")[1]).join(","), pickedDimm, pickedColor.r, pickedColor.g, pickedColor.b, pickedColor.w, 0);

                lights.split("\n").map((light, i) => {
                    if (!document.getElementById(`controls${i}`)) return;
                    document.getElementById(`controls${i}-color`).value = pickedColorHex;
                    document.getElementById(`controls${i}-dimm`).value = pickedDimm;
                });
                
                if (!document.getElementById(`controlsGroup`)) return;
                document.getElementById(`controlsGroup-color`).value = pickedColorHex;
                document.getElementById(`controlsGroup-dimm`).value = pickedDimm;
            };

            [allControlsTitle, elements.br(), elements.spacer(), allControlsColorInput, elements.br(), allControlsDimmerInput, elements.br(), allControlsSubmit].forEach(a => allControlsContainer.appendChild(a));

            document.querySelector("#lightcontrolsgroup").appendChild(allControlsContainer);
        };
    };

    class elements {
        static br = () => { return document.createElement("br") };
        static spacer = () => { return document.createElement("spacer") };

        static controlsColorInput = (containerId) => {
            let controlsColorInput = document.createElement("input");
            if (containerId) controlsColorInput.id = `${containerId}-color`;
            controlsColorInput.classList.add("controlsColorInput");
            controlsColorInput.type = "color";
            controlsColorInput.defaultValue = "#FFFFFF"

            return controlsColorInput;
        };

        static controlsDimmerInput = (containerId) => {
            let controlsDimmerInput = document.createElement("input");
            if (containerId) controlsDimmerInput.id = `${containerId}-dimm`;
            controlsDimmerInput.classList.add("controlsDimmerInput");
            controlsDimmerInput.type = "range";
            controlsDimmerInput.value = "0";
            controlsDimmerInput.min = "0";
            controlsDimmerInput.max = "255";
            controlsDimmerInput.defaultValue = "0";

            return controlsDimmerInput;
        };

    };

    window.onload = () => { scripts.loadLights(); scripts.loadLightGroups(); scripts.loadLightAll() };
</script>
<style>
    * {
        scrollbar-width: thin;
        font-family: "Trebuchet MS", "Lucida Sans Unicode", "Lucida Grande", "Lucida Sans", Arial, sans-serif;
        border-radius: 5px;
        word-break: normal;
        line-break: anywhere;
        text-align: center;
        user-select: none;
        padding: 3px;
    }

    body {
        background-color: #233d4d;
        transition: ease-in 2s;
        color: #f1faee;
        padding: 3px;
    }

    #lightcontrols {
        /* display: grid; */
        place-items: center;
    }

    .controlsContainer,
    .groupControlsContainer {
        padding: 3px;
        margin: 3px;
        width: 25%;
        display: inline-block;
        border: 1px solid;
        background-color: #1d2d44;
    }

    spacer {
        display: flex;
        height: 10px;
    }

    .controlsTitle {
        font-size: large;
    }

    .controlsDimmerInput {
        display: inline-block;
        width: 80%;
        /* transform: rotate(90deg); */
    }

    .controlsGroupTitle {
        font-size: small;
    }

    #lightcontrolsgroup div {
        vertical-align: top;
    }
</style>)rawliteral";

/*
void notifyClients()
{
  ws.textAll(String(ledState));
  Serial.println(ledState);
}
*/

void splitStringToIntegers(const String &data, int addr[], int &dimm, int &r, int &g, int &b, int &w, int &str)
{
  // Temporary variables to store intermediate values
  String tempAddr, tempDimm, tempR, tempG, tempB, tempW, tempStr;

  // Find the position of semicolons in the string
  int semicolon1 = data.indexOf(';');
  int semicolon2 = data.indexOf(';', semicolon1 + 1);
  int semicolon3 = data.indexOf(';', semicolon2 + 1);
  int semicolon4 = data.indexOf(';', semicolon3 + 1);
  int semicolon5 = data.indexOf(';', semicolon4 + 1);
  int semicolon6 = data.indexOf(';', semicolon5 + 1);

  // Extract individual substrings between semicolons
  tempAddr = data.substring(0, semicolon1);
  tempDimm = data.substring(semicolon1 + 1, semicolon2);
  tempR = data.substring(semicolon2 + 1, semicolon3);
  tempG = data.substring(semicolon3 + 1, semicolon4);
  tempB = data.substring(semicolon4 + 1, semicolon5);
  tempW = data.substring(semicolon5 + 1, semicolon6);
  tempStr = data.substring(semicolon6 + 1);

  // Convert the substrings to integers
  dimm = tempDimm.toInt();
  r = tempR.toInt();
  g = tempG.toInt();
  b = tempB.toInt();
  w = tempW.toInt();
  str = tempStr.toInt();

  // Split the 'tempAddr' string by commas
  // Find the position of commas in the string
  int comma1 = tempAddr.indexOf(',');
  int comma2 = tempAddr.indexOf(',', comma1 + 1);
  int comma3 = tempAddr.indexOf(',', comma2 + 1);

  // Extract individual substrings between commas
  addr[0] = tempAddr.substring(0, comma1).toInt();
  addr[1] = tempAddr.substring(comma1 + 1, comma2).toInt();
  addr[2] = tempAddr.substring(comma2 + 1, comma3).toInt();
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    Serial.println((char *)data);

    // [addr, dimm, r, g, b, w, str]

    int addr[3]; //number of different addresses
    int dimm, r, g, b, w, str;
    int address_size;

    splitStringToIntegers((char *)data, addr, dimm, r, g, b, w, str);

    // Print the extracted values
    Serial.print("Addresses: ");
    for (int i = 0; i < (sizeof(addr) / sizeof(int)); i++)
    {
      Serial.print(addr[i]);
      Serial.println();

      Serial.print("Dimm: ");
      Serial.println(dimm);

      Serial.print("R: ");
      Serial.println(r);

      Serial.print("G: ");
      Serial.println(g);

      Serial.print("B: ");
      Serial.println(b);

      Serial.print("W: ");
      Serial.println(w);

      Serial.print("String: ");
      Serial.println(str);
      dmx.write(addr[i], r);
      dmx.write(addr[i] + 1, g);
      dmx.write(addr[i] + 2, b);
    }
    dmx.update();
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
  /*
  //replace strings in the html
  Serial.println(var);
  if (var == "STATE")
  {
  }
  */
  return String();
}

void setup()
{
  // initialize dmx
  dmx.init(512);

  // Serial port for debugging purposes
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Serial.println("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", index_html, processor); });

  // Start server
  server.begin();
}

void loop()
{
  ws.cleanupClients();
}