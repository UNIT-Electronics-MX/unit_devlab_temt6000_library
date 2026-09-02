/**
 * @file adcAveragingSampler.ino
 * @brief Configures the TEMT6000 DDP device's onboard ADC moving-average
 *        window over I2C and streams the resulting averaged ADC0 readings
 *        to Serial.
 *
 * On startup the sketch verifies the sensor's identity, requests an
 * ADC_AVERAGING_SAMPLES-sample averaging window (CMD_SET_ADC_AVERAGING),
 * confirms the window the firmware actually applied (CMD_GET_ADC_AVERAGING),
 * and waits for that window to fill so the first reading is already a full
 * average. The main loop then polls CMD_READ_ADC0 every READ_INTERVAL_MS and
 * prints each raw averaged value as CSV to Serial.
 *
 * @author Cesar Bautista
 * @date 2026-09-02
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
constexpr uint8_t SENSOR_ADDRESS = 0x20U;
constexpr uint32_t READ_INTERVAL_MS = 100U;
/* Device-side moving-average window (firmware only accepts 1, 4, 8, 16 or 24).
 * A larger window smooths the reading more; it does not slow the device down,
 * because the average is kept up to date in the background. */
constexpr uint8_t ADC_AVERAGING_SAMPLES = 8U;
/* Rate at which the firmware feeds a fresh conversion into that window
 * (firmware/src/main.c: ADC_SAMPLE_INTERVAL_MS). Keep both in sync. */
constexpr uint32_t ADC_SAMPLE_INTERVAL_MS = 20U;

DevLabDDP::Master master(I2C_BUS, DevLabDDP::DEVICE_TEMT6000);
bool deviceVerified = false;

/* CMD_SET_ADC_AVERAGING is a two-step handshake: the command byte is
 * acknowledged first, then the sample-count byte is written and acknowledged
 * again once the firmware has stored it.
 *
 * Storing a new window also clears the device's moving-average buffer, which
 * then refills at one sample per ADC_SAMPLE_INTERVAL_MS. Waiting for it to
 * fill means the first reading is already a full average. */
bool setAdcAveraging(uint8_t address, uint8_t sampleCount) {
  uint8_t ack = 0;
  if (!master.readCommand(address, CMD_SET_ADC_AVERAGING, &ack, 1U, 10U) ||
      (ack & 0x0FU) != RESP_ADC_AVERAGING_SET) {
    return false;
  }
  if (!master.writeByte(address, sampleCount)) return false;
  delay(20U);
  uint8_t received = I2C_BUS.requestFrom(address, (uint8_t)1U);
  if (received != 1U) return false;
  if ((I2C_BUS.read() & 0x0FU) != RESP_ADC_AVERAGING_SET) return false;

  delay((uint32_t)sampleCount * ADC_SAMPLE_INTERVAL_MS);
  return true;
}

bool getAdcAveraging(uint8_t address, uint8_t &sampleCount) {
  return master.readCommand(address, CMD_GET_ADC_AVERAGING, &sampleCount, 1U);
}

/* Same as master.readAdc() but skips the identity check because setup() has
 * already confirmed the device. CMD_READ_ADC0 returns the background value. */
bool readAdc0(uint8_t address, uint16_t &value) {
  uint8_t bytes[2];
  if (!master.readCommand(address, CMD_READ_ADC0, bytes, 2U, 2U)) return false;
  value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
  return true;
}

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
      Serial, SENSOR_ADDRESS, info, DevLabDDP::DEVICE_TEMT6000);

  if (!setAdcAveraging(SENSOR_ADDRESS, ADC_AVERAGING_SAMPLES)) {
    Serial.println("WARN: could not set ADC averaging, using device default");
  }

  uint8_t confirmedSamples = 0;
  if (getAdcAveraging(SENSOR_ADDRESS, confirmedSamples)) {
    Serial.print("ADC averaging window: ");
    Serial.print(confirmedSamples);
    Serial.print(" samples (settles in ");
    Serial.print((uint32_t)confirmedSamples * ADC_SAMPLE_INTERVAL_MS);
    Serial.println(" ms)");
  }

  Serial.println("adc0_avg_raw");
}

void loop() {
  uint16_t raw;
  if (deviceVerified && readAdc0(SENSOR_ADDRESS, raw)) {
    Serial.println(raw);
  } else {
    Serial.println("ERR");
  }
  delay(READ_INTERVAL_MS);
}
