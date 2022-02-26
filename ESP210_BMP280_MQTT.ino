
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <hp_BH1750.h>

#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <PubSubClient.h>

#include <BearSSLHelpers.h>
#include <time.h> // NOLINT(modernize-deprecated-headers)

// Wifi and MQTT configuration
#include "wifi_config.h"

// Wifi and MQTT objects
WiFiClientSecure esp_client;
PubSubClient client(esp_client);

// Sensor objects
Adafruit_BME280 bme;
hp_BH1750 bh1750; 


// used to send messages after interval
unsigned long lastMsg = 0;

// the default interval between measurements
unsigned long intervalMs = 5000;


// the buffer used to format the JSON output to send
#define MSG_BUFFER_SIZE  (256)
char msg[MSG_BUFFER_SIZE];


// the base topic to publish and listen to
#ifndef TOPIC_BASE
  #define TOPIC_BASE "esp210"
#endif

const char* topic_observation =  TOPIC_BASE "/observation";
const char* topic_interval = TOPIC_BASE "/interval";

const int led = 5;

BearSSL::X509List trusted_roots;

// The correct time is needed to validate the certificate
// See: https://github.com/cdzombak/esp8266-basic-wifi/blob/d6fba145102ecf68f41a4bf186c5a75e5175d155/
// and: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/examples/BearSSL_CertStore/BearSSL_CertStore.ino
// for source and more explanation.  
void setClock() {
    configTime(CFG_TZ, "pool.ntp.org", "time.nist.gov");

    Serial.printf_P(PSTR("%lu: Waiting for NTP time sync "), millis());
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(250);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.print(F("\r\n"));
    struct tm timeinfo; // NOLINT(cppcoreguidelines-pro-type-member-init)
    gmtime_r(&now, &timeinfo);
    Serial.printf_P(PSTR("Current time (UTC):   %s"), asctime(&timeinfo));
    localtime_r(&now, &timeinfo);
    Serial.printf_P(PSTR("Current time (Local): %s"), asctime(&timeinfo));
}

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

  setClock();

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
  
  // configure the WifiClientSecure with the root CA
  // from https://arduino-esp8266.readthedocs.io/en/3.0.2/esp8266wifi/bearssl-client-secure-class.html
  trusted_roots.append(mqtt_root_ca);
  esp_client.setTrustAnchors(&trusted_roots);

  // setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  digitalWrite(led, LOW);
  
}

void reconnect_mqtt() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID

    // Attempt to connect
    if (client.connect(mqtt_device_name, mqtt_username, mqtt_password)) {
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

  // convert topic and payload to strings for easier handling
  String topicstr(topic);

  // add null-termination in last place (ok as long as we are not near the end of the buffer this points to)
  payload[length] = '\0';
  String payloadstr((char*)payload);

  // parse the payload as an integer (long)
  long i = payloadstr.toInt();

  if (i >= 500) {
    // we managed to parse a long, use it as interval for now
    Serial.print("New interval received: ");
    Serial.println(i, DEC);
    
    intervalMs = i;
  }
}

void loop() {
  if(!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  unsigned long now = millis();
  if(now - lastMsg > intervalMs) {
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
    client.publish(topic_observation, msg);  

    // run auto adjustment of measurement parameters
    bh1750.adjustSettings(80);
  }

}
