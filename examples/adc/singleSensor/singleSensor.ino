/**
 * @file singleSensor.ino
 * @brief Verifies a single TEMT6000 DDP device on the I2C bus and prints
 *        its raw ADC0 readings over Serial at a fixed interval.
 * @author Cesar Bautista
 */

#include <Arduino.h>
#include <Wire.h>
#include <DevLabDDP.h>
#include <DevLabI2CBusRecovery.h>

#if defined(ARDUINO_ARCH_RP2040)
  #define I2C_BUS Wire
  constexpr uint8_t I2C_SDA = 24U, I2C_SCL = 25U;
#elif defined(ARDUINO_ARCH_ESP32)
  #define I2C_BUS Wire
  constexpr uint8_t I2C_SDA = 6U, I2C_SCL = 7U;
#else
  #error "Use ESP32 or RP2040/RP2350"
#endif

constexpr uint32_t I2C_FREQ = 400000;
constexpr uint8_t SENSOR_ADDRESS = 0x20;
constexpr uint32_t READ_INTERVAL_MS = 1000U;

DevLabDDP::Master master(I2C_BUS, DevLabDDP::DEVICE_TEMT6000);
bool deviceVerified = false;

void setup() {
  Serial.begin(115200);
  delay(500);

  if (!devlabBeginI2cBusRecovered(I2C_BUS, I2C_SDA, I2C_SCL, I2C_FREQ, 100)) {
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
