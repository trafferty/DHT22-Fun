#include <Arduino.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define NUM_SENSORS 3

#define DHTPIN1    4     // Digital pin connected to the DHT sensor 
#define DHTPIN2    5     // Digital pin connected to the DHT sensor 
#define DHTPIN3    14     // Digital pin connected to the DHT sensor 
#define DHTTYPE    DHT22     // DHT 22 (AM2302)

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

void setup() {
    Serial.begin(9600);

    for (int i = 0; i < NUM_SENSORS; i++) 
    {
        dht[i].begin();
        humidity[i] = 0;
        temperature[i] = 0;
    }

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