#include <Wire.h>
#include "RTClib.h"
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2); // kalau LCD ga nyala coba ganti ke 0x3F

// --- WiFi (ganti sesuai hotspot/WiFi yang dipakai) ---
const char* WIFI_SSID = "TU";
const char* WIFI_PASSWORD = "smiskirkid";

// --- Firebase ---
const String FIREBASE_HOST = "https://medbotalarm-default-rtdb.asia-southeast1.firebasedatabase.app";
const String FIREBASE_AUTH = "OJxPbYq2uQnA5yLbdezQUjEnO7ZNRFeJMl35Tlga";

bool wifiTerhubung = false;
unsigned long waktuCekWifiTerakhir = 0;
const unsigned long INTERVAL_CEK_WIFI = 5000;
unsigned long waktuCekFirebaseTerakhir = 0;
const unsigned long INTERVAL_CEK_FIREBASE = 3000;

#define BUZZER_PIN 25

int alarmJam[3]   = {0, 0, 0};
int alarmMenit[3] = {0, 0, 0};
bool alarmSudahBunyiHariIni[3] = {false, false, false};

unsigned long waktuUpdateLCDTerakhir = 0;
String pesanLCDSementara = "";
unsigned long waktuPesanLCDMulai = 0;
const unsigned long DURASI_PESAN_LCD = 1500;

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("MedBot Alarm");
  lcd.setCursor(0, 1);
  lcd.print("Cari WiFi...");

  if (!rtc.begin()) {
    Serial.println("RTC tidak terdeteksi! Cek wiring SDA/SCL.");
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Menghubungkan ke WiFi...");

  int percobaan = 0;
  while (WiFi.status() != WL_CONNECTED && percobaan < 20) {
    delay(500);
    Serial.print(".");
    percobaan++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiTerhubung = true;
    Serial.println("\nWiFi terhubung!");
  } else {
    Serial.println("\nWiFi gagal connect, cek SSID/password.");
  }

  delay(1000);
}

void loop() {
  cekStatusWifi();

  if (wifiTerhubung && millis() - waktuCekFirebaseTerakhir > INTERVAL_CEK_FIREBASE) {
    waktuCekFirebaseTerakhir = millis();
    cekUpdateJadwalDariFirebase();
  }

  cekAlarm();
  updateLCD();
  delay(200);
}

void cekStatusWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiTerhubung = true;
  } else {
    wifiTerhubung = false;
    if (millis() - waktuCekWifiTerakhir > INTERVAL_CEK_WIFI) {
      waktuCekWifiTerakhir = millis();
      Serial.println("WiFi belum terhubung, mencoba lagi...");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}

// ambil 1 nilai integer dari Firebase lewat REST API (GET request)
int ambilIntDariFirebase(String path) {
  HTTPClient http;
  String url = FIREBASE_HOST + path + ".json?auth=" + FIREBASE_AUTH;
  http.begin(url);
  int httpCode = http.GET();
  int hasil = -1;

  if (httpCode == 200) {
    String payload = http.getString();
    if (payload != "null") {
      hasil = payload.toInt();
    }
  }
  http.end();
  return hasil;
}

// kirim 1 nilai ke Firebase lewat REST API (PUT request)
void kirimKeFirebase(String path, String valueJson) {
  HTTPClient http;
  String url = FIREBASE_HOST + path + ".json?auth=" + FIREBASE_AUTH;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.PUT(valueJson);
  http.end();
}

void cekUpdateJadwalDariFirebase() {
  for (int i = 1; i <= 3; i++) {
    int jamBaru = ambilIntDariFirebase("/alarm/" + String(i) + "/jam");
    int menitBaru = ambilIntDariFirebase("/alarm/" + String(i) + "/menit");

    if (jamBaru != -1 && menitBaru != -1) {
      if (jamBaru != alarmJam[i-1] || menitBaru != alarmMenit[i-1]) {
        alarmJam[i-1] = jamBaru;
        alarmMenit[i-1] = menitBaru;
        alarmSudahBunyiHariIni[i-1] = false;
        Serial.printf("Alarm %d diupdate ke %02d:%02d\n", i, jamBaru, menitBaru);

        char pesan[17];
        snprintf(pesan, sizeof(pesan), "Set %d: %02d:%02d", i, jamBaru, menitBaru);
        tampilkanPesanLCD(String(pesan));
      }
    }
  }
}

void tampilkanPesanLCD(String pesan) {
  pesanLCDSementara = pesan;
  waktuPesanLCDMulai = millis();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Status:");
  lcd.setCursor(0, 1);
  lcd.print(pesan);
}

void updateLCD() {
  if (pesanLCDSementara != "" && (millis() - waktuPesanLCDMulai < DURASI_PESAN_LCD)) {
    return;
  }
  pesanLCDSementara = "";

  if (millis() - waktuUpdateLCDTerakhir < 1000) {
    return;
  }
  waktuUpdateLCDTerakhir = millis();

  DateTime now = rtc.now();
  char baris1[17];
  snprintf(baris1, sizeof(baris1), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  lcd.setCursor(0, 0);
  lcd.print("Jam  ");
  lcd.print(baris1);
  lcd.print("  ");

  lcd.setCursor(0, 1);
  lcd.print(wifiTerhubung ? "WiFi: Terhubung " : "WiFi: Menunggu..");
}

void cekAlarm() {
  DateTime now = rtc.now();
  for (int i = 0; i < 3; i++) {
    if (now.hour() == alarmJam[i] && now.minute() == alarmMenit[i]) {
      if (!alarmSudahBunyiHariIni[i]) {
        bunyikanAlarm(i + 1);
        alarmSudahBunyiHariIni[i] = true;
      }
    } else if (now.hour() == 0 && now.minute() == 0) {
      alarmSudahBunyiHariIni[i] = false;
    }
  }
}

void bunyikanAlarm(int nomorAlarm) {
  tampilkanPesanLCD("Minum Obat!");

  if (wifiTerhubung) {
    kirimKeFirebase("/status/alarm_bunyi", "true");
    kirimKeFirebase("/status/alarm_nomor", String(nomorAlarm));
  }

  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);
    delay(300);
  }
}
