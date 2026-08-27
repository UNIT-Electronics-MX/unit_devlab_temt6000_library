#include <Arduino.h>
#include <Wire.h>
#include <DevLabDDP.h>

#if defined(ARDUINO_ARCH_RP2040)
  #define I2C_BUS Wire1
  constexpr uint8_t I2C_SDA = 12U, I2C_SCL = 13U;
#elif defined(ARDUINO_ARCH_ESP32)
  #define I2C_BUS Wire
  constexpr uint8_t I2C_SDA = 6U, I2C_SCL = 7U;
#else
  #error "Use ESP32 or RP2040/RP2350"
#endif

constexpr uint8_t SENSOR_ADDRESS = 0x20;
constexpr uint32_t READ_INTERVAL_MS = 1000U;

DevLabDDP::Master master(I2C_BUS, DevLabDDP::DEVICE_TEMT6000);
bool deviceVerified = false;

void setup() {
  Serial.begin(115200);
  delay(500);

#if defined(ARDUINO_ARCH_RP2040)
  I2C_BUS.setSDA(I2C_SDA); I2C_BUS.setSCL(I2C_SCL); I2C_BUS.begin();
#else
  I2C_BUS.begin(I2C_SDA, I2C_SCL);
#endif
  I2C_BUS.setClock(400000);

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
