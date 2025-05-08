#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <TFT_eSPI.h>  // Bodmer'in TFT ekran kütüphanesi
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();  // TFT ekran nesnesi
MAX30105 particleSensor;

const byte RATE_SIZE = 4; // Nabız ölçümleri için dizi boyutu
byte rates[RATE_SIZE]; 
byte rateSpot = 0;
long lastBeat = 0; // Son nabız algılandığı zaman

float beatsPerMinute;
int beatAvg;

unsigned long previousMillis = 0;  // Son ekran güncelleme zamanı
const long updateInterval = 1000;  // Ekranı her 1 saniyede bir güncelle

void setup() {
    Serial.begin(115200);
    Serial.println("Initializing...");

    // Sensörü başlat
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("MAX30105 was not found. Please check wiring/power.");
        while (1);
    }
    Serial.println("Place your index finger on the sensor with steady pressure.");

    particleSensor.setup(); // Sensör varsayılan ayarlarla başlatılır
    particleSensor.setPulseAmplitudeRed(0x0A); // Kırmızı LED düşük parlaklıkta çalışsın
    particleSensor.setPulseAmplitudeGreen(0);  // Yeşil LED kapalı

    // TFT ekranı başlat
    tft.init();
    tft.setRotation(2); // Ekran yönü
    tft.fillScreen(TFT_BLACK);

    // Başlangıç mesajı göster
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 30);
    tft.println("Place Finger...");
    delay(2000);
    tft.fillScreen(TFT_BLACK);
}

void loop() {
    long irValue = particleSensor.getIR(); // IR sensör verisini al

    if (checkForBeat(irValue) == true) {
        // Nabız algılandı!
        long delta = millis() - lastBeat;
        lastBeat = millis();

        beatsPerMinute = 60 / (delta / 1000.0);

        if (beatsPerMinute < 255 && beatsPerMinute > 20) {
            rates[rateSpot++] = (byte)beatsPerMinute;
            rateSpot %= RATE_SIZE; // Döngüsel indeksleme

            // Ortalama nabız hesapla (orijinal koddan birebir alındı)
            beatAvg = 0;
            for (byte x = 0 ; x < RATE_SIZE ; x++)
                beatAvg += rates[x];
            beatAvg /= RATE_SIZE;
        }
    }

    // Seri Monitör'e yazdır
    Serial.print("IR=");
    Serial.print(irValue);
    Serial.print(", BPM=");
    Serial.print(beatsPerMinute);
    Serial.print(", Avg BPM=");
    Serial.print(beatAvg);

    if (irValue < 50000) {
        Serial.print(" No finger?");
    }
    Serial.println();

    // **TFT ekranı yalnızca belirli aralıklarla güncelle**
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= updateInterval) {
        previousMillis = currentMillis;

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 30);

        if (irValue < 50000) {
            tft.println("No Finger!");
        } else {
            tft.print("BPM:");
            tft.println(beatsPerMinute);
            tft.print("AvgBPM:");
            tft.println(beatAvg);
        }
    }
}