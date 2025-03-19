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
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_TOPIC "esmanur/healthData"
#define MQTT_USER "hivemq.webclient.1741827367846" 
#define MQTT_PASS "AS46kt3W0Hqp!RK%b>$a"
#define MQTT_QOS 1
#define MQTT_RETAIN false

#define SECRET_SSID "TurkTelekom_TP7CB8_2.4GHz"		// replace SSID with your WiFi network name
#define SECRET_PASS "4CM79L9F7Nv9"         // replace Password with your WiFi password
char ssid[] = SECRET_SSID;   // WiFi SSID
char pass[] = SECRET_PASS;   // WiFi Password

WiFiClient wificlient;
PubSubClient mqttClient(wificlient);
MAX30105 particleSensor;
 
const byte RATE_SIZE = 4; // Ortalama hesaplamak için kullanılacak örnek sayısı
byte rates[RATE_SIZE];    // Kalp atış hızlarını saklamak için dizi
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;              //gönderilecek veriler 1 ortalama nabız 

byte ledBrightness = 0x1F;
byte sampleAverage = 8;
byte ledMode = 3;
int sampleRate = 100;
int pulseWidth = 411;
int adcRange = 4096;

unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
const long interval = 20000; // upload intervali

StaticJsonDocument<200> data;
String jsonString;

void setupWiFi(){
  Serial.print("Connecting to WiFi...");
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  int attempts =0;

  while (WiFi.status() != WL_CONNECTED && attempts<10){
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() != WL_CONNECTED){
    Serial.println("Connected!");
  }
  else{
    Serial.println("Failed to connect to WiFi");
  }
}

void setupMQTT(){
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
}

void connectToMQTT() {
  String mqttClientId= "ESPClient";

  while (!mqttClient.connected()) {
    Serial.print("MQTT'ye bağlanılıyor...");
    mqttClientId += String(random(0xffff), HEX);
    if (mqttClient.connect("ESP32Client", MQTT_USER, MQTT_PASS)) {
      Serial.println("Bağlandı!");
    } else {
      Serial.print("Bağlantı başarısız, hata kodu: ");
      Serial.print(mqttClient.state());
      Serial.println(" 5 saniye sonra tekrar deneniyor...");
      delay(5000);
    }
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Wİfi connection lost. Reconnecting...");
    setupWiFi();
  }
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

void publishMessage () 
{
if (!mqttClient.connected()) {
  connectToMQTT();       // MQTT bağlantısını kontrol et
}

if (currentMillis - previousMillis >= interval) {
  previousMillis = currentMillis;

  data.clear();
  data["HR"]= HR();
  //GSR için de eklenecek

  serializeJson(data, jsonString); //?
  Serial.print("Publishing MQTT message; ");
  Serial.println(jsonString);

  if(mqttClient.publish(MQTT_TOPIC, jsonString.c_str(), jsonString.length(), MQTT_QOS, MQTT_RETAIN)){
    Serial.print("Message published successfully.");
  } 
  else{
    Serial.println("Failed to publish message.");
  }
  //Serial.print("MQTT Mesajı Gönderildi "); boolean publish (topic, payload, [length], [retained]) fonksiyonu ile kontorl et
}
}


void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 was not found. Please check wiring/power.");
    while (1);
  }
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  setupWiFi();
  setupMQTT(); 
  connectToMQTT();
  mqttClient.subscribe (MQTT_TOPIC);
}


void loop() {
  currentMillis = millis();

  checkWiFiConnection();
  mqttClient.loop();           // MQTT istemcisi döngüsü
  publishMessage();//topic, mesajın içeriği, qos fonksiyonun bağlantısı önce olması lazım mesajı attığımız fonksiyon sadece publish message setupa mqtt connect eklenecek
 }
