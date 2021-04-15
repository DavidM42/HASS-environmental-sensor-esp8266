# HASS-environmental-sensor-esp8266

This project consists of a PlatformIO project to measure Temperature, humidity and CO² concentration in a room and send those values via MQTT to Homeassistant.

## How to

1. Download/Clone this repo
2. Copy `config.h.example` into `config.h` and change the values according to your Wifi and MQTT server used
2. Flash the sketch onto your Wemos D1 Mini (or other Esp-8266 if you change the pins used)
3. Connect the DHT11 and MH-Z19B sensors
4. Add the content of `HomeassistantConfigAdditions.yaml` to your Homeassistant configuration
5. Restart Homeassistant & Power the Esp-8266

<!-- TODO more info and maybe blog post -->