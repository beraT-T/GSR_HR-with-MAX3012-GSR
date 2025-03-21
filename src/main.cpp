/**
 * @file main.cpp
 * @authors berat(berat_baki@outlook.com),
 * @brief
 * @version 0.1
 * @date 2025-03-13
 *
 * @copyright Copyright (c) 2025
 *
 */
#include <Arduino.h>
#include <Wire.h> //i2c haberleşmesi için
#include "MAX30105.h"
#if defined(ESP32)
// ESP32 specific code here
#include <WiFi.h>
#elif defined(ESP8266)
// ESP8266 specific code here
#include <ESP8266WiFi.h>
#endif
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define MQTT_BROKER "mqtt-dashboard.com"
#define MQTT_PORT 1883
#define MQTT_TOPIC "esmanur/healthData"
#define MQTT_USER ""
#define MQTT_PASS ""


#define SECRET_SSID "MrHOME"     // replace SSID with your WiFi network name
#define SECRET_PASS "3b4e162d8b" // replace Password with your WiFi password


WiFiClient wificlient;
PubSubClient mqttClient(wificlient);
// MAX30105 particleSensor;

const byte RATE_SIZE = 4; // Ortalama hesaplamak için kullanılacak örnek sayısı
byte rates[RATE_SIZE];    // Kalp atış hızlarını saklamak için dizi
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg; // gönderilecek veriler 1 ortalama nabız

byte ledBrightness = 0x1F;
byte sampleAverage = 8;
byte ledMode = 3;
int sampleRate = 100;
int pulseWidth = 411;
int adcRange = 4096;

unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
const long interval = 20000; // upload intervali

/*
mqttClientId olarak cihazın mac adresi
Global olarak  uniq :))
(000.. diye tanimladimki bir hata da en kotu varsayilan degeri olsun, neydi diye beynimizi yormayalim :)) )
*/
String mqttClientId = "000000000000";

/*
Bu kullanim degismis, warning uretiyordu :))
JsonDocument<200> data;
*/
JsonDocument data;
String jsonString;

void setupWiFi()
{
  Serial.print("Connecting to WiFi...");
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 10)
  {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  /*
  if deki kullanim hatalarina  dikkat ediyoruz :)))
  if (WiFi.status() != WL_CONNECTED)
  */
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Connected!");
    Serial.print("IP Adresi: ");
    Serial.println(WiFi.localIP()); // fazla bilgi den zarar gelmez :))
  }
  else
  {
    Serial.println("Failed to connect to WiFi");
  }
}

/*
mqttClientId degiskenini 1 kere burada tanimlamak daha mantikli olabilir :))
*/
void setMqttClientId(void)
{
  mqttClientId = WiFi.macAddress();
  mqttClientId.replace(":", ""); // degisken turunun string olmasinin verdigi avantajlardan biri :))
  Serial.print("MQTT Client ID: ");
  Serial.println(mqttClientId);
}
void setupMQTT()
{
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  setMqttClientId();
}

void connectToMQTT()
{

  while (!mqttClient.connected())
  {
    Serial.println("MQTT'ye bağlanılıyor...");
    if (mqttClient.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASS))
    {
      mqttClient.subscribe(MQTT_TOPIC);
      Serial.println("Bağlandı!");
    }
    else
    {
      Serial.print("Bağlantı başarısız, hata kodu: ");
      Serial.print(mqttClient.state());
      Serial.println(" 5 saniye sonra tekrar deneniyor...");
      delay(5000);
    }
  }
}

void checkMQTTConnection()
{
  if (!mqttClient.connected())
  {
    connectToMQTT(); // MQTT bağlantısını kontrol et
  }
}

void checkWiFiConnection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Wİfi connection lost. Reconnecting...");
    setupWiFi();
  }
}

// HR fonksiyonu burada sadece ortalama nabzı hesaplayıyor
//  double HR(){
//    long irValue = particleSensor.getIR();

//   if (irValue > 50000) { // IR değeri belirli bir eşik değerini aşarsa kalp atışını algıla
//     long delta = millis() - lastBeat;
//     lastBeat = millis();

//     beatsPerMinute = 60 / (delta / 1000.0);
//     if (beatsPerMinute < 255 && beatsPerMinute > 20) {
//       rates[rateSpot++] = (byte)beatsPerMinute;
//       rateSpot %= RATE_SIZE;
//     }

//     int sum = 0;
//     for (byte i = 0; i < RATE_SIZE; i++) {
//       sum += rates[i];
//     }
//     beatAvg = sum / RATE_SIZE;
//   }

//   Serial.print("BPM: ");
//   Serial.println(beatAvg);
//   return(beatAvg);
// }

void publishMessage()
{

  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;

    data.clear();

    data["HR"] = "x"; // HR();
    // GSR için de eklenecek

    serializeJson(data, jsonString); //?
    Serial.print("Publishing MQTT message: ");
    Serial.println(jsonString);
    if (mqttClient.publish(MQTT_TOPIC, jsonString.c_str()))
    {
      Serial.println(" Successfully.");
    }
    else
    {
      Serial.println(" Failed.");
    }
    // Serial.print("MQTT Mesajı Gönderildi "); boolean publish (topic, payload, [length], [retained]) fonksiyonu ile kontorl et
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Initializing...");

  // if (!particleSensor.begin(Wire, I2C_SPEED_FAST))
  // {
  //   Serial.println("MAX30105 was not found. Please check wiring/power.");
  //   while (1)
  //     ;
  // }
  // particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  setupWiFi();
  setupMQTT();
  connectToMQTT();
}

void loop()
{
  currentMillis = millis();
  checkWiFiConnection();
  checkMQTTConnection();
  publishMessage();  // topic, mesajın içeriği, qos fonksiyonun bağlantısı önce olması lazım mesajı attığımız fonksiyon sadece publish message setupa mqtt connect eklenecek
  mqttClient.loop(); // MQTT istemcisi döngüsü
}
