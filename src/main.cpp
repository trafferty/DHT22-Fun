#include <Arduino.h>
#include <ArduinoJson.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <sierra_wifi_defs.h>

#define version_str "v1.0 (adding network and webserver)"

#define NUM_SENSORS 3

#define DHTPIN1    4     // Digital pin connected to the DHT sensor 
#define DHTPIN2    5     // Digital pin connected to the DHT sensor 
#define DHTPIN3    14     // Digital pin connected to the DHT sensor 
#define DHTTYPE    DHT22     // DHT 22 (AM2302)

DHT_Unified dht[] = {
  {DHTPIN1, DHT22},
  {DHTPIN2, DHT22},
  {DHTPIN3, DHT22},
};

float humidity[NUM_SENSORS];
float temperature[NUM_SENSORS];

/*
**  Network variables...
*/
IPAddress ip(IP1, IP2, IP3, DHT22_TEMP_SERVER_IP_LAST_FIELD);  // make sure IP is *outside* of DHCP pool range
IPAddress gateway(GW1, GW2, GW3, GW4);
IPAddress subnet(SN1, SN2, SN3, SN4);
IPAddress DNS(DNS1, DNS2, DNS3, DNS4);
const char* ssid     = SSID;
const char* password = WIFI_PW;
int server_port = 8088;
String DNS_name = DHT22_TEMP_SERVER_HOSTNAME;

uint32_t delayMS;

// forward declarations
void wifi_init();
void updateSensorData();
String buildJSONData();
void handleRoot();
void handleGetData();

// Create web server and set port number
ESP8266WebServer server(server_port);

WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval
NTPClient timeClient(ntpUDP);

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.print("\nDHT22 TempServer Version: ");
    Serial.println(version_str);

    for (int i = 0; i < NUM_SENSORS; i++) 
    {
        dht[i].begin();
        humidity[i] = 0;
        temperature[i] = 0;
    }

    wifi_init();

    timeClient.begin();
    int GMTOffset = -5;
    timeClient.setTimeOffset(GMTOffset * 3600);

    Serial.println("Starting up time client");
    delay(1000);
    timeClient.update();
    Serial.println(timeClient.getFormattedTime());

    // setup routes
    server.on("/", handleRoot);
    server.on("/get_data", handleGetData);

    // Start server
    server.begin();
    Serial.print("http server started at: ");
    Serial.println(server.uri());

    delayMS = 2000;
}
void loop() {

#if 0
    // Delay between measurements.
    delay(delayMS);

    updateSensorData();

    String data = buildJSONData();
    Serial.println(data);
#endif

    // Listen for HTTP requests from clients
    server.handleClient(); 
}

void updateSensorData() {
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
}

/*
    Read humidity[NUM_SENSORS] and temperature[NUM_SENSORS] globals and put
    together JSON string like this:

    "{'timestamp': '2025-08-26T18:10:55.379061', 'temp': [-2.06, 15.67, 31.81], 'humidity': [50.0, 49.1, 45.7]}"

*/
String buildJSONData() {
    // Allocate a temporary JsonDocument
    JsonDocument doc;

    doc["timestamp"] = timeClient.getFormattedTime();

    // Create the "temp" array
    JsonArray tempVals = doc["temp"].to<JsonArray>();
    // Create the "humidity" array
    JsonArray humidityVals = doc["humidity"].to<JsonArray>();

    for (int i = 0; i < NUM_SENSORS; i++) 
    {
        tempVals.add(float(temperature[i]));
        humidityVals.add(float(humidity[i]));
    }

    String jsonStr;
    serializeJson(doc, jsonStr);
    //Serial.println(jsonStr);

    return jsonStr;
}

void wifi_init()
{
    Serial.print("Setting up network with static IP.");
    WiFi.config(ip, gateway, subnet, DNS);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    // Connect to Wi-Fi network with SSID and password
    Serial.printf("Connecting to %s", ssid);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(200);
    }
    Serial.println();
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Fail connecting");
        delay(5000);
        ESP.restart();
    }
    Serial.print("WiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
}

void handleRoot() {
  server.send(404, "text/plain", "404: Not found");   // Send HTTP status 200 (Ok) and send some text to the browser/client

  Serial.print("getFreeHeap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("getHeapFragmentation: ");
  Serial.println(ESP.getHeapFragmentation());
  Serial.print("getMaxFreeBlockSize: ");
  Serial.println(ESP.getMaxFreeBlockSize());
 
}

void handleGetData() {
    //Serial.println(" - Handling request for data...");
    updateSensorData();
    String data = buildJSONData();
    server.send(200, "text/plain", data.c_str());
}