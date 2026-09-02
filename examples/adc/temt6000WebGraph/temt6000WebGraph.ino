/**
 * @file temt6000WebGraph.ino
 * @brief Hosts a WiFi access point with a live web dashboard that graphs
 *        the TEMT6000 DDP device's ADC0 readings, served over a small
 *        onboard HTTP API.
 * @author Cesar Bautista
 */

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <DevLabDDP.h>
#include <DevLabI2CBusRecovery.h>

#if !defined(ARDUINO_ARCH_ESP32)
  #error "temt6000WebGraph requires an ESP32"
#endif

#define I2C_BUS Wire

// User configuration.
constexpr uint8_t I2C_SDA_PIN = 6;
constexpr uint8_t I2C_SCL_PIN = 7;
constexpr uint32_t I2C_FREQUENCY_HZ = 100000;
constexpr uint8_t STARTUP_I2C_ADDRESS = 0x20;
constexpr uint8_t SENSOR_AVERAGING_SAMPLES = 24;
constexpr uint32_t SENSOR_SAMPLE_INTERVAL_MS = 25;
constexpr float ADC_REFERENCE_VOLTAGE = 3.3f;
constexpr char ACCESS_POINT_SSID[] = "TEMT6000-Sensor";
constexpr char ACCESS_POINT_PASSWORD[] = "temt6000";

constexpr uint32_t MIN_COMMAND_INTERVAL_MS = 25;

static_assert(
    SENSOR_AVERAGING_SAMPLES == 4 || SENSOR_AVERAGING_SAMPLES == 8 ||
    SENSOR_AVERAGING_SAMPLES == 16 || SENSOR_AVERAGING_SAMPLES == 24,
    "SENSOR_AVERAGING_SAMPLES must be 4, 8, 16 or 24");

WebServer webServer(80);
DevLabDDP::Master master(I2C_BUS, DevLabDDP::DEVICE_TEMT6000);
bool deviceVerified = false;
uint16_t latestSensorValue = 0;
bool latestSensorValueValid = false;
uint32_t lastSensorSampleMs = 0;
uint32_t lastCommandMs = 0;
bool commandWasSent = false;

const char WEB_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>TEMT6000 Light Sensor</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; }
    body { margin: 0; background: #0b1220; color: #e5eefc; }
    main { max-width: 960px; margin: auto; padding: 22px; }
    h1 { margin: 0 0 18px; font-size: 1.55rem; }
    .cards { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; }
    .card, .chart { background: #121d31; border: 1px solid #263653; border-radius: 14px; }
    .card { padding: 14px; }
    .label { color: #91a4c3; font-size: .8rem; text-transform: uppercase; }
    .value { margin-top: 5px; font-size: 1.65rem; font-weight: 700; }
    .chart { margin-top: 12px; padding: 12px; }
    canvas { display: block; width: 100%; height: 360px; }
    #status { margin-top: 10px; color: #91a4c3; }
    @media (max-width: 620px) { .cards { grid-template-columns: 1fr; } canvas { height: 300px; } }
  </style>
</head>
<body>
<main>
  <h1>TEMT6000 light sensor</h1>
  <section class="cards">
    <div class="card"><div class="label">ADC value</div><div id="adc" class="value">--</div></div>
    <div class="card"><div class="label">Voltage</div><div id="voltage" class="value">-- V</div></div>
    <div class="card"><div class="label">Full scale</div><div id="percent" class="value">-- %</div></div>
  </section>
  <section class="chart"><canvas id="graph"></canvas></section>
  <div id="status">Connecting to sensor...</div>
</main>
<script>
const samples = [];
const maxSamples = 240;
const canvas = document.getElementById('graph');
const context = canvas.getContext('2d');

function drawGraph() {
  const ratio = window.devicePixelRatio || 1;
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;
  if (canvas.width !== width * ratio || canvas.height !== height * ratio) {
    canvas.width = width * ratio;
    canvas.height = height * ratio;
  }
  context.setTransform(ratio, 0, 0, ratio, 0, 0);
  context.clearRect(0, 0, width, height);

  const padding = { left: 52, right: 12, top: 14, bottom: 28 };
  const plotWidth = width - padding.left - padding.right;
  const plotHeight = height - padding.top - padding.bottom;
  const values = samples.map(sample => sample.value);
  let low = values.length ? Math.min(...values) : 0;
  let high = values.length ? Math.max(...values) : 4095;
  const margin = Math.max(20, (high - low) * 0.2);
  low = Math.max(0, Math.floor(low - margin));
  high = Math.min(4095, Math.ceil(high + margin));
  if (high - low < 40) high = Math.min(4095, low + 40);

  context.strokeStyle = '#263653';
  context.fillStyle = '#91a4c3';
  context.font = '12px system-ui';
  context.lineWidth = 1;
  for (let line = 0; line <= 4; ++line) {
    const y = padding.top + (plotHeight * line / 4);
    const label = Math.round(high - ((high - low) * line / 4));
    context.beginPath();
    context.moveTo(padding.left, y);
    context.lineTo(width - padding.right, y);
    context.stroke();
    context.fillText(label, 4, y + 4);
  }

  if (samples.length < 2) return;
  context.strokeStyle = '#fbbf24';
  context.lineWidth = 2;
  context.beginPath();
  samples.forEach((sample, index) => {
    const x = padding.left + plotWidth * index / (maxSamples - 1);
    const y = padding.top + plotHeight * (high - sample.value) / (high - low);
    if (index === 0) context.moveTo(x, y); else context.lineTo(x, y);
  });
  context.stroke();
}

async function updateSensor() {
  try {
    const response = await fetch('/api/value', { cache: 'no-store' });
    const data = await response.json();
    if (!data.valid) throw new Error('I2C read failed');
    document.getElementById('adc').textContent = data.value;
    document.getElementById('voltage').textContent = data.voltage.toFixed(3) + ' V';
    document.getElementById('percent').textContent = data.percent.toFixed(1) + ' %';
    document.getElementById('status').textContent = 'Live · ' + data.averaging + '-sample moving average';
    samples.push({ value: data.value });
    if (samples.length > maxSamples) samples.shift();
    drawGraph();
  } catch (error) {
    document.getElementById('status').textContent = 'Sensor unavailable: ' + error.message;
  }
}

window.addEventListener('resize', drawGraph);
setInterval(updateSensor, 50);
updateSensor();
</script>
</body>
</html>
)HTML";

void waitForCommandInterval()
{
  if (!commandWasSent) return;
  uint32_t elapsedMs = millis() - lastCommandMs;
  if (elapsedMs < MIN_COMMAND_INTERVAL_MS) {
    delay(MIN_COMMAND_INTERVAL_MS - elapsedMs);
  }
}

uint8_t sendCommandByte(uint8_t command)
{
  waitForCommandInterval();
  I2C_BUS.beginTransmission(STARTUP_I2C_ADDRESS);
  I2C_BUS.write(command);
  uint8_t error = I2C_BUS.endTransmission();
  lastCommandMs = millis();
  commandWasSent = true;
  return error;
}

bool readResponseByte(uint8_t &response)
{
  delay(10);
  if (I2C_BUS.requestFrom(STARTUP_I2C_ADDRESS, (uint8_t)1) != 1) {
    while (I2C_BUS.available()) I2C_BUS.read();
    return false;
  }
  response = I2C_BUS.read();
  return true;
}

bool readAveragingConfiguration(uint8_t &sampleCount)
{
  if (sendCommandByte(CMD_GET_ADC_AVERAGING) != 0 || !readResponseByte(sampleCount)) {
    return false;
  }
  return sampleCount == 4 || sampleCount == 8 ||
         sampleCount == 16 || sampleCount == 24;
}

bool sendAveragingConfigurationByte(uint8_t value)
{
  if (sendCommandByte(value) != 0) return false;
  uint8_t response = 0xFF;
  return readResponseByte(response) &&
         (response & 0x0F) == RESP_ADC_AVERAGING_SET;
}

bool configureSensorAveraging()
{
  uint8_t currentSampleCount = 0;
  if (readAveragingConfiguration(currentSampleCount) &&
      currentSampleCount == SENSOR_AVERAGING_SAMPLES) {
    return true;
  }
  return sendAveragingConfigurationByte(CMD_SET_ADC_AVERAGING) &&
         sendAveragingConfigurationByte(SENSOR_AVERAGING_SAMPLES);
}

bool readLightSensor(uint16_t &value)
{
  uint8_t bytes[2];
  if (!deviceVerified ||
      !master.readCommand(STARTUP_I2C_ADDRESS, CMD_READ_ADC0, bytes, 2U, 30U)) return false;
  value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
  return true;
}

void handleRoot()
{
  webServer.send_P(200, "text/html", WEB_PAGE);
}

void handleSensorValue()
{
  String json = "{\"valid\":";
  json += latestSensorValueValid ? "true" : "false";
  json += ",\"value\":" + String(latestSensorValue);
  json += ",\"voltage\":" + String(latestSensorValue * ADC_REFERENCE_VOLTAGE / 4095.0f, 4);
  json += ",\"percent\":" + String(latestSensorValue * 100.0f / 4095.0f, 2);
  json += ",\"averaging\":" + String(SENSOR_AVERAGING_SAMPLES);
  json += "}";
  webServer.send(200, "application/json", json);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  bool i2cBusOk = devlabBeginI2cBusRecovered(
      I2C_BUS, I2C_SDA_PIN, I2C_SCL_PIN,
      I2C_FREQUENCY_HZ, 100);
  if (!i2cBusOk) {
    Serial.println("Error: I2C bus is blocked");
  }

  DevLabDDP::DeviceInfo info;
  deviceVerified = i2cBusOk && master.matchesExpectedDevice(STARTUP_I2C_ADDRESS, &info);
  if (!deviceVerified) {
    Serial.println("Error: address is not a DDP TEMT6000 (ID 0x0102)");
  }

  if (deviceVerified && configureSensorAveraging()) {
    Serial.printf("Sensor averaging: %u samples\n", SENSOR_AVERAGING_SAMPLES);
  } else {
    Serial.println("Warning: sensor averaging configuration failed");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ACCESS_POINT_SSID, ACCESS_POINT_PASSWORD);
  webServer.on("/", handleRoot);
  webServer.on("/api/value", handleSensorValue);
  webServer.onNotFound(handleRoot);
  webServer.begin();

  Serial.println("TEMT6000 access point ready");
  Serial.print("SSID: ");
  Serial.println(ACCESS_POINT_SSID);
  Serial.print("Open: http://");
  Serial.println(WiFi.softAPIP());
}

void loop()
{
  webServer.handleClient();

  uint32_t now = millis();
  if (now - lastSensorSampleMs >= SENSOR_SAMPLE_INTERVAL_MS) {
    lastSensorSampleMs = now;
    latestSensorValueValid = readLightSensor(latestSensorValue);
  }
}
