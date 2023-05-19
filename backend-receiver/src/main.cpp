/*
This code is a firmware running on a receiver ESP32 module that enables the 
control of DMX-Lights (RGB + W + STROBE) remotely. It allows communication with the
 master ESP32 module hosting a web interface, enabling the user to control lights 
 that are not directly connected to the master module. 
 The firmware facilitates seamless coordination between the master and receiver modules, 
 enhancing the flexibility and convenience of controlling multiple DMX-Lights.
 */

// ESP-NOW code adapted from https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/

#include <esp_now.h>
#include <WiFi.h>
#include <ESPDMX.h>

DMXESPSerial dmx;

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message
{
  int addr[3];
  int r;
  int g;
  int b;
  int w;
  int s;
} struct_message;

struct_message DMX_Data;


void outputDMX(int addr[], int r = 0, int g = 0, int b = 0, int w = 0, int s = 0)
{
  for (int i = 0; i < (sizeof(addr) / sizeof(int)); i++)
  {
    dmx.write(addr[i], r);
    dmx.write(addr[i] + 1, g);
    dmx.write(addr[i] + 2, b);
  }
  dmx.update();
}

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  memcpy(&DMX_Data, incomingData, sizeof(DMX_Data));
  Serial.print("length: ");
  Serial.println(len);
  Serial.print("addr0: ");
  Serial.println(DMX_Data.addr[0]);
  Serial.print("addr1: ");
  Serial.println(DMX_Data.addr[1]);
  Serial.print("addr2: ");
  Serial.println(DMX_Data.addr[2]);
  Serial.print("r: ");
  Serial.println(DMX_Data.r);
  Serial.print("g: ");
  Serial.println(DMX_Data.g);
  Serial.print("b: ");
  Serial.println(DMX_Data.b);
  Serial.print("w: ");
  Serial.println(DMX_Data.w);
  Serial.print("s: ");
  Serial.println(DMX_Data.s);

  outputDMX(DMX_Data.addr, DMX_Data.r, DMX_Data.g, DMX_Data.b, DMX_Data.s, DMX_Data.w);
}


void setup()
{
  // Initialize Serial Monitor
  Serial.begin(115200);

  dmx.init(512);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  Serial.println(WiFi.macAddress());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
}