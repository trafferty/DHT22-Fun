#include <Arduino.h>
#include <ArduinoJson.h>
#include <Time.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <sierra_wifi_defs.h>

#define NUM_SENSORS 3

#define DHTPIN1    4     // Digital pin connected to the DHT sensor 
#define DHTPIN2    5     // Digital pin connected to the DHT sensor 
#define DHTPIN3    14     // Digital pin connected to the DHT sensor 
#define DHTTYPE    DHT22     // DHT 22 (AM2302)

/*
**  Network variables...
*/
IPAddress ip(IP1, IP2, IP3, DHT22_TEMP_SERVER_IP_LAST_FIELD);  // make sure IP is *outside* of DHCP pool range
IPAddress gateway(GW1, GW2, GW3, GW4);
IPAddress subnet(SN1, SN2, SN3, SN4);
IPAddress DNS(DNS1, DNS2, DNS3, DNS4);
const char* ssid     = SSID;
const char* password = WIFI_PW;
int server_port = 80;
String DNS_name = DHT22_TEMP_SERVER_HOSTNAME;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

// DHT_Unified dht1(DHTPIN1, DHTTYPE);
// DHT_Unified dht2(DHTPIN2, DHTTYPE);
// DHT_Unified dht3(DHTPIN3, DHTTYPE);

DHT_Unified dht[] = {
  {DHTPIN1, DHT22},
  {DHTPIN2, DHT22},
  {DHTPIN3, DHT22},
};

float humidity[NUM_SENSORS];
float temperature[NUM_SENSORS];

uint32_t delayMS;

// Set web server port number
ESP8266WebServer server(server_port);

bool NTPTimeSet = false;
char ntpServerNamePrimary[] = "pool.ntp.org";
char ntpServerNameSecondary[] = "time.nist.gov";
char *ntpServerName = ntpServerNamePrimary;

WiFiUDP Udp;
unsigned int UdpPort = 8888;  // local port to listen for UDP packets

void setup() {
    Serial.begin(9600);

    for (int i = 0; i < NUM_SENSORS; i++) 
    {
        dht[i].begin();
        humidity[i] = 0;
        temperature[i] = 0;
    }

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi..");
    }

    // Print ESP32 Local IP Address
    Serial.println(WiFi.localIP());

    // Init and get the time
    Serial.println("configuring NTP...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    printLocalTime();

    // setup routes
    server.on("/get_data", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/plain", processData().c_str());
    });

    // Start server
    server.begin();

    delayMS = 2000;
}

void loop() {
    // Delay between measurements.
    delay(delayMS);

    for (int i = 0; i < NUM_SENSORS; i++) 
    {
        sensors_event_t event;
        dht[i].temperature().getEvent(&event);
        if (isnan(event.temperature))
            temperature[i] = -99;
        else
            temperature[i] = event.temperature * 1.8 + 32;

        dht[i].humidity().getEvent(&event);
        if (isnan(event.relative_humidity))
            humidity[i] = -99;
        else
            humidity[i] = event.relative_humidity;
    }

    Serial.print(F("Temp (F):     "));
    for (int i = 0; i < NUM_SENSORS; i++) 
    {
        Serial.print(F(" "));
        Serial.print(temperature[i]);
    }
    Serial.println(F(""));

    Serial.print(F("Humidity (%): "));
    for (int i = 0; i < NUM_SENSORS; i++) 
    {
        Serial.print(F(" "));
        Serial.print(humidity[i]);
    }
    Serial.println(F(""));
}

/*
    Read humidity[NUM_SENSORS] and temperature[NUM_SENSORS] globals and put
    together JSON string like this:

    "{'timestamp': '2025-08-26T18:10:55.379061', 'temp1': -2.06, 'temp2': 15.67, 'temp3': 31.81}"

*/
String processData() {
    StaticJsonDocument<200> doc;

    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        doc["timestamp"] = String(millis()/1000)
    } else {
        char timeString[20];
        strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S", &timeinfo);
        doc["timestamp"] = timeString
    }

    for (int i = 0; i < NUM_SENSORS; i++) 
    {
        String key = "temp" + (i+1)
        doc[key] = String(temperature[i]);
        key = "humidity" + (i+1)
        doc[key] = String(humidity[i]);
    }
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    Serial.println(jsonOutput);

    return jsonOutput;
}