# Changelog

## Unreleased

### Battery Voltage Compensation

- Adds Battery Voltage Compensation: forward throttle below full stick is scaled by a
  voltage- and throttle-dependent amount so a fresh 2S pack feels like a partly used
  one. Full stick, neutral, brake, and reverse always pass through unchanged.
- Drives the compensation from a resting-voltage estimate sampled while the throttle
  is lifted, so a short burst sag cannot change the feel mid-corner, with an
  asymmetric drop/recovery filter as the alternative source. Any fault fades the
  compensation out over one second instead of stepping the throttle.
- Stores every compensation setting per profile: enable, start and end voltage,
  strength, curve (linear, expo, custom knee), voltage filter, drop and recovery
  rates, and the resting-voltage source. The sense pin, voltage scale, and throttle
  direction are global hardware settings.
- Adds a scrolling Battery Compensation page to the touch UI, a web configurator card
  with live graphs of compensation against pack voltage and throttle in against ESC
  out, and one-step multimeter calibration of the divider.
- Logs `battery_raw_v`, `battery_filtered_v`, `battery_resting_v`, `battery_comp_pct`,
  and `throttle_out_us` in the blackbox. Records grow from 244 to 264 bytes, which
  retains about 13 minutes in the 4 MB buffer.
- Needs a resistor divider from the pack to GPIO 5, 6, 7, or 8 (GPIO 8 recommended)
  and the ESC driven by OpenDrift; see `OpenDrift/docs/Hardware.md`. Sensing is off
  by default, so existing installs behave as before.
- Migrates saved profiles from the 76-byte version 10 layout to the 116-byte
  version 11 layout on first boot, and now also loads version 6 profiles, which
  shared the old size and could never load before. **Downgrading to v1.0.8 or older
  afterwards drops every saved profile**, because older firmware does not recognise
  the new size; note your tunes before flashing an older release.
- Adds a host-side test suite under `OpenDrift/hosttest` (plain `g++` and `make`)
  covering the compensation maths, profile migration, battery sensing, and the
  blackbox record layout.
- Review fixes: the throttle neutral is learned from the radio (1400-1600 us held
  still for 1 s), so trim and subtrim neither freeze the resting-voltage estimate nor
  count as forward throttle, and an estimate with no lift for 60 s follows the
  filtered voltage. A settings change that makes the voltage span invalid, a sensor
  or calibration change, and a lost link all fade instead of stepping. The web
  calibration is applied once the selected pin has a reading and reports its result;
  the preview mirrors reversed channels and the learned neutral, keeps emptied fields
  at their saved value, carries the firmware ranges on its inputs, and cannot be hung
  by pasted input. The blackbox logs 0 % while no ESC output is active and keeps a
  CSV row that does not fit its buffer on its own line. Sense-pin and curve values
  from the web are clamped as integers, and selecting a sense pin clears its aux
  channel so the divider pin never resumes as an accessory output.

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
