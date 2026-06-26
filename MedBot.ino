#include "BluetoothSerial.h"
#include <ESP32Servo.h>
#include <Wire.h>
#include "RTClib.h"

BluetoothSerial SerialBT;
Servo servoObat;
RTC_DS3231 rtc;

// Pin buzzer
#define BUZZER_PIN 25

// Jadwal alarm (jam, menit) - default jam 00:00 artinya belum diset
int alarmJam[3]   = {0, 0, 0};
int alarmMenit[3] = {0, 0, 0};
bool alarmSudahBunyiHariIni[3] = {false, false, false};

String bufferBT = ""; // buat nampung teks yang masuk dari app, huruf per huruf

// Buat ngecek status koneksi Bluetooth (biar gak nge-print berkali-kali)
bool statusConnectSebelumnya = false;

// --- Ganti pin di bawah sesuai wiring motor driver kamu ---
// Motor kiri
#define IN1 26
#define IN2 27
// Motor kanan
#define IN3 14
#define IN4 12

// Pin servo buat buka-tutup kotak obat
#define SERVO_PIN 13

// Posisi sudut servo (sesuaikan nanti pas testing fisik)
#define SUDUT_TUTUP 0
#define SUDUT_BUKA  90

void setup() {
  Serial.begin(115200);
  SerialBT.begin("MedBot");  // nama Bluetooth yang akan muncul di HP
  Serial.println("MedBot siap, cari device 'MedBot' di Bluetooth HP kamu");
  Serial.println("Status: BELUM TERHUBUNG ke HP manapun.");

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  servoObat.attach(SERVO_PIN);
  servoObat.write(SUDUT_TUTUP); // posisi awal: kotak obat tertutup

  // Inisialisasi RTC
  if (!rtc.begin()) {
    Serial.println("RTC tidak terdeteksi! Cek wiring SDA/SCL.");
  }
  // Kalau RTC kehilangan power (baterai lemah/baru pasang), set waktu otomatis
  // sesuai waktu compile kode ini. Sekali aja, baris ini bisa dihapus/comment
  // setelah RTC berhasil diset pertama kali.
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  stopMotor();
}

void loop() {
  // 0. Cek status koneksi Bluetooth, print ke Serial Monitor kalau berubah
  cekStatusKoneksi();

  // 1. Baca data masuk dari Bluetooth
  while (SerialBT.available()) {
    char c = SerialBT.read();

    if (c == '#') {
      // mulai mode "terima jadwal alarm", kumpulin huruf sampai Enter
      bufferBT = "#";
    }
    else if (bufferBT.startsWith("#")) {
      if (c == '\n' || c == '\r') {
        prosesSetAlarm(bufferBT);
        bufferBT = "";
      } else {
        bufferBT += c;
      }
    }
    else {
      // bukan mode set alarm, berarti perintah gerak 1 huruf biasa
      switch (c) {
        case 'F': maju(); break;
        case 'B': mundur(); break;
        case 'L': kiri(); break;
        case 'R': kanan(); break;
        case 'S': stopMotor(); break;
        case 'O': bukaKotakObat(); break;
      }
    }
  }

  // 2. Cek alarm tiap detik
  cekAlarm();
  delay(200); // kasih jeda kecil biar gak terlalu sering cek
}

// Cek apakah ada HP yang sedang connect via Bluetooth.
// Cuma print ke Serial Monitor SEKALI tiap kali statusnya berubah
// (jadi gak spam tulisan yang sama berulang-ulang)
void cekStatusKoneksi() {
  bool statusSekarang = SerialBT.hasClient();

  if (statusSekarang != statusConnectSebelumnya) {
    if (statusSekarang) {
      Serial.println(">>> HP TERHUBUNG ke MedBot! <<<");
    } else {
      Serial.println(">>> HP TERPUTUS dari MedBot. Menunggu koneksi baru... <<<");
    }
    statusConnectSebelumnya = statusSekarang;
  }
}

// Proses teks "#108:30" / "#208:30" / "#308:30"
// Format: # diikuti index alarm (1/2/3), lalu jam:menit
void prosesSetAlarm(String teks) {
  // contoh: #108:30  -> index alarm = 1, jam = 08, menit = 30
  int indexAlarm = teks.substring(1, 2).toInt(); // karakter ke-2 (setelah #)
  String sisaJamMenit = teks.substring(2);        // sisanya "08:30"

  int posTitikDua = sisaJamMenit.indexOf(':');
  if (posTitikDua == -1) return; // format salah, abaikan

  int jam   = sisaJamMenit.substring(0, posTitikDua).toInt();
  int menit = sisaJamMenit.substring(posTitikDua + 1).toInt();

  if (indexAlarm >= 1 && indexAlarm <= 3) {
    alarmJam[indexAlarm - 1]   = jam;
    alarmMenit[indexAlarm - 1] = menit;
    alarmSudahBunyiHariIni[indexAlarm - 1] = false; // reset biar bisa bunyi lagi
    Serial.printf("Alarm %d diset ke %02d:%02d\n", indexAlarm, jam, menit);
  }
}

// Cek apakah sekarang waktunya alarm bunyi
void cekAlarm() {
  DateTime now = rtc.now();

  for (int i = 0; i < 3; i++) {
    if (now.hour() == alarmJam[i] && now.minute() == alarmMenit[i]) {
      if (!alarmSudahBunyiHariIni[i]) {
        bunyikanAlarm();
        alarmSudahBunyiHariIni[i] = true;
      }
    } else if (now.hour() == 0 && now.minute() == 0) {
      // lewat tengah malam, reset semua biar besok bisa bunyi lagi
      alarmSudahBunyiHariIni[i] = false;
    }
  }
}

// Bunyikan buzzer + kirim notif "ALARM" ke app
void bunyikanAlarm() {
  SerialBT.println("ALARM"); // app bakal baca ini & munculin notifikasi

  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);
    delay(300);
  }
}

void bukaKotakObat() {
  servoObat.write(SUDUT_BUKA);   // servo gerak ke posisi buka
  delay(2000);                   // tahan 2 detik biar obat sempat keluar
  servoObat.write(SUDUT_TUTUP);  // servo balik ke posisi tutup
}

void maju() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void mundur() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void kiri() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void kanan() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}
