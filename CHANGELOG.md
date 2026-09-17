# Changelog

## Unreleased

### Display and web configurator

- Adds a Display page with a 180 degree screen flip for an upside-down board;
  the rendered image and the touch input rotate together.
- Adds a display theme: a selectable accent colour, light or dark text, and
  translucent panels behind the controls so any background stays readable.
- Adds uploadable AMOLED backgrounds, stored on the board and selectable from
  the display or the web configurator.
- Adds a display brightness setting and a configurable idle dim timeout.
- Adds profile export and import, plus a JSON export of every setting, the
  endpoint calibration and all profiles.
- Adds physical servo endpoint capture and reset from the web configurator,
  and a Restart and Factory reset button.
- The web configurator has five tabs (Tune, Servo, Radio, Profiles, Board)
  and a sticky header with Save and an unsaved-changes indicator, so a phone
  no longer scrolls the whole page to save one value.

### Fixes

- Legacy profile migration no longer drops profiles or shifts the active
  profile index.
- The ESC neutral failsafe now runs in the control task, so it cannot be
  delayed by the UI or the web server. The CRSF neutral-hold arming is also
  cleared there on link loss, so a loop blocked in a web download cannot
  re-apply the receiver's throttle the moment the link returns; the PWM
  build holds neutral until the loop removes the signal.
- The gyro calibrate button no longer races the control task.
- The Drive page gain and deadband buttons now edit the saved value instead of
  the live gain, so an edit is no longer overwritten by the gain channel. The
  Drive page shows the saved value under the live one while a gain channel
  is overriding it.
- The web save rejects NaN and empty numbers and clamps the deadband.
- Servo center and travel are locked while a physical endpoint calibration
  is active; the Steering page reports a refused change. Servo reverse keeps
  working after calibration by swapping the captured left and right stops.
- Splits the EdgeTX tool's gain into Saved Gain and a read-only Live Gain,
  adds an editing guard and a BUSY indicator, and handles EXIT correctly.
- Removes dead files left over from the original round-display port.
- Auxiliary CRSF outputs no longer share the MCPWM timer that ESP32Servo
  uses for the steering servo; assigning a channel to GPIO 1 or 2 on a V1
  board previously reprogrammed the steering PWM to 50 Hz.
- Gyro bias calibration averages half a second of samples and rejects the
  window when the car moved, at boot and from the CAL button, instead of
  storing one raw sample. The Drive page shows `CAL OK` or `HOLD STILL`.
- Gyro and accelerometer read failures are counted separately, so an
  accelerometer fault cannot disable steering, and a failed LPF mode write
  backs off instead of toggling the sensor every tick.
- The control task waits at most 2 ms for the I2C bus and no longer replays
  missed ticks after a stall.
- Servo center, travel, endpoints, gain range and WiFi timeout are clamped
  when set and when loaded.
- The web form no longer refuses to save after the EdgeTX tool stored a
  fractional deadband, and a page opened before a calibration was cleared
  elsewhere cannot flip servo reverse on save.
- The WiFi client count falls back to the station list after a minute
  without station events, so a missed disconnect event can no longer keep
  the access point on forever.
- The web save only writes a steering endpoint the user edited, so a page
  opened before the stops were captured, reset or swapped elsewhere no
  longer writes the old values back and re-marks them calibrated. Servo
  reverse from the web page now swaps the calibrated stops like the display
  and the EdgeTX tool do; before, the same save undid the swap.
- The web endpoint capture buttons refuse while a calibration is active
  instead of silently re-storing the current stop.
- Gyro Reverse and the gyro bias now share one frame: the boot calibration
  measures the bias on the reversed signal, and toggling Gyro Reverse flips
  the stored bias instead of doubling it.
- A gyro filter change that fails on the I2C bus no longer leaves the
  gyroscope disabled until reboot.
- The EdgeTX tool sends at most one request per frame, so the BUSY flag no
  longer stays lit and the Endpoints status is polled as intended.
- The screen flip is included in the settings export and on the web
  Display card.

- The controller's chassis direction-change trigger now keeps its own
  direction memory through the quiet band, so a real yaw reversal arms the
  transition phase at any control rate. Previously it only fired when the
  filtered yaw crossed plus or minus 7 deg/s inside one tick, which left
  Transition Speed driven almost only by stick movement. Expect transitions
  to feel more damped; re-check Transition Speed.
- The controller measures driver activity on the normalized steering
  command, so Radio Steering Travel no longer makes the driver look calmer
  or busier. Tunes running travel below 100 percent will see the assists
  come in slightly later than before.

### Known limitations

- Throttle prediction reacts to the size of a throttle change in either
  direction; brake and throttle stabs count the same. This is deliberate and
  keeps reversed-throttle ESCs working.
- A settings save can mask the CRSF UART interrupt for a few milliseconds
  per flash write; the 50 ms link-loss window absorbs it.
- The blackbox CSV download blocks the loop task for the whole transfer.
  Steering and the ESC run in the control task and are unaffected.

## v1.0.8 - 2026-09-03

### Lower-latency gyro experiments

- Adds selectable QMI8658 gyro-filter modes: the original `24 Hz`, a
  lower-latency `120 Hz` mode, and hardware LPF bypass for controlled testing.
- Makes Smoothing `0.00` a true software-filter bypass so hardware and software
  phase delay can be evaluated independently.
- Raises the supported gyro Gain ceiling from `3.00` to `6.00` while retaining
  the existing `0.50-3.00` Channel 3 mapping by default.

### Correction authority

- Refactors the controller to return a signed gyro correction instead of a
  centered pseudo-servo command, eliminating an unintended internal
  `1000-2000 us` saturation point.
- Redefines Max Correction across the full endpoint-to-endpoint steering span:
  `50%` can move from center to one endpoint, while `100%` can override one
  endpoint all the way to the other.
- Migrates existing Max Correction settings and saved profiles to preserve
  their real correction authority. An old displayed value of `74` becomes
  approximately `37` without weakening or doubling the tune.
- Stops Transition Speed from dynamically shrinking the Max Correction ceiling.
  It now shapes transition damping only.
- Keeps the combined driver-plus-gyro command and calibrated physical servo
  endpoints as the final hard safety limits.

### Channel 3 and EdgeTX

- Adds persistent `CH3 Gain Min` and `CH3 Gain Max` controls to the EdgeTX Lua
  tool and onboard web configurator, adjustable from `0.00` to `6.00`.
- Keeps Channel 3 authoritative while its receiver signal is valid and reports
  the resulting live gain consistently to the display and EdgeTX tool.
- Adds the gyro LPF selector and expanded `0.00-6.00` Active Gain range to the
  current `OpenDrift.lua` release asset.

### Blackbox and documentation

- Adds `gyro_lpf_mode` so matched filter tests identify the active sensor mode.
- Replaces ambiguous correction columns with `gyro_requested_us`,
  `gyro_limited_us`, and `gyro_applied_us`.
- Adds `correction_saturated` to distinguish Max Correction clipping from final
  steering-range saturation during entries and transitions.
- Updates the tuning reference and website for the new gain range, filter modes,
  full-span Max Correction behavior, tune migration, and test procedure.

### Release targets

- Publishes Waveshare AMOLED 1.64 V1 and V2 firmware with PWM and CRSF receiver
  support.
- The deprecated Waveshare Round 1.28 builds remain available at v1.0.7c and
  are not part of the v1.0.8 public release.

## v1.0.7c - 2026-08-30

### Physical steering limits

- Replaces receiver-range calibration with physical servo endpoint calibration.
- Captures the servo's actual left, center, and right PWM positions and uses
  them as the final asymmetric output map and hard safety clamp.
- Invalidates v1.0.7b receiver-range captures because they are not safe to
  reinterpret as physical servo limits.
- Redefines Max Correction as `0-100%` of calibrated physical steering travel;
  existing settings and profiles migrate from the previous +/-500 us scale.
- Makes Steering Travel affect driver input only, leaving gyro authority to
  Max Correction and the calibrated physical endpoints.
- Gives both AMOLED and round displays dedicated red/green physical endpoint
  pages and keeps the same captures synchronized with the EdgeTX tool.

## v1.0.7b - 2026-08-29

### Steering calibration

- Moves AMOLED steering calibration onto its own page with three large capture targets.
- Gives each endpoint a red-to-green confirmation state and reports missing signal or invalid endpoint ordering directly on screen.
- Fixes stale touch hitboxes that could interpret endpoint taps as swipe gestures.
- Normalizes captured left/right PWM values so reversed transmitter channels calibrate correctly.
- Persists shared left/center/right capture state and synchronizes calibration status between the AMOLED page, web configurator, CRSF device, and EdgeTX tool.
- Adds `Capture Left`, `Capture Center`, and `Capture Right` actions to `OpenDrift.lua` with an always-visible calibration status.

## v1.0.7 - 2026-08-27

### Control

- Replaces event-gated hunt damping with a narrow phase-aware dynamic notch targeting the measured 2.5-3.6 Hz wheel-wobble mode.
- Keeps a shallow guard active through entries and transitions, then blends smoothly to full settled-drift depth.
- Tracks notch center frequency only during suitable settled conditions so deliberate chassis motion does not retune the filter.
- Improves transition prediction and damping through the complete yaw reversal.
- Renames the user-facing **Hunt Strength** control to **Anti Wobble** and keeps its fresh-install default at `50`.
- Preserves existing saved tunes by retaining the compatible preference and profile storage layout.

### Reliability and interface

- Defers CRSF UART startup until the AMOLED, IMU, UI, and shared resources are ready.
- Adds a deliberate AMOLED hardware-reset sequence and startup settling delay for more reliable power-up and rapid power-cycle recovery.
- Makes WiFi auto-off count from the most recent client disconnect instead of initial startup.
- Enlarges the AMOLED steering endpoint buttons for easier trackside calibration.

### Blackbox and documentation

- Adds dynamic-notch residual, removed correction, envelope, latch, consistent-cycle, and tracked-frequency telemetry.
- Renames the saved blackbox control column from `hunt_strength` to `anti_wobble`.
- Updates the technical guide, web tuning guide, web configurator help, CRSF documentation, and EdgeTX tool for Anti Wobble and current Transition Speed behavior.

### Hardware

- Corrects the OpenDrift daughterboard regulator enable/feedback connections and updates the PCB routing to match the repaired schematic.
- Waveshare AMOLED 1.64 V1 and V2 PWM/CRSF builds are included.
- The deprecated Waveshare Round 1.28 firmware remains frozen at v1.0.2.

## v1.0.6 - 2026-08-17

- Added selectable 250/333 Hz control and steering-servo output.
- Improved transition authority and off-throttle prediction.
- Added the first bounded settled-drift hunt-suppression implementation and supporting blackbox telemetry.

## v1.0.5 - 2026-08-14

- Replaced internal-flash blackbox writes with non-blocking PSRAM circular logging.

## v1.0.4 - 2026-08-12

- Synchronized live channel-3 gain across the display, web configurator, CRSF telemetry, and EdgeTX tool.
