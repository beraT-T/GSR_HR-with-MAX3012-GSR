#include <Wire.h>
#include "MAX30105.h" // MAX30105 kütüphanesi
#include "heartRate.h"  // Nabız hesaplama algoritması için
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


MAX30105 particleSensor;
//wifi bilgiler
#define SECRET_SSID "TurkTelekom_TP7CB8_2.4GHz"
#define  SECRET_PASS "4CM79L9F7Nv9" 
char ssid[]= SECRET_SSID;
char pass[]= SECRET_PASS;


const char* clientid= "esma-ESP32" ;
const char* mqtt_server= "mqtt-dashboard.com";
const int mqtt_port= 1883;
 // MQTT konusunu tanımla
 const char* topic ="isu_ilab/esma/sensor_data"; // Kendi topic'inizi belirleyin


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

// // --- GSR (Galvanik Deri Tepkisi) Değişkenleri ---
// const int GSR_PIN = 34;       // GSR sensörünün bağlı olduğu analog pin (ESP32 için)
// int gsrRawSum = 0;            // 10 GSR okumasının toplamı
// int gsrAverageRaw = 0;        // Ortalama ham GSR değeri (10 okumadan)
// int gsrValueToPrint = 0;      // Seri porta yazdırılacak son hesaplanan ve ölçeklenmiş GSR değeri
// unsigned long lastGsrReadMillis = 0; // Son GSR okuma zamanı
// const long gsrReadInterval = 500;    // GSR okuma aralığı (milisaniye), ~3 Hz için

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
    client.setServer(mqtt_server,mqtt_port);
    client.setCallback(callback);
    Serial.println("MQTT setup complete.");
}

void connectToMQTT(){
    while (!client.connected() && WiFi.status() == WL_CONNECTED) {
        Serial.print("Attempting MQTT connection...");
        // Client ID ile bağlanmayı dene
        // İsteğe bağlı: Kullanıcı adı ve şifre varsa: client.connect(clientid, mqtt_username, mqtt_password)
        if (client.connect(clientid)) {
            Serial.println("connected to MQTT broker");
            // Bağlantı kurulunca, dinlemek istediğiniz konulara burada abone olabilirsiniz.
            // Örneğin: client.subscribe("cihazim/komut");
            // Bu projede şimdilik sadece yayın yapacağımız için subscribe kısmını boş bırakabiliriz.
            // Ama bir komut almak isterseniz burası doğru yer.
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state()); // Bağlantı hatası durumunu yazdır
            Serial.println(" try again in 5 seconds");
            // 5 saniye bekle ve tekrar dene
            delay(5000);
        }
    }
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

     if (WiFi.status() == WL_CONNECTED) { // Sadece WiFi bağlıysa MQTT'yi kur ve bağlanmayı dene
        setupMQTT();    // MQTT ayarlarını yap
        connectToMQTT(); // İlk bağlantı denemesini yap (opsiyonel, loop içinde zaten yapılacak)
                          // Ancak ilk açılışta hızlı bir deneme iyi olabilir.
    } else {
        Serial.println("WiFi connection failed, MQTT setup skipped.");
    }

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

// Bu fonksiyonu loop() fonksiyonundan önce veya global alanda tanımlayın
void publishSensorData(long ir, int bpm){ //int gsr) {
    // StaticJsonDocument<200> data; // Zaten globalde tanımlı, burada tekrar tanımlamaya gerek yok.
    data.clear(); // Her yeni mesaj için JSON belgesini temizle

    // JSON belgesine verileri ekle
    data["ir_value"] = ir;
    data["bpm"] = bpm;
    //data["gsr_value"] = gsr;
    data["timestamp"] = millis(); // İsteğe bağlı: Verinin zaman damgası

    // JSON belgesini bir String'e dönüştür
    // String jsonString; // Zaten globalde tanımlı
    serializeJson(data, jsonString);

   

    // JSON String'ini MQTT üzerinden yayınla
    if (client.publish(topic, jsonString.c_str())) { // .c_str() ile char* formatına çevir
        Serial.print("Published to ");
        Serial.print(topic);
        Serial.print(": ");
        Serial.println(jsonString);
    } else {
        Serial.println("Failed to publish message.");
    }
    jsonString = ""; // Bir sonraki kullanım için stringi temizle (opsiyonel, serializeJson üzerine yazar)
}






void loop() {
    unsigned long currentMillis = millis(); 
     checkWiFiConnection();

      // --- MQTT BAĞLANTI YÖNETİMİ ---
    if (WiFi.status() == WL_CONNECTED) { // Sadece WiFi bağlıysa MQTT işlemlerini yap
        if (!client.connected()) {     // Eğer MQTT bağlantısı kopmuşsa
            connectToMQTT();           // Yeniden bağlanmayı dene (bu fonksiyon zaten içinde deneme döngüsü barındırıyor)
        }
        client.loop(); // MQTT kütüphanesinin çalışması için BU ÇOK ÖNEMLİ!
                       // Gelen mesajları işler, keep-alive gönderir vs.
    }
    // --- MQTT BAĞLANTI YÖNETİMİ SONU ---

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

                // if (currentMillis - lastGsrReadMillis >= gsrReadInterval) {
                //     lastGsrReadMillis = currentMillis; 
                //     gsrRawSum = 0;
                //     for (int i = 0; i < 10; i++) { 
                //         gsrRawSum += analogRead(GSR_PIN);
                //     }
                //     gsrAverageRaw = gsrRawSum / 10;
                //     gsrValueToPrint = map(gsrAverageRaw, 0, 4095, 0, 1023); 
                // }

                if (currentMillis - lastPpgPrintMillis >= ppgPrintInterval) {
                    lastPpgPrintMillis = currentMillis; 
                    Serial.print(irValue); // Her loop'ta okunan en son irValue
                    Serial.print(",");
                    Serial.print(averageBPM); 
                    Serial.print(",");
                   // Serial.println(gsrValueToPrint); 
                   if (client.connected()) {
                    publishSensorData(irValue, averageBPM); // MQTT'ye gönder
                } else {
                    Serial.println("MQTT not connected to HiveMQ, data not published."); // BU MESAJI GÖRMELİSİN EĞER BAĞLI DEĞİLSE
                }
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