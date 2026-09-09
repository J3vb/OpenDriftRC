# OpenDrift Hardware Notes

## Primary board

The final supported target is the Waveshare ESP32-S3 Touch AMOLED 1.64. The older Waveshare 1.28-inch round display is deprecated and frozen; its existing source remains available for experimentation but receives no new releases or feature-parity work.

The AMOLED board has incompatible V1 and V2 revisions. V1 is marked at the top of the PCB and uses LCD_CS GPIO 9. V2 is marked beside the right-side headers and uses LCD_CS GPIO 46. V2 also connects IMU_INT2 to GPIO 17 and TP_INT to GPIO 18, so OpenDrift does not use GPIO 17/18 for external signals on V2.

## QMI8658 IMU

Both boards use the QMI8658 six-axis IMU. OpenDrift uses body Z as yaw and records body X/Y gyro plus all three accelerometer axes for terrain and load-transfer analysis.

| Board | SDA | SCL |
| --- | ---: | ---: |
| AMOLED 1.64 | GPIO 47 | GPIO 48 |
| Round 1.28 | GPIO 6 | GPIO 7 |

The current board orientation reports clockwise rotation as positive Z and counter-clockwise rotation as negative Z. Always verify correction direction by rotating the complete car before driving.

## PWM receiver and servo routing

AMOLED V1 and the Round build use the same PWM pinout:

| Signal | GPIO | Direction |
| --- | ---: | --- |
| Receiver steering / servo in | 15 | Input |
| Receiver throttle / throttle in | 16 | Input |
| Steering servo / servo out | 17 | Output at selectable 250/333 Hz |
| Gain input or throttle passthrough | 18 | Selectable |

GPIO 18 can be a receiver gain input or an ESC throttle output, but not both.
To retain throttle sensing and receiver gain control simultaneously, split the
receiver throttle signal between GPIO 16 and the ESC instead of connecting the
ESC to GPIO 18.

AMOLED V2 PWM uses GPIO 15 steering input, GPIO 16 throttle input, GPIO 1 steering-servo output, and GPIO 2 as the selectable gain input or throttle output.

## CRSF routing

AMOLED V1 and Round CRSF share this routing:

| Signal | GPIO | Direction |
| --- | ---: | --- |
| CRSF RX from receiver TX | 17 | Input |
| CRSF TX to receiver RX | 18 | Output |
| Steering servo / servo port | 15 | Output at selectable 250/333 Hz |
| ESC throttle / throttle port | 16 | Output at 50 Hz |

Both `waveshare_128_crsf` and `waveshare_amoled_164_crsf` enable the complete
full-duplex path. They remain separate from the normal PWM environments because
the GPIO routing and settings namespace differ.

AMOLED V2 CRSF uses GPIO 1 RX, GPIO 2 TX, GPIO 15 steering-servo output, and GPIO 16 ESC output. Its targets are `waveshare_amoled_164_v2` and `waveshare_amoled_164_v2_crsf`.

CRSF channel mapping is channel 1 steering, channel 2 throttle, and channel 3
gain. A stale channel frame centers steering and commands neutral throttle.
Throttle output requires a valid link and a 500 ms neutral hold
before arming.

## CRSF auxiliary channel outputs

The AMOLED CRSF builds can mirror any CRSF channel from 1 through 16 to a
standard 50 Hz receiver-style PWM signal. Assign each pin independently in the
WiFi web configurator under **Auxiliary Channel Outputs**.

| Board | Available auxiliary GPIOs |
| --- | --- |
| AMOLED V1 CRSF | GPIO 1–8 |
| AMOLED V2 CRSF | GPIO 3–8 |

GPIO 1/2 are unavailable on V2 because they carry the CRSF UART. Disabled pins
remain inputs. Enabled pins output the selected channel and command 1500
microseconds when the CRSF link is lost. GPIOs provide a 3.3 V signal only;
lights, controllers, or other accessories require their own appropriate power
supply and a common ground with OpenDrift.

## Throttle sensing

Throttle sensing is electrically optional and automatically falls back when no valid PWM signal exists. It is strongly recommended for OpenDrift v1.0 because it announces power and chassis-load changes, temporarily extends yaw prediction, and makes the slow drift reference yield before stale feedback can fight the transition.

Only the receiver signal and a shared ground are required. OpenDrift does not power the receiver or ESC through the throttle input.

## Power and grounding

The receiver, ESP32 board, and servo/ESC system must share ground. Power the steering servo from an appropriate BEC or receiver rail; do not draw servo current through the ESP32 board.

Feed a DIY OpenDrift display/development board with regulated 5 V on its 5 V
input. Do not connect a 6 V or higher BEC directly to that input. The OpenDrift
daughter boards under development include an onboard regulator so the builder
does not need to add a separate 5 V regulator. ESP32-S3 GPIOs remain 3.3 V logic
and are not 5 V tolerant; every external GPIO is a 3.3 V signal only.

Fast drift servos can draw large transient current and can oscillate from their own internal settings. Verify servo stability directly from the receiver before diagnosing the gyro.

OpenDrift defaults to 250 Hz for broad digital-servo compatibility. The optional 333 Hz control and servo rate is only for servos whose manufacturer explicitly supports 333 Hz. Restart the board after changing the rate.

## Battery voltage sense

Battery Voltage Compensation needs to read the car's 2S pack. Nothing on the Waveshare
board or the OpenDrift daughter board measures it, so a small resistor divider must be
added. The feature stays inert until the sense pin is selected in the web configurator.

| Board | Sense GPIO | ADC | Header location on the daughter board |
| --- | ---: | --- | --- |
| AMOLED V1 / V2 | 8 (also 5, 6, 7) | ADC1, usable with WiFi on | J6 pin 5 (GPIO 8); J6 pins 6, 7, 8 are GPIO 7, 6, 5 |
| Round 1.28 | none | GPIO 5–8 are used by touch, I2C and the display | not available |

GPIO 8 is the documented choice. On CRSF builds the selected sense pin is removed from the
auxiliary channel output list automatically.

Circuit, three parts:

```
pack +  --[ R1 47k 1% ]--+--[ R2 15k 1% ]-- GND (OpenDrift ground)
                         |
                         +--[ C1 100 nF ]-- GND
                         |
                         +----------------- GPIO 8 (J6 pin 5)
```

- Ratio (47 + 15) / 15 = 4.133: 8.4 V becomes 2.03 V at the pin. The firmware default
  voltage scale is 4.133; the one-time calibration below removes resistor tolerance.
- Drain is 0.14 mA. Unplug the sense lead together with the pack if the car is stored with
  the battery connected.
- A 3S pack reads 3.05 V at the pin and is rejected by firmware (anything above 9.2 V is a
  fault). A 4S pack or reversed lead is limited by R1 to well under 1 mA into the pin's
  protection diode. An optional BAT54S clamp to 3V3 and GND at the pin is cheap insurance on
  a board revision.
- Never connect the pack directly to a GPIO. ESP32-S3 pins are 3.3 V maximum.

Where to tap the pack: the 2S balance plug (JST-XH, three pins: black = GND, middle = cell 1,
outer = pack +). Use a balance extension or breakout, take only the outer pack + wire to R1,
and leave the balance GND unconnected; OpenDrift's ground is already the battery negative
through the ESC. Alternative taps: the ESC's battery + input or the ESC side of the power
switch. Route the sense wire away from the motor wires.

Hand-wired: solder R1 and R2 inline in the sense wire or on a scrap of perfboard with C1,
heat-shrink it, output to header pin J6 pin 5 and ground to any daughter-board GND pin.

Next daughter-board revision: a 2-pin JST-PH VBAT input (pack +, GND), R1, R2, C1 and the
optional BAT54S on the board, traced to the GPIO 8 header pin.

Calibration, once: power the car, read the pack with a multimeter at the balance plug, type
the value into "Measured pack voltage" on the web configurator and save. The scale is
applied as soon as the selected pin has a reading (the sense pin can be selected in the same
save) and the Calibration pill on the card confirms it. The live readout should then match
the meter within about 0.05 V at rest.

The compensation only acts when the ESC is driven by OpenDrift: on CRSF builds (GPIO 16, or
GPIO 15 on the `waveshare_amoled_164_crsf_oops_swapped` recovery build), and on PWM builds
only with GPIO 18 (V1) or GPIO 2 (V2) in THROTTLE OUT mode with the ESC plugged into that
output.
