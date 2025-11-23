#include <Arduino.h>
#include <ArduinoJson.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <deque>
#include <algorithm> // Required for std::min

#include "sierra_wifi_defs.h"

#define version_str "2025_11_23: v2.2: Changed to ESPAsyncWebServer."
//#define MOBILE true

#ifdef MOBILE
    int IP_last_field = DHT22_MOBILE_TEMP_SERVER_IP_LAST_FIELD;
    String dns_name   = DHT22_MOBILE_TEMP_SERVER_HOSTNAME;
    #define DHTPIN1    4      // DHT sensor 1.  ESP12: D2
    #define DHTPIN2    5      // DHT sensor 2.  ESP12: D1
    #define DHTPIN3    14     // DHT sensor 3.  ESP12: D5
#else
    int IP_last_field = DHT22_PORCH_TEMP_SERVER_IP_LAST_FIELD;
    String dns_name   = DHT22_PORCH_TEMP_SERVER_HOSTNAME;
    #define DHTPIN1    4      // DHT sensor 1.  ESP12: D2
    #define DHTPIN2    5      // DHT sensor 2.  ESP12: D1
    #define DHTPIN3    14     // DHT sensor 3.  ESP12: D5
#endif

/*
**  Network variables...
*/
IPAddress ip(IP1, IP2, IP3, IP_last_field);  // make sure IP is *outside* of DHCP pool range
IPAddress gateway(GW1, GW2, GW3, GW4);
IPAddress subnet(SN1, SN2, SN3, SN4);
IPAddress DNS(DNS1, DNS2, DNS3, DNS4);
const char* ssid     = SSID;
const char* password = WIFI_PW;
int server_port = 80;
String DNS_name = dns_name;

const int8_t num_sensors = 3;

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

bool online = false;
const long interval_ms = 5000;
const long record_interval_ms = 1000 * 60 * 5; // record every 5 min
const long wifi_interval_ms = 5000;
const long ntp_interval_ms = 1000 * 3600; // 1 hour
unsigned long previousRecordedMillis = interval_ms;
unsigned long previousMillis = interval_ms;
unsigned long previousWiFiMillis = wifi_interval_ms;
unsigned long previousNTPMillis = ntp_interval_ms;

struct sensor_data_t {
    String timestamp;
    float humidity[num_sensors];
    float temperature[num_sensors];
};

std::deque<sensor_data_t> sensor_data;
const int16_t num_data_pts = (300); 

// forward declarations
void setupOnline();
bool wifi_init(uint32_t waitTime_ms);
void updateSensorData();
String buildJSONData(uint16_t num_pts);
void handleRoot();
void purgeData();

String buildTimeDateStr();
String CreateTempDisplayHTML();
String CreateRootHTML();

// Create web server and set port number
AsyncWebServer server(server_port);

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
        humidity[i] = -99;
        temperature[i] = -99;
    }

    online = wifi_init(5000);
    if (!online)
        Serial.println("Could not connect to WiFi.  Running in offline mode.");
    else
        setupOnline();

    updateSensorData();
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval_ms) 
    {
        previousMillis = currentMillis; 

        updateSensorData();

        if ((sensor_data.size() == 0) ||
            (currentMillis - previousRecordedMillis >= record_interval_ms))
        {
            previousRecordedMillis = currentMillis;

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
        }

        String data = (online?"online, pts=":"offline, pts=") + String(sensor_data.size()) + ": " + buildJSONData(1);
        Serial.println(data);
    }

    if (online)
    {
        // Listen for HTTP requests from clients
        //server.handleClient(); 

        // update time with NTP
        if (currentMillis - previousNTPMillis >= ntp_interval_ms) 
        {
            previousNTPMillis = currentMillis;
            timeClient.update();
        }
    }
    else
    {
        if (currentMillis - previousWiFiMillis >= wifi_interval_ms) 
        {
            previousWiFiMillis = currentMillis;

            Serial.println("Trying to connect to wifi again...");
            online = wifi_init(5000);
            if (!online)
                Serial.println("Could not connect to WiFi.  Running in offline mode.");
            else
                setupOnline();
        }
    }
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

void setupOnline() {
    Serial.println("Starting up time client");
    timeClient.begin();
    int GMTOffset = -6;  // -5 for Mar-Oct, -6 Nov-Mar
    timeClient.setTimeOffset(GMTOffset * 3600);
    delay(1000);
    timeClient.update();
    Serial.println(timeClient.getFormattedTime());

    // setup routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        handleRoot();
        request->send(200, "text/html", CreateRootHTML());
    });
    server.on("/display_data", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println(" - Handling request for display_data...");
        request->send(200, "text/html", CreateTempDisplayHTML());
    });
    server.on("/get_data", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/text", buildJSONData(1));
    });
    server.on("/get_data_all", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println(" - Handling request for get_data...");
        request->send(200, "text/text", buildJSONData(num_data_pts));
    });
    server.on("/purge", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println(" - Clearing recorded data...");
        sensor_data.clear();
        request->send(200, "text/plain", "Sensor data purged");
    });

    // Start server
    server.begin();
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
    String timeDateStr = String(currentYear) + "-";
    if (currentMonth < 10)
        timeDateStr += "0" + String(currentMonth) + "-";
    else
        timeDateStr += String(currentMonth) + "-";
    if (currentDay < 10)
        timeDateStr += "0" + String(currentDay) + "T";
    else
        timeDateStr += String(currentDay) + "T";
    timeDateStr += timeClient.getFormattedTime();

    return timeDateStr;
}

/*
    Read humidity[num_sensors] and temperature[num_sensors] globals and put
    together JSON string like this:

    [{"timestamp":"2025-11-15T17:18:25","temp":[77.18,77,77.18],"humidity":[58.4,59.7,58.6]},
     {"timestamp":"2025-11-15T17:18:25","temp":[77,77,77.18],"humidity":[58.4,59.8,58.6]},
    ...
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

        val["timestamp"] = sensor_data[ptIdx].timestamp;

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

void handleRoot() {
    Serial.println(" - Handling request for get_data...");
    Serial.print("getFreeHeap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("getHeapFragmentation: ");
    Serial.println(ESP.getHeapFragmentation());
    Serial.print("getMaxFreeBlockSize: ");
    Serial.println(ESP.getMaxFreeBlockSize());
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
    String get_data_URL     = "http://" + WiFi.localIP().toString()+":"+String(server_port) + "/get_data";
    String get_data_all_URL = "http://" + WiFi.localIP().toString()+":"+String(server_port) + "/get_data_all";
    String display_data_URL = "http://" + WiFi.localIP().toString()+":"+String(server_port) + "/display_data";
    String purge_data_URL   = "http://" + WiFi.localIP().toString()+":"+String(server_port) + "/purge";

    String ptr = "";
    ptr += "<!DOCTYPE html> <html>\n";
    ptr += "<style>\n";
    ptr += "table, th, td {font-size: 14px;border: 1px solid;border-collapse: collapse;padding: 5px;}\n";
    ptr += "</style>\n";
    ptr += "<body> <h1>Welcome to Sierra Temp/Humidity widget</h1>\n";
    ptr += "<p style=\"font-size: 20px;\">To display latest temp/humidity data in browser: <a href="+display_data_URL+">"+display_data_URL+"</a></p>\n";
    ptr += "<p style=\"font-size: 20px;\">To get latest temp/humidity data as JSON       : <a href="+get_data_URL+">"+get_data_URL+"</a></p>\n";
    ptr += "<p style=\"font-size: 20px;\">To get ALL temp/humidity data as JSON          : <a href="+get_data_all_URL+">"+get_data_all_URL+"</a></p>\n";
    ptr += "<p style=\"font-size: 20px;\">To purge all data                              : <a href="+purge_data_URL+">"+purge_data_URL+"</a></p>\n";
    ptr += "<p style=\"font-size: 16px;\"> ESP Debug Data: </p>\n";
    ptr += "<p style=\"font-size: 16px;\"> Version str: " + String(version_str) + "</p>\n";
    ptr += "<table><tbody><tr><td><strong>Param</strong></td><td><strong>Value</strong></td></tr>\n";
    ptr += "<tr><td>Timestamp</td><td>" + buildTimeDateStr() + "</td></tr>\n";
    ptr += "<tr><td>Port</td><td>" + String(server_port) + "</td></tr>\n";
    ptr += "<tr><td>FreeHeap</td><td>" + String(ESP.getFreeHeap()) + "</td></tr>\n";
    ptr += "<tr><td>MaxFreeBlockSize</td><td>" + String(ESP.getMaxFreeBlockSize()) + "</td></tr>\n";
    ptr += "<tr><td>HeapFragmentation</td><td>" + String(ESP.getHeapFragmentation()) + "</td></tr>\n";
    ptr += "<tr><td>Recorded data pts</td><td>" + String(sensor_data.size()) + "</td></tr>\n";
    ptr += "<tr><td>Max data pts</td><td>" + String(num_data_pts) + "</td></tr>\n";
    ptr += "<tr><td>Record interval (ms)</td><td>" + String(record_interval_ms) + "</td></tr>\n";
    ptr += "</tbody></table>\n";
    ptr += "</body> </html>\n";

    return ptr;
}
