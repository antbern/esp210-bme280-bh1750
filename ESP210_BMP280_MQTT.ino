
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <hp_BH1750.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Wifi and MQTT configuration
#include "wifi_config.h"

// Wifi and MQTT objects
WiFiClient esp_client;
PubSubClient client(esp_client);

// Sensor objects
Adafruit_BME280 bme;
hp_BH1750 bh1750; 


// used to send messages after interval
unsigned long lastMsg = 0;

// the default interval between measurements
unsigned long interval = 5000;


// the buffer used to format the JSON output to send
#define MSG_BUFFER_SIZE  (256)
char msg[MSG_BUFFER_SIZE];

const char* topic_environment = "esp210/environment";
const char* topic_interval = "esp210/interval";

const int led = 5;

void setup() {
  // Start Serial
  Serial.begin(115200);

  // Set up pins
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);


  // Connect to WiFi
  WiFi.mode(WIFI_STA);
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

  if(!bh1750.begin(BH1750_TO_GROUND)) {
    Serial.println(F("Could not find a valid BH1750 sensor, check wiring!"));
    
    // blink rapidly and hang
    while(1){
      digitalWrite(led, HIGH);
      delay(100);
      digitalWrite(led, LOW);
      delay(100);
    }
  }

  bh1750.calibrateTiming();
  
  digitalWrite(led, LOW);

  // setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
}

void reconnect_mqtt() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID

    // Attempt to connect
    if (client.connect(mqtt_device_name)) {
      Serial.println("connected");

      // do stuff when connected here (like subscribing to topics)
      
      client.subscribe(topic_interval);
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  // try to interpret the message as an unsigned long
  char *end;
  unsigned long i = strtoul((const char*)payload, &end, 10);

  if (end != (char*)payload && i >= 500) {
    // we managed to parse an unsigned long, use it as interval for now
    Serial.print("New interval received: ");
    Serial.println(i, DEC);
    
    interval = i;
  }
}

void loop() {
  if(!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  unsigned long now = millis();
  if(now - lastMsg > interval) {
    lastMsg = now;

    // read data: BH1750 first since it takes some time (~500ms)
    bh1750.start();
    float lux = bh1750.getLux();
    float temp = bme.readTemperature();
    float pressure = bme.readPressure();
    float humidity = bme.readHumidity();

    // format it to JSON
    snprintf(msg, MSG_BUFFER_SIZE, "{\"temperature\":%.2f,\"pressure\":%.2f,\"humidity\":%.2f,\"illuminance\":%.2f}", temp, pressure, humidity, lux);
    
    Serial.print("Publish message: ");
    Serial.println(msg);

    // publish message
    client.publish(topic_environment, msg);  

    // run auto adjustment of measurement parameters
    bh1750.adjustSettings(80);
  }

}
