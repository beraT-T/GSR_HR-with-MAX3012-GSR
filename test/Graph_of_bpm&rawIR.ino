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
long lastBeat = 0;

float beatsPerMinute;
int beatAvg;

unsigned long previousMillis = 0;
const long updateInterval = 50;  // 50 ms'de bir grafik güncelleme

#define SCREEN_WIDTH  128  // TFT genişliği
#define SCREEN_HEIGHT 160  // TFT yüksekliği

#define GRAPH_HEIGHT 60  // Grafik yüksekliği
#define IR_OFFSET    10  // IR grafiği için dikey konum
#define BPM_OFFSET   90  // BPM grafiği için dikey konum

int irGraph[SCREEN_WIDTH];  // IR verilerini saklayan dizi
int bpmGraph[SCREEN_WIDTH]; // BPM verilerini saklayan dizi

void setup() {
    Serial.begin(115200);
    Serial.println("Initializing...");

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("MAX30105 was not found. Please check wiring/power.");
        while (1);
    }
    Serial.println("Place your finger on the sensor.");

    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x0A); 
    particleSensor.setPulseAmplitudeGreen(0);

    // TFT ekranı başlat
    tft.init();
    tft.setRotation(2);
    tft.fillScreen(TFT_BLACK);

    // Başlangıç mesajı göster
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 30);
    tft.println("Place Finger...");
    delay(2000);
    tft.fillScreen(TFT_BLACK);

    // Grafikleri başlatmak için boş veri ekleyelim
    for (int i = 0; i < SCREEN_WIDTH; i++) {
        irGraph[i] = SCREEN_HEIGHT / 2;
        bpmGraph[i] = SCREEN_HEIGHT / 2;
    }
}

void loop() {
    long irValue = particleSensor.getIR(); 

    if (checkForBeat(irValue) == true) {
        long delta = millis() - lastBeat;
        lastBeat = millis();

        beatsPerMinute = 60 / (delta / 1000.0);

        if (beatsPerMinute < 255 && beatsPerMinute > 20) {
            rates[rateSpot++] = (byte)beatsPerMinute;
            rateSpot %= RATE_SIZE;

            beatAvg = 0;
            for (byte x = 0; x < RATE_SIZE; x++)
                beatAvg += rates[x];
            beatAvg /= RATE_SIZE;
        }
    }

    Serial.print("IR=");
    Serial.print(irValue);
    Serial.print(", BPM=");
    Serial.print(beatsPerMinute);
    Serial.print(", Avg BPM=");
    Serial.println(beatAvg);

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= updateInterval) {
        previousMillis = currentMillis;

        // **Eski verileri sola kaydır**
        for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
            irGraph[i] = irGraph[i + 1];
            bpmGraph[i] = bpmGraph[i + 1];
        }

        // **Yeni veriyi ekleyelim**
        irGraph[SCREEN_WIDTH - 1] = map(irValue, 50000, 120000, IR_OFFSET + GRAPH_HEIGHT, IR_OFFSET);
        bpmGraph[SCREEN_WIDTH - 1] = map(beatAvg, 40, 120, BPM_OFFSET + GRAPH_HEIGHT, BPM_OFFSET);

        // **Ekranı temizleyelim**
        tft.fillScreen(TFT_BLACK);

        // **Başlıkları ve Sayısal Verileri Yaz**
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(1);

        tft.setCursor(5, 5);
        tft.print("IR Signal: ");
        tft.print(irValue);

        tft.setCursor(5, 85);
        tft.print("Heart Rate: ");
        tft.print(beatAvg);

        // **IR Grafiğini Çiz**
        for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
            tft.drawLine(i, irGraph[i], i + 1, irGraph[i + 1], TFT_RED);
        }

        // **BPM Grafiğini Çiz**
        for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
            tft.drawLine(i, bpmGraph[i], i + 1, bpmGraph[i + 1], TFT_GREEN);
        }
    }
}