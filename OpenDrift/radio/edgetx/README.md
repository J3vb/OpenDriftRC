# [OpenDrift EdgeTX tool](https://github.com/doublej380-pixel/OpenDriftRC/releases/download/v1.0.7c/OpenDrift.lua)

This tool supports the AMOLED V1 and V2 **full-duplex** CRSF firmware targets:
`waveshare_amoled_164_crsf` and `waveshare_amoled_164_v2_crsf`.

Download [`OpenDrift.lua`](https://github.com/doublej380-pixel/OpenDriftRC/releases/download/v1.0.7c/OpenDrift.lua), copy it to `SCRIPTS/TOOLS/OpenDrift.lua` on the radio SD card,
then launch **OpenDrift** from the [EdgeTX Tools menu](https://github.com/doublej380-pixel/OpenDriftRC/releases/download/v1.0.7c/OpenDrift.lua).

CRSF wiring for the full-duplex firmware:

- Receiver TX to OpenDrift GPIO 17
- Receiver RX to OpenDrift GPIO 18
- Receiver and OpenDrift grounds connected

Use the roller to select a setting, press it to enter edit mode, rotate to
change the value, and press again to finish. Changes are applied live and are
saved by OpenDrift's normal delayed settings writer. A successful radio write
also requests an immediate refresh of the current OpenDrift display page.

Physical endpoint calibration sets the servo's hard output limits and is shared with both displays. The header and
`Endpoints` row show `NO`, `PARTIAL`, or `YES`. To calibrate from the radio:

1. Hold the steering at its safe physical left stop, select `Capture Left`, and press Enter.
2. Release to neutral, select `Capture Center`, and press Enter.
3. Hold full right, select `Capture Right`, and press Enter.

The action rows show `HOLD POSITION + ENTER` while selected. After all three
valid captures, the status changes to `YES` and the AMOLED calibration buttons
turn green. Capturing on the AMOLED page updates the radio status as well.
Use `Reset Cal` before expanding or replacing existing endpoints.

`Active Gain` follows CRSF channel 3 live. The tool shows a reminder that
channel 3 overrides gain changes made elsewhere while its signal is valid; the
stored profile gain remains the fallback used without that gain signal.

The tool exposes the gyro and steering values: Active Gain, Deadband, Max Correction,
Smoothing, Drift Memory, Memory Limit, Hold Assist, Countersteer, Transition
Speed, Prediction, Anti Wobble, Servo Quiet, Steering Travel, physical endpoints,
Servo Travel, Servo Center,
Servo Reverse, and Gyro Reverse. It also assigns CRSF channel 1–16 or OFF to
GPIO 1–8 on AMOLED V1 and GPIO 3–8 on AMOLED V2. GPIO 1/2 display `RES` on V2
because those pins carry the CRSF UART.
