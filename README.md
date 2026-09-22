# DevLab_TEMT6000

Arduino library for the TEMT6000 ambient light sensor running the DevLab Device
Protocol (DDP) over I2C.

This library does not talk to the sensor directly. It is a thin entry point
(`DevLab_TEMT6000.h`) that pulls in the shared [`DevLabDDP`](https://github.com/UNIT-Electronics-MX/unit_devlab_ddp_library)
master, which discovers, identifies and reads any DDP node over I2C -
including the TEMT6000. All sensor access goes through `DevLabDDP::Master`
and the [`DevLab_Interface`](https://github.com/UNIT-Electronics-MX/unit_devlab_interface_library)
`DevLab_I2C_Orchestrator` bus class.

Compatible with ESP32 and RP2040/RP2350 boards.

---

# Features

- Device discovery and identity verification (device ID `0x0102`) before any read
- Raw ADC0 light reading (12-bit, 0-4095) via the DDP command/response protocol
- Configurable on-device moving-average window (1, 4, 8, 16 or 24 samples)
- Runtime I2C address scanning and reassignment (no fixed address required)
- I2C bus recovery on startup (clears a slave stuck mid-transaction)
- Object-oriented, lightweight implementation
- Supports custom I2C pins and clock speed

---

# Supported Interfaces

| Interface | Support Status |
|---|---|
| I2C | Supported |

---

# Installation

## Manual Installation

1. Open Arduino IDE
2. Go to:

```text
Sketch -> Library Manager -> Search DevLab_TEMT6000...
```

3. Click on Install
4. Install its dependencies the same way: `DevLabDDP` and `DevLab_Interface`
   (the Library Manager installs both automatically if it resolves
   dependencies; otherwise install them manually)
5. Compile and upload the examples for the sensor

---

# Quick Start Example

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <DevLabDDP.h>
#include <DevLab_I2C_Orchestrator.h>

constexpr uint32_t I2C_FREQ = 400000;
constexpr uint8_t SENSOR_ADDRESS = 0x20;
constexpr uint8_t I2C_SDA = 6;   // ESP32: 6/7. RP2040/RP2350: 24/25.
constexpr uint8_t I2C_SCL = 7;

DevLab_I2C_Orchestrator bus(Wire, I2C_FREQ);
DevLabDDP::Master master(bus, DevLabDDP::DEVICE_TEMT6000);
bool deviceVerified = false;

void setup() {
  Serial.begin(115200);
  delay(500);

  if (!bus.beginRecovered(I2C_SDA, I2C_SCL, 20000, false)) {
    Serial.println("ERROR: I2C bus is blocked");
    return;
  }

  DevLabDDP::DeviceInfo info;
  deviceVerified = master.matchesExpectedDevice(SENSOR_ADDRESS, &info);
  if (!deviceVerified) {
    Serial.println("ERROR: address is not a DDP TEMT6000 (ID 0x0102)");
  }
}

void loop() {
  uint16_t raw;
  if (deviceVerified && master.readAdc(SENSOR_ADDRESS, 0, raw)) {
    Serial.println(raw);
  } else {
    Serial.println("ERR");
  }
  delay(1000);
}
```

See [`examples/adc/singleSensor`](examples/adc/singleSensor) for the full,
commented version, and [`examples/README.md`](examples/README.md) for every
other example (averaging, OLED graph, web dashboard, address change).

---

# Wiring Example

| TEMT6000 | MCU |
|---|---|
| SDA | SDA |
| SCL | SCL |
| VDD | 3.3V |
| GND | GND |

The device ships at I2C address `0x20`. Address is not pin-selected: use
[`examples/i2c/changeAddress`](examples/i2c/changeAddress) to scan the bus
and reassign it at runtime through the DDP protocol.

---

# Compatibility

| MCU Platform | Status | Default I2C pins (in examples) |
|---|---|---|
| ESP32 | Tested | SDA 6 / SCL 7 |
| RP2040 / RP2350 | Tested | SDA 24 / SCL 25 |
| Other Arduino-compatible boards | Compatible (define `I2C_SDA`/`I2C_SCL` for your board) | - |

---

# Notes

- The TEMT6000 DDP node reports device ID `0x0102`; `matchesExpectedDevice()`
  rejects any other device found at the configured address.
- ADC reads (`CMD_READ_ADC0`) return the device's background-averaged value,
  not a raw instantaneous sample - the moving-average window
  (`CMD_SET_ADC_AVERAGING` / `CMD_GET_ADC_AVERAGING`) only changes how many
  samples feed that average.
- `DevLab_I2C_Orchestrator::beginRecovered()` toggles SCL to release a slave
  left mid-transaction (e.g. after a reset) before calling `Wire.begin()`.
  Prefer it over a plain `begin()` on shared or hot-pluggable buses.
- Current library version supports only I2C communication.

---

# Folder Structure

```text
DevLab_TEMT6000/
├── examples/
│   ├── adc/
│   │   ├── singleSensor/
│   │   ├── adcAveragingSampler/
│   │   ├── sensorOledGraph/
│   │   └── temt6000WebGraph/
│   └── i2c/
│       └── changeAddress/
├── src/
│   └── DevLab_TEMT6000.h
├── library.properties
├── README.md
└── LICENSE
```

---

# Version

| Parameter | Value |
|---|---|
| Library Name | DevLab_TEMT6000 |
| Version | 2.0.1 |
| Communication | I2C |
| Architecture | Cross-platform |
| Dependencies | DevLabDDP, DevLab_Interface |

---

# Author

Adrian Rabadan | Cesar Bautista | Jonathan Mejorado | Fernando Flores

UNIT Electronics - DevLab Ecosystem

---

# License

MIT License
