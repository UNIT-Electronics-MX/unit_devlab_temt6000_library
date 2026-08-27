# TEMT6000 DDP examples

| Example | Purpose |
|---|---|
| `adc/singleSensor` | Read ADC0 through Serial. |
| `adc/adcAveragingSampler` | Configure moving-average sampling and read ADC0. |
| `adc/sensorOledGraph` | Display samples on an SSD1306 OLED. |
| `adc/temt6000WebGraph` | Display a web graph from an ESP32. |
| `i2c/changeAddress` | Interactively change the address of a TEMT6000 DDP node. |

The address example verifies Device ID `0x0102`. It scans the bus at startup
without modifying any device and scans again after a successful address change.
Open Serial at 115200 baud and enter, for example:

```text
scan
change 20 30
```

ESP32 uses SDA GPIO6 and SCL GPIO7. RP2040/RP2350 uses `Wire1`, SDA GPIO12
and SCL GPIO13. Compile every Arduino example with `make examples-check`.
