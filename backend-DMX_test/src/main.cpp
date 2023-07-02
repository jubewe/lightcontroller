#include <Arduino.h>
#include <ESPDMX.h>

DMXESPSerial dmx;

void setup()
{
  dmx.init(512);
}

void loop()
{
  dmx.write(1, 125);
  dmx.write(1 + 1, 125);
  dmx.write(1 + 2, 125);
  dmx.write(1 + 3, 255);
  dmx.write(1 + 4, 255);
  dmx.write(1 + 5, 255);
  dmx.write(1 + 6, 0);
  dmx.write(1 + 7, 0);
  dmx.update();
}