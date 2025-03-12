#include <Arduino.h>
#include <Wire.h> //i2c haberleşmesi için
#include "MAX30105.h"
#include <WiFi.h>
#include <ArduinoJson.h>

#define SECRET_SSID "SUPERONLINE_WiFi_2704"		// replace SSID with your WiFi network name
#define SECRET_PASS "HATWK4CHPVAW"	         // replace Password with your WiFi password
char ssid[] = SECRET_SSID;   // WiFi SSID
char pass[] = SECRET_PASS;   // WiFi Password
WiFiClient client;

MAX30105 particleSensor;

const byte RATE_SIZE = 4; // Ortalama hesaplamak için kullanılacak örnek sayısı
byte rates[RATE_SIZE];    // Kalp atış hızlarını saklamak için dizi
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;              //gönderilecek veriler 1 ortalama nabız 

unsigned long previousMillis = 0;
const long interval = 20000; // upload intervali

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
}
// gsr example kodundan da buraya gsr değerini return eden fonksiyon eklenecek 
double GSR(){
  return(1000);
 }

//HR fonksiyonu burada sadece ortalama nabzı hesaplayıyor
double HR(){
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
  return(beatAvg);
}
// wifi bağlantısını kontrol eden kodu direkt loopun içine yazmak yerine bağlantıyı kontrol eden bir fonksiyon oluşturduk
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Reconnecting to WiFi...");
    WiFi.begin(ssid, pass);
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 10) { // 10 kere denesin
      delay(1000);
      Serial.print(".");
      attempt++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected!");
    } else {
      Serial.println("Failed to connect.");
    }
  }
  
}
void loop() {

  StaticJsonDocument<200> data;
  data["HR"] = HR();
  data["GSR"] = GSR();

  String jsonString;
  serializeJson(data, jsonString);

  checkWiFiConnection();
  unsigned long currentMillis = millis();

}//json halledilmiş