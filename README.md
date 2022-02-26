# ESP210 Environmental MQTT Sensor

Built for the ESP210 board based on a ESP8266 microcontroller. 

Settings in the Arduino IDE for code compilation and upload:
* Board: Generic ESP8266 Module
* Freq: 80 Mhz (use 160 MHz for TLS according to https://nofurtherquestions.wordpress.com/2016/03/14/making-an-esp8266-web-accessible/)
* Flash size: 4MB (any configuration)

Uses libraries:
* Adafruit_BME280 library
* hp_BH1750 - https://github.com/Starmbi/hp_BH1750
* PubSubClient https://github.com/knolleary/pubsubclient
* ESP8266 core: http://arduino.esp8266.com/stable/package_esp8266com_index.json