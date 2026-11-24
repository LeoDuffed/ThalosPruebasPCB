#include <Arduino.h>

namespace {
constexpr uint32_t kBaudRate = 115200;
constexpr int kStm32RxPin = 16;  // STM32 PB12 (TX) -> ESP32 GPIO16 (RX2).
constexpr int kUnusedTxPin = 17;  // No se conecta para esta prueba.
uint32_t lastDataMs = 0;
uint32_t lastNoticeMs = 0;
}

void setup() {
  Serial.begin(kBaudRate);
  Serial2.setRxBufferSize(2048);
  Serial2.begin(kBaudRate, SERIAL_8N1, kStm32RxPin, kUnusedTxPin);
  Serial.println("ESP32: puente UART2 -> USB listo. RX=GPIO16, 115200 8N1.");
}

void loop() {
  // Reenviar todos los mensajes: VL, matriz y errores de inicializacion.
  while (Serial2.available() > 0) {
    const int value = Serial2.read();
    if (value >= 0) {
      Serial.write(static_cast<uint8_t>(value));
      lastDataMs = millis();
    }
  }

  const uint32_t now = millis();
  if (uint32_t(now - lastDataMs) >= 3000U &&
      uint32_t(now - lastNoticeMs) >= 3000U) {
    Serial.println("[ESP32] Sin datos del STM32 por GPIO16; revisar TX y GND.");
    lastNoticeMs = now;
  }

  delay(1);
}
