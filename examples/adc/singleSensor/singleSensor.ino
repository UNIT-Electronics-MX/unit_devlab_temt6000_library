/**
 * @file singleSensor.ino
 * @brief Verifies a single TEMT6000 DDP device on the I2C bus and prints
 *        its raw ADC0 readings over Serial at a fixed interval.
 * @author Cesar Bautista
 */

#include <Arduino.h>
#include <Wire.h>
#include <DevLabDDP.h>
#include <DevLab_I2C_Orchestrator.h>

constexpr uint8_t SENSOR_ADDRESS = 0x20;
constexpr uint32_t READ_INTERVAL_MS = 1000U;

#if defined(ARDUINO_ARCH_RP2040)
  #define I2C_BUS Wire
  constexpr uint32_t I2C_FREQ = 400000;
  constexpr uint8_t I2C_SDA = 24U, I2C_SCL = 25U;
#elif defined(ARDUINO_ARCH_ESP32)
  #define I2C_BUS Wire
  constexpr uint32_t I2C_FREQ = 400000;
  constexpr uint8_t I2C_SDA = 6U, I2C_SCL = 7U;
#elif defined(ARDUINO_ARCH_AVR)
  #define I2C_BUS Wire
  constexpr uint32_t I2C_FREQ = 400000;  // 400kHz falla con el level shifter en UNO
  constexpr uint8_t I2C_SDA = SDA, I2C_SCL = SCL;  // solo informativo
#elif defined(ARDUINO_ARCH_STM32)
  #define I2C_BUS Wire
  constexpr uint32_t I2C_FREQ = 400000;  // 400kHz falla con pull-ups débiles/cables largos en Blue Pill
  // STM32duino: los pines dependen del paquete/placa y se aplican
  // vía Wire.begin(sda, scl) (ver DevLab_I2C_Common::beginCommon).
  // Ejemplo típico para Blue Pill (F103C8T6):
  constexpr uint8_t I2C_SDA = PB7, I2C_SCL = PB6;
  // Otras placas STM32 pueden usar PB9/PB8, o PA10/PA9, etc.
#else
  #error "Use ESP32, RP2040/RP2350, AVR o STM32"
#endif




DevLab_I2C_Orchestrator bus(I2C_BUS, I2C_FREQ);
DevLabDDP::Master master(bus, DevLabDDP::DEVICE_TEMT6000);
bool deviceVerified = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  bool i2cOk = bus.beginRecovered(I2C_SDA, I2C_SCL, 20000, false);

  if (!i2cOk) {
    Serial.println("ERROR: I2C bus is blocked");
    return;
  }

  DevLabDDP::DeviceInfo info;
  deviceVerified = master.matchesExpectedDevice(SENSOR_ADDRESS, &info);

  if (!deviceVerified) {
    Serial.println("ERROR: address is not a DDP TEMT6000 (ID 0x0102)");
    return;
  }

  DevLabDDP::printDeviceInfo(
      Serial,
      SENSOR_ADDRESS,
      info,
      DevLabDDP::DEVICE_TEMT6000);

  Serial.println("adc0_raw");
}

void loop() {
  uint16_t raw;

  if (deviceVerified && master.readAdc(SENSOR_ADDRESS, 0, raw)) {
    Serial.println(raw);
  } else {
    Serial.println("ERR");
  }

  delay(READ_INTERVAL_MS);
}