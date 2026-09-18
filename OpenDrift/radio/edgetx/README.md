# [OpenDrift EdgeTX tool](https://github.com/doublej380-pixel/OpenDriftRC/releases/download/v1.0.8/OpenDrift.lua)

This tool supports the AMOLED V1 and V2 **full-duplex** CRSF firmware targets:
`waveshare_amoled_164_crsf` and `waveshare_amoled_164_v2_crsf`.

Download [`OpenDrift.lua`](https://github.com/doublej380-pixel/OpenDriftRC/releases/download/v1.0.8/OpenDrift.lua), copy it to `SCRIPTS/TOOLS/OpenDrift.lua` on the radio SD card,
then launch **OpenDrift** from the [EdgeTX Tools menu](https://github.com/doublej380-pixel/OpenDriftRC/releases/download/v1.0.8/OpenDrift.lua).

CRSF wiring for the full-duplex firmware:

- AMOLED V1: receiver TX to OpenDrift GPIO 17, receiver RX to GPIO 18
- AMOLED V2: receiver TX to OpenDrift GPIO 1, receiver RX to GPIO 2
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

`Saved Gain` is the stored fallback gain and is the value the tool edits.
`Live Gain` is read-only and follows CRSF channel 3 while its signal is valid;
channel 3 overrides the saved gain whenever it is connected, so the two rows
differ during a session and agree once the gain channel is removed.
`CH3 Gain Min` and `CH3 Gain Max` map the full Channel 3 control movement to
the desired gyro-gain range. The default remains `0.50` to `3.00`, while both
the controller and Channel 3 mapping support values up to `6.00`.

The tool exposes the gyro and steering values: Saved Gain, Live Gain, Channel 3 gain range, Deadband, Max Correction,
Smoothing, Gyro LPF, Drift Memory, Memory Limit, Hold Assist, Countersteer, Transition
Speed, Prediction, Anti Wobble, Servo Quiet, Steering Travel, physical endpoints,
Servo Travel, Servo Center, Servo Rate,
Servo Reverse, and Gyro Reverse. `Servo Travel` and `Servo Center` are refused
while the physical endpoint calibration is active; use `Reset Cal` first. The
same applies to the capture actions, which are refused once the endpoints are
calibrated. Restart OpenDrift after changing `Servo Rate`; check your servo is
rated for 333 Hz. It also assigns CRSF channel 1–16 or OFF to
GPIO 1–8 on AMOLED V1 and GPIO 3–8 on AMOLED V2. GPIO 1/2 display `RES` on V2
because those pins carry the CRSF UART.

`Gyro LPF` selects the QMI8658 hardware filter: `24 Hz` is the original
low-bandwidth mode, `120 Hz` reduces sensor phase delay, and `Off` bypasses the
sensor LPF. `Smoothing 0.00` is a true software-filter bypass. Blackbox logs
record the active selection in `gyro_lpf_mode` as 0, 1, or 2 respectively.
