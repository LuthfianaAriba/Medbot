#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// --- Ganti pin di bawah sesuai wiring motor driver kamu ---
// Motor kiri
#define IN1 26
#define IN2 27
#define ENA 33  // enable motor kiri (speed)
// Motor kanan
#define IN3 14
#define IN4 12
#define ENB 32  // enable motor kanan (speed)

// kecepatan motor (0-255), turunin dari full speed (255) biar gak terlalu kencang
#define KECEPATAN 150

bool statusConnectSebelumnya = false;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("MedBot");
  Serial.println("MedBot siap, cari device 'MedBot' di Bluetooth HP kamu");

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  stopMotor();
}

void loop() {
  cekStatusKoneksi();

  while (SerialBT.available()) {
    char c = SerialBT.read();
    switch (c) {
      case 'F': maju();      break;
      case 'B': mundur();    break;
      case 'L': kiri();      break;
      case 'R': kanan();     break;
      case 'S': stopMotor(); break;
    }
  }

  delay(200);
}

void cekStatusKoneksi() {
  bool statusSekarang = SerialBT.hasClient();
  if (statusSekarang != statusConnectSebelumnya) {
    Serial.println(statusSekarang ? ">>> HP TERHUBUNG <<<" : ">>> HP TERPUTUS <<<");
    statusConnectSebelumnya = statusSekarang;
  }
}

// maju: kedua motor jalan ke depan, speed dikurangin pakai PWM
void maju() {
  analogWrite(ENA, KECEPATAN);
  analogWrite(ENB, KECEPATAN);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

// mundur: kedua motor jalan ke belakang
void mundur() {
  analogWrite(ENA, KECEPATAN);
  analogWrite(ENB, KECEPATAN);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

// kiri: cuma motor KANAN yang jalan, motor kiri mati (pivot ke kiri)
void kiri() {
  analogWrite(ENA, 0);          // motor kiri mati
  analogWrite(ENB, KECEPATAN);  // motor kanan jalan
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

// kanan: cuma motor KIRI yang jalan, motor kanan mati (pivot ke kanan)
void kanan() {
  analogWrite(ENA, KECEPATAN);  // motor kiri jalan
  analogWrite(ENB, 0);          // motor kanan mati
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
}

void stopMotor() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}
