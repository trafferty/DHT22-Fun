#include <Arduino.h>
#include <ArduinoJson.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <deque>
#include <algorithm> // Required for std::min

#include "sierra_wifi_defs.h"

#define version_str "v2.0: Deploy version"

const int8_t num_sensors = 3;

#define DHTPIN1    4      // DHT sensor 1.  ESP12: D2
#define DHTPIN2    5      // DHT sensor 2.  ESP12: D1
#define DHTPIN3    14     // DHT sensor 3.  ESP12: D5
#define DHTTYPE    DHT22  // DHT 22 (AM2302)

DHT_Unified dht[] = {
  {DHTPIN1, DHT22},
  {DHTPIN2, DHT22},
  {DHTPIN3, DHT22},
};

const int8_t outside_S1 = 0;      // Index of outside sensor 1
const int8_t outside_S2 = 1;      // Index of outside sensor 2

float humidity[num_sensors];
float temperature[num_sensors];

struct sensor_data_t {
    String timestamp;
    float humidity[num_sensors];
    float temperature[num_sensors];
};

std::deque<sensor_data_t> sensor_data;
const int8_t num_data_pts = 20; 

/*
**  Network variables...
*/
IPAddress ip(IP1, IP2, IP3, DHT22_MOBILE_TEMP_SERVER_IP_LAST_FIELD);  // make sure IP is *outside* of DHCP pool range
IPAddress gateway(GW1, GW2, GW3, GW4);
IPAddress subnet(SN1, SN2, SN3, SN4);
IPAddress DNS(DNS1, DNS2, DNS3, DNS4);
const char* ssid     = SSID;
const char* password = WIFI_PW;
int server_port = 8088;
String DNS_name = DHT22_MOBILE_TEMP_SERVER_HOSTNAME;

bool online = false;
const long interval_ms = 5000;     // 5 seconds
const long ntp_interval_ms = 1000 * 3600; // 1 hour
unsigned long previousMillis = interval_ms;
unsigned long previousNTPMillis = ntp_interval_ms;

// forward declarations
bool wifi_init(uint32_t waitTime_ms);
void updateSensorData();
String buildJSONData(uint16_t num_pts);
void handleRoot();
void handleGetData();
void handleGetDataFull();
void handleDisplayData();
String buildTimeDateStr();
String CreateTempDisplayHTML();
String CreateRootHTML();

// Create web server and set port number
ESP8266WebServer server(server_port);

WiFiUDP ntpUDP;
// By default 'pool.ntp.org' is used with 60 seconds update interval
NTPClient timeClient(ntpUDP);

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.print("\nDHT22 TempServer Version: ");
    Serial.println(version_str);

    for (int i = 0; i < num_sensors; i++) 
    {
        dht[i].begin();
        humidity[i] = 0;
        temperature[i] = 0;
    }

    online = wifi_init(5000);
    if (!online)
    {
        Serial.println("Could not connect to WiFi.  Running in offline mode.");
    }
    else
    {
        timeClient.begin();
        int GMTOffset = -6;  // -5 for Mar-Oct, -6 Nov-Mar
        timeClient.setTimeOffset(GMTOffset * 3600);

        Serial.println("Starting up time client");
        delay(1000);
        timeClient.update();
        Serial.println(timeClient.getFormattedTime());

        // setup routes
        server.on("/", []() {
            handleRoot();
            server.send(200, "text/html", CreateRootHTML());
        });
        server.on("/get_data", handleGetData);
        server.on("/get_data_full", handleGetDataFull);
        server.on("/display_data", []() {
            server.send(200, "text/html", CreateTempDisplayHTML());
        });

        // Start server
        server.begin();
        Serial.print("http server started at: ");
        Serial.println(server.uri());
    }
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval_ms) 
    {
        previousMillis = currentMillis; 

        updateSensorData();

        sensor_data_t latest_data;
        for (int i = 0; i < num_sensors; i++) 
        {
            latest_data.temperature[i] = temperature[i];
            latest_data.humidity[i] = humidity[i];
            latest_data.timestamp = buildTimeDateStr();
        }
        sensor_data.push_front(latest_data);

        if (sensor_data.size() > num_data_pts)
            sensor_data.pop_back();

        String data = buildJSONData(1);
        Serial.println(data);
    }

    if (online)
    {
        // Listen for HTTP requests from clients
        server.handleClient(); 

        if (currentMillis - previousMillis >= ntp_interval_ms) 
        {
            timeClient.update();
            previousNTPMillis = currentMillis;
        }
    }
}

void updateSensorData() {
    for (int i = 0; i < num_sensors; i++) 
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

String buildTimeDateStr()
{
    time_t epochTime = timeClient.getEpochTime();
    //Get a time structure
    struct tm *ptm = gmtime ((time_t *)&epochTime); 
    int currentMonth = ptm->tm_mon+1;
    int currentDay = ptm->tm_mday;
    int currentYear = ptm->tm_year+1900;
    int currentHour =((epochTime  % 86400L) / 3600);
    int currentMin  =((epochTime  % 3600) / 60);
    int currentSec  = (epochTime  % 60);

    String timeDateStr = String(currentYear) + "-";
    timeDateStr += String(currentMonth) + "-";
    timeDateStr += String(currentDay) + "T";
    timeDateStr += String(currentHour) + ":";
    timeDateStr += String(currentMin) + ":";
    timeDateStr += String(currentSec);
    return timeDateStr;
}

/*
    Read humidity[num_sensors] and temperature[num_sensors] globals and put
    together JSON string like this:

    "{'timestamp': '2025-08-26T18:10:55', 
      'values': [
        {'temp': [-2.06, 15.67, 31.81], 'humidity': [50.0, 49.1, 45.7]},
        {'temp': [-2.06, 15.67, 31.81], 'humidity': [50.0, 49.1, 45.7]},
        {'temp': [-2.06, 15.67, 31.81], 'humidity': [50.0, 49.1, 45.7]}
      ]}"
*/
String buildJSONData(uint16_t num_pts) {
    // Allocate a temporary JsonDocument
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    
    //JsonArray values = doc["values"].to<JsonArray>();

    uint16_t pts_to_get = std::min(num_pts, (uint16_t)sensor_data.size());

    for (uint16_t ptIdx = 0; ptIdx < pts_to_get; ptIdx++)
    {
        JsonDocument val;

        val["timestamp"] = buildTimeDateStr();

        // Create the "temp" array
        JsonArray tempVals = val["temp"].to<JsonArray>();
        // Create the "humidity" array
        JsonArray humidityVals = val["humidity"].to<JsonArray>();

        for (int sensorIdx = 0; sensorIdx < num_sensors; sensorIdx++) 
        {
            tempVals.add(float(sensor_data[ptIdx].temperature[sensorIdx]));
            humidityVals.add(float(sensor_data[ptIdx].humidity[sensorIdx]));
        }
        array.add(val);
    }

    String jsonStr;
    serializeJson(doc, jsonStr);
    //Serial.println(jsonStr);

    return jsonStr;
}

bool wifi_init(uint32_t waitTime_ms)
{
    Serial.print("Setting up network with static IP.");
    WiFi.config(ip, gateway, subnet, DNS);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    // Connect to Wi-Fi network with SSID and password
    Serial.printf("Connecting to %s\n", ssid);
    uint32_t start_ts = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(200);
        if (millis() - start_ts > waitTime_ms)
            return false;
    }
    Serial.println("WiFi connected.  Setting up address.");
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Fail connecting");
        delay(waitTime_ms);
        ESP.restart();
    }
    Serial.print("WiFi connected. IP address: ");
    Serial.println(WiFi.localIP().toString());
    return true;
}

void handleRoot() {
    Serial.println(" - Handling request for get_data...");
    Serial.print("getFreeHeap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("getHeapFragmentation: ");
    Serial.println(ESP.getHeapFragmentation());
    Serial.print("getMaxFreeBlockSize: ");
    Serial.println(ESP.getMaxFreeBlockSize());
}

void handleGetData() {
    Serial.println(" - Handling request for get_data...");
    String data = buildJSONData(1);
    server.send(200, "text/plain", data.c_str());
}

void handleGetDataFull() {
    Serial.println(" - Handling request for get_data...");
    String data = buildJSONData(num_data_pts);
    server.send(200, "text/plain", data.c_str());
}

void handleDisplayData() {
    Serial.println(" - Handling request for display_data...");
    String data = CreateTempDisplayHTML();
    server.send(200, "text/plain", data);
}

String CreateTempDisplayHTML()
{
    float outside_temp     = (temperature[outside_S1] + temperature[outside_S2]) / 2.0; 
    float outside_humidity = (humidity[outside_S1] + humidity[outside_S2]) / 2.0;

    String ptr = "";
    ptr += "<!DOCTYPE html> <html>\n";
    ptr += "<style>\n";
    ptr += "table, th, td {font-size: 14px;border: 1px solid;border-collapse: collapse;padding: 5px;}\n";
    ptr += "</style>\n";
    ptr += "<body> <h1>Sierra Temps</h1>\n";
    ptr += "<p style=\"font-size: 24px;\"> Outside Temperature: <strong>" + String(outside_temp, 1) + " degF</strong></p>\n";
    ptr += "<p style=\"font-size: 24px;\"> Outside Humidity   : <strong>" + String(outside_humidity, 1) + "%</strong></p>\n";
    ptr += "<p style=\"font-size: 24px;\"> Timestamp          : <strong>" + buildTimeDateStr() + "</strong></p>\n";
    ptr += "<p style=\"font-size: 16px;\"> Individual Sensor Data: </p>\n";
    ptr += "<table><tbody><tr><td><strong>ID</strong></td><td><strong>Location</strong></td><td><strong>Temp (degF)</strong></td><td><strong>Humidity (%)</strong></td></tr>\n";
    ptr += "<tr><td>T0</td><td>Outside</td><td>" + String(temperature[0], 2) + "</td><td>" + String(humidity[0], 2) + "%</td></tr>\n";
    ptr += "<tr><td>T1</td><td>Outside</td><td>" + String(temperature[1], 2) + "</td><td>" + String(humidity[1], 2) + "%</td></tr>\n";
    ptr += "<tr><td>T2</td><td>Inside Enclosure</td><td>" + String(temperature[2], 2) + "</td><td>" + String(humidity[2], 2) + "%</td></tr>\n";
    ptr += "</tbody></table>\n";
    ptr += "</body> </html>\n";

    return ptr;
}

String CreateRootHTML()
{
    String ptr = "";
    ptr += "<!DOCTYPE html> <html>\n";
    ptr += "<style>\n";
    ptr += "table, th, td {font-size: 14px;border: 1px solid;border-collapse: collapse;padding: 5px;}\n";
    ptr += "</style>\n";
    ptr += "<body> <h1>Welcome to Sierra Temp/Humidity widget</h1>\n";
    ptr += "<p style=\"font-size: 20px;\">To get temp/humidity data as JSON: <strong>http://" + WiFi.localIP().toString()+":"+String(server_port) + "/get_data</strong></p>\n";
    ptr += "<p style=\"font-size: 20px;\">To get temp/humidity data as HTML: <strong>http://" + WiFi.localIP().toString()+":"+String(server_port) + "/display_data</strong></p>\n";
    ptr += "<p style=\"font-size: 16px;\"> ESP Debug Data: </p>\n";
    ptr += "<table><tbody><tr><td><strong>Param</strong></td><td><strong>Value</strong></td></tr>\n";
    ptr += "<tr><td>Timestamp</td><td>" + buildTimeDateStr() + "</td></tr>\n";
    ptr += "<tr><td>Port</td><td>" + String(server_port) + "</td></tr>\n";
    ptr += "<tr><td>FreeHeap</td><td>" + String(ESP.getFreeHeap()) + "</td></tr>\n";
    ptr += "<tr><td>HeapFragmentation</td><td>" + String(ESP.getHeapFragmentation()) + "</td></tr>\n";
    ptr += "<tr><td>MaxFreeBlockSize</td><td>" + String(ESP.getMaxFreeBlockSize()) + "</td></tr>\n";
    ptr += "</tbody></table>\n";
    ptr += "</body> </html>\n";

    return ptr;
}
