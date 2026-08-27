#include <Arduino.h>
#include <Wire.h>
#include <DevLabDDP.h>
#include <DevLabI2CBusRecovery.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Both devices share the same I2C bus.
#if defined(ARDUINO_ARCH_RP2040)
  #define WIRE Wire1
  constexpr uint8_t I2C_SDA = 12;
  constexpr uint8_t I2C_SCL = 13;
#elif defined(ARDUINO_ARCH_ESP32)
  #define WIRE Wire
  constexpr uint8_t I2C_SDA = 6;
  constexpr uint8_t I2C_SCL = 7;
#else
  #error "Use ESP32 or RP2040/RP2350, or define the I2C pins for your master"
#endif

constexpr uint32_t I2C_FREQ = 100000;

// Startup address configuration. Edit this value as needed.
constexpr uint8_t STARTUP_I2C_ADDRESS = 0x20;
constexpr uint8_t OLED_ADDRESS = 0x3C;

constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 64;
constexpr int GRAPH_LEFT = 27;
constexpr int GRAPH_WIDTH = OLED_WIDTH - GRAPH_LEFT;
constexpr int GRAPH_TOP = 16;
constexpr int GRAPH_BOTTOM = OLED_HEIGHT - 1;
constexpr uint16_t GRAPH_ADC_MIN = 2000;
constexpr uint16_t GRAPH_ADC_MAX = 4095;
constexpr uint16_t GRAPH_ADC_MIDDLE =
    GRAPH_ADC_MIN + ((GRAPH_ADC_MAX - GRAPH_ADC_MIN) / 2);
constexpr uint32_t SAMPLE_INTERVAL_MS = 50;

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &WIRE, -1);
DevLabDDP::Master master(WIRE, DevLabDDP::DEVICE_TEMT6000);
bool deviceVerified = false;
uint16_t samples[GRAPH_WIDTH] = {0};
uint16_t last_value = 0;
uint32_t last_sample_ms = 0;

bool readADC0(uint16_t &value)
{
  uint8_t bytes[2];
  if (!deviceVerified ||
      !master.readCommand(STARTUP_I2C_ADDRESS, CMD_READ_ADC0, bytes, 2U, 30U)) return false;
  value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
  return true;
}

int adcToY(uint16_t value)
{
  uint16_t visible_value = constrain(value, GRAPH_ADC_MIN, GRAPH_ADC_MAX);
  return map(
      visible_value,
      GRAPH_ADC_MIN,
      GRAPH_ADC_MAX,
      GRAPH_BOTTOM,
      GRAPH_TOP);
}

void addSample(uint16_t value)
{
  for (int x = 0; x < GRAPH_WIDTH - 1; ++x) {
    samples[x] = samples[x + 1];
  }
  samples[GRAPH_WIDTH - 1] = value;
}

void drawGraph(bool i2c_ok)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (i2c_ok) {
    display.print("ADC0: ");
    display.print(last_value);
    display.print("  ");
    display.print((last_value * 3.3f) / 4095.0f, 2);
    display.print("V");
  } else {
    display.print("PY32 0x");
    display.print(STARTUP_I2C_ADDRESS, HEX);
    display.print(" no response");
  }

  // Expanded visual scale for the useful sensor range. The reading sent
  // over I2C always keeps its original ADC value from 0 to 4095.
  const int graph_middle = adcToY(GRAPH_ADC_MIDDLE);
  display.setCursor(0, GRAPH_TOP);
  display.print(GRAPH_ADC_MAX);
  display.setCursor(0, graph_middle - 3);
  display.print(GRAPH_ADC_MIDDLE);
  display.setCursor(0, GRAPH_BOTTOM - 7);
  display.print(GRAPH_ADC_MIN);

  display.drawFastVLine(
      GRAPH_LEFT - 2, GRAPH_TOP, GRAPH_BOTTOM - GRAPH_TOP + 1, SSD1306_WHITE);
  display.drawFastHLine(
      GRAPH_LEFT, GRAPH_TOP, GRAPH_WIDTH, SSD1306_WHITE);

  // Dotted line marking the middle of the visible range.
  for (int x = GRAPH_LEFT; x < OLED_WIDTH; x += 4) {
    display.drawPixel(x, graph_middle, SSD1306_WHITE);
  }

  for (int x = 1; x < GRAPH_WIDTH; ++x) {
    display.drawLine(
        GRAPH_LEFT + x - 1,
        adcToY(samples[x - 1]),
        GRAPH_LEFT + x,
        adcToY(samples[x]),
        SSD1306_WHITE);
  }

  display.display();
}

void setup()
{
  Serial.begin(115200);

  if (!devlabBeginI2cBusRecovered(WIRE, I2C_SDA, I2C_SCL, I2C_FREQ, 100)) {
    Serial.println("Error: I2C bus is blocked");
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("No SSD1306 OLED found at 0x3C");
    while (true) {
      delay(1000);
    }
  }

  // Some libraries change the bus speed during initialization.
  WIRE.setClock(I2C_FREQ);
  DevLabDDP::DeviceInfo info;
  deviceVerified = master.matchesExpectedDevice(STARTUP_I2C_ADDRESS, &info);
  if (!deviceVerified) {
    Serial.println("The address does not contain a TEMT6000 DDP device (ID 0x0102)");
  }
  display.clearDisplay();
  display.display();
}

void loop()
{
  if (millis() - last_sample_ms < SAMPLE_INTERVAL_MS) {
    return;
  }
  last_sample_ms = millis();

  uint16_t value = 0;
  bool i2c_ok = readADC0(value);
  if (i2c_ok) {
    last_value = value;
    addSample(value);
    Serial.println(value);
  }

  drawGraph(i2c_ok);
}
