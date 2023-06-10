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

lightState DMX_Data;


// callback function that will be executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
  memcpy(&DMX_Data, incomingData, sizeof(DMX_Data));

  dmx.write(DMX_Data.start_address, DMX_Data.red);
  dmx.write(DMX_Data.start_address + 1, DMX_Data.green);
  dmx.write(DMX_Data.start_address + 2, DMX_Data.blue);
  dmx.write(DMX_Data.start_address + 3, DMX_Data.white);
  dmx.write(DMX_Data.start_address + 4, DMX_Data.amber);
  dmx.write(DMX_Data.start_address + 5, DMX_Data.uv);
  dmx.update();

  Serial.print("addr: ");
  Serial.println(DMX_Data.start_address);
  Serial.print("r: ");
  Serial.println(DMX_Data.red);
  Serial.print("g: ");
  Serial.println(DMX_Data.green);
  Serial.print("b: ");
  Serial.println(DMX_Data.blue);
  Serial.print("w: ");
  Serial.println(DMX_Data.white);
  Serial.print("am: ");
  Serial.println(DMX_Data.amber);
  Serial.print("uv: ");
  Serial.println(DMX_Data.uv);

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