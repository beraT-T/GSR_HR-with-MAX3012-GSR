#include <Wire.h>
#include "MAX30105.h" // MAX30105 kütüphanesi
#include "heartRate.h"  // Nabız hesaplama algoritması için
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


MAX30105 particleSensor;
//wifi bilgiler
#define SECRET_SSID "Galaxy M3121E1"
#define  SECRET_PASS "ildv3767"
char ssid[]= SECRET_SSID;
char pass[]= SECRET_PASS;

const char* clientid "isu-ilab-esma";
const char* mqtt_server "//opkg.pievision.com";


WiFiClient espClient;
PubSubClient client(espClient);


// --- Program Durumları ---
enum ProgramState {
  STATE_INITIAL_DELAY,
  STATE_LOGGING_DATA,
  STATE_PRINTING_PEAKS,
  STATE_FINISHED
};
ProgramState currentState = STATE_INITIAL_DELAY;

unsigned long programStartTime = 0;
unsigned long loggingStartTime = 0;
const unsigned long initialDelayDuration = 5000;    // 5 saniye
const unsigned long loggingDuration = 120000;      // 1 dakika (60 saniye)


// --- Peak Zaman ve Değerlerini Tutma ---
#define MAX_BEATS 100                 // Kaydedilecek maksimum peak sayısı
unsigned long peakTimes[MAX_BEATS];   // Algılanan beat zamanları (ms)
long peakValues[MAX_BEATS];            // Algılanan beat anındaki IR değerleri
int peakCount = 0;                    // Kaydedilen peak sayısı (1 dakikalık periyotta)

// --- Nabız Hesaplama Değişkenleri ---
const byte RATE_SIZE = 1;       // Ortalama nabız için kullanılacak son ölçüm sayısı
byte rates[RATE_SIZE] = {0};    // Son nabız ölçümlerini tutan dizi, başlangıçta 0'larla dolu
byte rateSpot = 0;              // Dizideki mevcut yazma pozisyonu
byte beatsCollected = 0;        // `rates` dizisini doldurmak için toplanan geçerli vuruş sayısı
long lastBeatTime = 0;          // Son nabız algılandığı zaman (milisaniye cinsinden)
float currentBPM_float;         // İki vuruş arası hesaplanan anlık BPM (float)
int averageBPM = 0;             // Son RATE_SIZE vuruşun ortalama BPM'i

// --- GSR (Galvanik Deri Tepkisi) Değişkenleri ---
const int GSR_PIN = 34;       // GSR sensörünün bağlı olduğu analog pin (ESP32 için)
int gsrRawSum = 0;            // 10 GSR okumasının toplamı
int gsrAverageRaw = 0;        // Ortalama ham GSR değeri (10 okumadan)
int gsrValueToPrint = 0;      // Seri porta yazdırılacak son hesaplanan ve ölçeklenmiş GSR değeri
unsigned long lastGsrReadMillis = 0; // Son GSR okuma zamanı
const long gsrReadInterval = 500;    // GSR okuma aralığı (milisaniye), ~3 Hz için

// --- Seri Port Yazdırma Zamanlaması (Ana Veri Akışı) ---
unsigned long lastPpgPrintMillis = 0;   // Son ana veri yazdırma zamanı
const long ppgPrintInterval = 10;       // Ana veri yazdırma aralığı (milisaniye), 100 Hz hedefi

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
    Serial.println("Failed to connect to WiFi");
  }
  else{
    Serial.println("Connected");
  }
}
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Wİfi connection lost. Reconnecting...");
    setupWiFi();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i=0;i<length;i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }


void setupMQTT(){
    mqttClient.setServer(mqtt_server,1883);
    mqttClient.setCallback(callback);
}

void connectToMQTT(){
    String mqttCientd= ""
}
void setup() {
    Serial.begin(115200);
    Serial.println("Initializing MAX3010X sensor...");
    Wire.begin(25, 26); // I2C pinlerini GPIO25 (SDA) ve GPIO26 (SCL) olarak ayarla

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("MAX3010X was not found. Please check wiring/power.");
        while (1);
    }
    Serial.println("Setup complete. Program will start after initial delay.");
    Serial.println("Place your index finger on the sensor with steady pressure.");
     setupWiFi();

    byte irLedBrightness = 0x32; 
    byte sampleAverage = 1;      
    byte ledMode = 2;            
    int sampleRate = 100;       
    int pulseWidth = 411;       
    int adcRange = 4096;        

    particleSensor.setup(irLedBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); 
    particleSensor.setPulseAmplitudeIR(irLedBrightness); 
    particleSensor.setPulseAmplitudeRed(0x0A);          
    particleSensor.setPulseAmplitudeGreen(0);           
    
    averageBPM = 0; 
    programStartTime = millis(); // Programın başlama zamanını kaydet
}

void loop() {
    unsigned long currentMillis = millis(); 
     checkWiFiConnection();

    switch (currentState) {
        case STATE_INITIAL_DELAY:
            if (currentMillis - programStartTime >= initialDelayDuration) {
                Serial.println("Initial delay finished. Starting data logging for 1 minute...");
                Serial.println("IR,BPM,GSR"); // CSV başlığını kayıt başlamadan hemen önce yazdır
                loggingStartTime = currentMillis;
                currentState = STATE_LOGGING_DATA;
            }
            // İsteğe bağlı: Bekleme sırasında bir mesaj yazdırılabilir
            // else { Serial.print("."); delay(500); }
            break;

        case STATE_LOGGING_DATA:
            if (currentMillis - loggingStartTime < loggingDuration) {
                // --- Sensör Okuma ve İşleme Bloğu ---
                long irValue = particleSensor.getIR(); 

                if (checkForBeat(irValue) == true) { 
                    // ----- PEAK ZAMAN VE DEĞERLERİNİ KAYDET (sadece kayıt periyodunda ve limit dolmadıysa) -----
                    if (peakCount < MAX_BEATS) {
                        peakTimes[peakCount] = currentMillis; // Vuruşun olduğu anki zaman
                        peakValues[peakCount] = irValue;     // O andaki IR değeri
                        peakCount++;
                    }

                    if (lastBeatTime != 0) { 
                        long deltaTime = currentMillis - lastBeatTime;
                        if (deltaTime >= 400 && deltaTime < 2000) { 
                            currentBPM_float = 60000.0 / deltaTime; 
                            rates[rateSpot] = (byte)currentBPM_float; 
                            rateSpot = (rateSpot + 1) % RATE_SIZE;    
                            if (beatsCollected < RATE_SIZE) {
                                beatsCollected++; 
                            }
                            if (beatsCollected >= RATE_SIZE) {
                                long sumOfRates = 0;
                                for (byte i = 0; i < RATE_SIZE; i++) {
                                    sumOfRates += rates[i];
                                }
                                averageBPM = sumOfRates / RATE_SIZE; 
                            } else {
                                averageBPM = 0; 
                            }
                        }
                    }
                    lastBeatTime = currentMillis; 
                }

                if (currentMillis - lastGsrReadMillis >= gsrReadInterval) {
                    lastGsrReadMillis = currentMillis; 
                    gsrRawSum = 0;
                    for (int i = 0; i < 10; i++) { 
                        gsrRawSum += analogRead(GSR_PIN);
                    }
                    gsrAverageRaw = gsrRawSum / 10;
                    gsrValueToPrint = map(gsrAverageRaw, 0, 4095, 0, 1023); 
                }

                if (currentMillis - lastPpgPrintMillis >= ppgPrintInterval) {
                    lastPpgPrintMillis = currentMillis; 
                    Serial.print(irValue); // Her loop'ta okunan en son irValue
                    Serial.print(",");
                    Serial.print(averageBPM); 
                    Serial.print(",");
                    Serial.println(gsrValueToPrint); 
                }
                // --- Sensör Okuma ve İşleme Bloğu Sonu ---
            } else {
                Serial.println("1 minute data logging finished.");
                currentState = STATE_PRINTING_PEAKS;
            }
            break;

        case STATE_PRINTING_PEAKS:
            Serial.println("--- PEAK_DATA_START ---");
            if (peakCount > 0) {
                Serial.print("PeakTimes_ms:");
                for (int i = 0; i < peakCount; i++) {
                    Serial.print(peakTimes[i]);
                    if (i < peakCount - 1) Serial.print(";");
                }
                Serial.println();

                Serial.print("PeakValues_IR:");
                for (int i = 0; i < peakCount; i++) {
                    Serial.print(peakValues[i]);
                    if (i < peakCount - 1) Serial.print(";");
                }
                Serial.println();
            } else {
                Serial.println("No peaks recorded during the logging period.");
            }
            Serial.println("--- PEAK_DATA_END ---");
            Serial.println("Program finished. Restart ESP32 to run again.");
            currentState = STATE_FINISHED;
            break;

        case STATE_FINISHED:
            // Program bu durumda kalır, başka bir işlem yapmaz.
            // ESP32'yi yeniden başlatmak gerekir.
            delay(1000); // Boşta bekleme
            break;
    }
}