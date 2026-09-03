# Changelog

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
