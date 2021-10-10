

/***************************************************************************
  This is a library for the BMP280 humidity, temperature & pressure sensor

  Designed specifically to work with the Adafruit BMEP280 Breakout 
  ----> http://www.adafruit.com/products/2651

  These sensors use I2C or SPI to communicate, 2 or 4 pins are required 
  to interface.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit andopen-source hardware by purchasing products
  from Adafruit!

  Written by Limor Fried & Kevin Townsend for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
 ***************************************************************************/
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BME280.h>

#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
Adafruit_BME280 bme;

WiFiClient client;

// settings
#include "wifi_config.h"


// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

Adafruit_MQTT_Publish topic_temperature = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/temperature");
Adafruit_MQTT_Publish topic_pressure = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/pressure");
Adafruit_MQTT_Publish topic_humidity = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/humidity");

const int led = 5;

void setup() {
  // Start Serial
  Serial.begin(115200);

  // Set up pins
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);

  WiFi.mode(WIFI_STA);
  // Connect to WiFi
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    
    digitalWrite(led, HIGH);
    delay(250);
    digitalWrite(led, LOW);
    delay(250);
    
    Serial.print(".");
  }
  // Print Debug info
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

 
  // Start I2C and connect to sensor  
  Wire.begin(2,14);
  if (!bme.begin(0x76)) {  
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    
    // blink rapidly and hang
    while(1){
      digitalWrite(led, HIGH);
      delay(100);
      digitalWrite(led, LOW);
      delay(100);
    }    
  }
  
  digitalWrite(led, LOW);
}

void loop() {

  MQTT_connect();
  
    // connect to thingspeak and post data
    if (WiFi.status() == WL_CONNECTED) {

        // read data      
      float temp = bme.readTemperature();
      float pressure = bme.readPressure();
      float humidity = bme.readHumidity();

      topic_temperature.publish(temp);
      topic_pressure.publish(pressure);
      topic_humidity.publish(humidity);

      Serial.println("Published!");
      
    }
   // TODO: add deep sleep
   delay(5000);


     // ping the server to keep the mqtt connection alive
    // NOT required if you are publishing once every KEEPALIVE seconds
  
    if(! mqtt.ping()) {
      mqtt.disconnect();
    }
    
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}
