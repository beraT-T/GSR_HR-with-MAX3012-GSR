#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include <WiFi.h>
#include <ArduinoJson.h>

#define SECRET_SSID "SUPERONLINE_WiFi_2704"		// replace SSID with your WiFi network name
#define SECRET_PASS "HATWK4CHPVAW"	// replace Password with your WiFi password

MAX30105 particleSensor;

char ssid[] = SECRET_SSID;   // WiFi SSID
char pass[] = SECRET_PASS;   // WiFi Password
WiFiClient client;

unsigned long previousMillis = 0;
const long interval = 20000; // 20 saniye

const byte RATE_SIZE = 4; // Ortalama hesaplamak için kullanılacak örnek sayısı
byte rates[RATE_SIZE]; // Kalp atış hızlarını saklamak için dizi
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 was not found. Please check wiring/power.");
    while (1);
  }

  byte ledBrightness = 0x1F;
  byte sampleAverage = 8;
  byte ledMode = 3;
  int sampleRate = 100;
  int pulseWidth = 411;
  int adcRange = 4096;

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  WiFi.mode(WIFI_STA);
  // ThingSpeak.begin(client);
}
// void GSR(){

// }
void HR(){
  long irValue = particleSensor.getIR();

  if (irValue > 50000) { // IR değeri belirli bir eşik değerini aşarsa kalp atışını algıla
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);
    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;
    }

    int sum = 0;
    for (byte i = 0; i < RATE_SIZE; i++) {
      sum += rates[i];
    }
    beatAvg = sum / RATE_SIZE;
  }

  Serial.print("BPM: ");
  Serial.println(beatAvg);
}
void loop() {
  HR();
  //GSR();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, pass);
      Serial.print(".");
      delay(5000);
    }
    Serial.println("\nConnected.");
  }

  unsigned long currentMillis = millis();

}