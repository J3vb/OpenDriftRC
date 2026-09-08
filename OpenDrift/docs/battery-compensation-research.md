# Battery Voltage Compensation — research

Phase 1 output for the Battery Voltage Compensation feature. Read-only findings about the
firmware as it stands at v1.0.8 (commit `5f63095`); every `file:line` reference points at
that revision. The plan that follows from these findings is in
[battery-compensation-plan.md](battery-compensation-plan.md).

The eight research questions are answered in order (A1–A8), followed by the risk list.

## Findings

### A0. Repo shape

- `OpenDrift/` is a PlatformIO + Arduino project for the ESP32-S3 (Waveshare AMOLED 1.64,
  `boards/waveshare_amoled_164.json`; 240 MHz, OPI PSRAM, 16 MB flash, single-precision FPU).
- Eight envs in `platformio.ini`; the public ones are AMOLED V1/V2 × PWM/CRSF. Variant flags:
  `OPENDRIFT_BOARD_AMOLED_164`, `OPENDRIFT_AMOLED_V2`, `OPENDRIFT_INPUT_CRSF`. The round
  display builds (`lolin_s3_mini`, no `psram_type`) are frozen but must still compile.
- Settings live in NVS via `Preferences`, namespace `OpenDrift` / `OpenDriftCRSF` /
  `OpenDriftR51` by build (`Settings.cpp:171-183`). NVS partition is 20 KB (`partitions.csv`).
- Toolchain here: g++ 13, clang, make, cmake, python 3.11. No `pio`. No tests, no CI files
  (`.github/workflows` is gitignored on purpose).

### A1. Control loop and throttle path

| Context | Where | Rate |
|---|---|---|
| `controlTask` → `runControlIteration()` (IMU, gyro, steering servo) | `src/main.cpp:1069-1090`, created `:1824-1833`, core 1 prio 4 | fixed `vTaskDelayUntil`, 4 ms (250 Hz) or 3 ms (333 Hz); `controlLoopHz` `:72-73`, `:1269-1270` |
| `crsfTask` → `crsf.update()` | `src/main.cpp:1052-1066` | 1 ms delay loop |
| Arduino `loop()` (touch, settings, WiFi, web, **ESC write**, UI, blackbox) | `src/main.cpp:1848-2115` | free-run, `delay(1)` at `:2114` |

- `dt` is measured, not passed: `GyroController.cpp:159-176` from `micros()` (fallback
  0.004 f, clamped 1–50 ms). `update()` has no dt parameter (`GyroController.h:12-18`).
- Throttle is an `int` in raw microseconds (~1000–2000, neutral 1500) end to end. No
  normalised form exists anywhere.
- CRSF path: decode `CrsfInput.cpp:443-478` → µs `:507-525` (988–2012) → `main.cpp:807-811`
  → validity gate `RadioInput.cpp:56-68` → signal flag `main.cpp:909` → neutral substitution
  `:915-918` → `gyro.update(...)` `:939-946` → snapshot for `loop()` `:1015-1016` →
  `updateCrsfThrottleOutput()` `:1947-1950` → arming/failsafe `:600-674` → **ESC write
  `src/main.cpp:676-682`** `throttleOutput.writeMicroseconds(constrain(throttlePulse,1000,2000))`.
  Arming: ±50 µs neutral for 500 ms (`:136-137`); link loss writes 1500 (`:600-613`).
- PWM path: ISR capture `RadioInput.cpp:199-226` on GPIO 16 (`main.cpp:145`) → signal flag
  (250 ms timeout) → **passthrough write `src/main.cpp:1982-1985`**
  `throttleOutput.writeMicroseconds(throttleRadio.getPulseWidth())` on GPIO 18 (V1) / 2 (V2),
  only when the shared pin is in THROTTLE OUT mode (`pin18ThrottleOutputMode`,
  `settings.getThrottleOutputEnabled()`). Signal loss detaches the output (`:1987-2001`).
- No deadband, expo, endpoint or reverse processing exists on throttle. `EscOutput::configure(1500,
  false, 100, 0)` makes `writeMicroseconds` an identity map with a final 1000–2000 clamp
  (`EscOutput.cpp:63-117`); the LEDC hardware holds 50 Hz between writes.
- The wire is signed about 1500 (below = brake/reverse). The controller discards the sign
  (`fabsf`, `GyroController.cpp:236-240`). There is no brake handling anywhere.
- Throttle-dependent logic already in the controller: prediction horizon, quiet/settled
  blends gating Drift Memory, Countersteer Assist and Anti Wobble (`GyroController.cpp:236-366`,
  `458-477`, `599-636`, `999-1017`, `1133-1147`, `1226-1242`). No throttle-dependent gain.
- ESC write cadence: once per `loop()` iteration (tens of Hz to ~1 kHz depending on UI
  work), priority 1. Comment at `main.cpp:1013-1014` explains why the ESC write is not in the
  control task (ESP32Servo/LEDC allocation must not run at high priority).
- **The gyro is only in the ESC signal path on CRSF builds, or on PWM builds when GPIO 18/2 is
  set to THROTTLE OUT.** A default PWM install senses throttle on GPIO 16 while the receiver
  drives the ESC directly; the feature can do nothing there. The user's daughter board
  (`Pcb/OpenDrift Pdb`, connectors `Esc_In`/`Esc_out`) routes the ESC through OpenDrift, so
  the intended install is covered.

### A2. Battery voltage sensing — none exists

- Zero hits for `analogRead|adc|voltage|vbat|battery|axp|pmu|charger` in `src/`, `lib/`,
  `include/`, `boards/`, `platformio.ini` (only false positives: `madctl`, `broadcast`,
  `gyroMaxPct`). No PMU driver in `lib_deps` (SensorLib is the IMU driver).
- ESC telemetry is not read (`lib/EscOutput/EscOutput.h:13-31` is output-only). CRSF battery
  frame 0x08 is neither parsed nor sent (`CrsfInput.cpp:356-387` handles 0x16, 0x14, ≥0x28).
- Daughter board `Pcb/OpenDrift Pdb`: BOM is 4 capacitors, one inductor, a TPS62162 buck
  (BEC 5 V → 3.3 V), four JST-PH servo/ESC connectors, two 1×11 headers. **No resistors,
  therefore no divider.** The header net `BAT` (J5-9) is a dead-end pass-through of the
  Waveshare board's own 1S cell terminal, not the car pack.
- Header GPIOs (from the user's schematic; Waveshare's site is blocked from here): 1, 2, 3,
  5, 6, 7, 8, 15, 16, 17, 18, 19, 20, 45, TXD, RXD, SDA(47), SCL(48), BAT, 3V3, 5V, GND.
  GPIO 4 is not brought out.
- ESP32-S3 ADC: 12-bit SAR; ADC1 = GPIO 1–10, ADC2 = GPIO 11–20. **ADC2 is unusable while
  WiFi is on** (the AP is started at `main.cpp:1706-1710`), so GPIO 15–20 are out. Usable range
  ≈ 0–3.1 V at 11 dB attenuation; factory eFuse calibration through `analogReadMilliVolts()`.
  Expect ±10–20 mV noise per raw sample and a few percent INL near full scale, so average
  8–16 samples and keep the divided voltage ≤ ~2.5 V.
- Free ADC1 header pins on every AMOLED build: **GPIO 5, 6, 7, 8**. GPIO 3 is a JTAG
  strapping pin; GPIO 1/2 carry servo/ESC or the CRSF UART on V2; GPIO 9/10 are display CS/SCLK.
  The frozen round board has **no free ADC1 pin**: GPIO 5–8 are touch IRQ, I2C and display DC
  (`include/board_128.h:4-7`, `Touch.h:31-34`, `LGFX_OpenDrift.hpp:228`), so its pin select
  offers only Off.
  On CRSF builds GPIO 1–8 (V1) / 3–8 (V2) are also the aux PWM outputs
  (`AuxChannelOutputs.cpp:125-141`, `isPinAvailable()`, web `<select>` at `WebConfigurator.cpp:362-410`),
  so the sense pin must be reserved there.
- Hardware change required: one divider from pack + to the chosen GPIO with the shared
  ground. Full design in battery-compensation-plan.md, section B10: 47 kΩ / 15 kΩ (ratio 4.133) plus 100 nF at the pin, GPIO 8 on
  the user's AMOLED V2, tapped from the 2S balance plug. The firmware treats anything above
  9.2 V as a fault.

### A3. Settings, storage, versioning, profiles

- Storage: `Preferences` opened once in `Settings::begin()` (`Settings.cpp:171-183`).
  Globals are loose members (`Settings.h:170-242`) loaded with `prefs.getX(key, default)` in
  `begin()` (`:185-442`) and written unconditionally in `save()` (`:498-688`). Keys ≤ 15 chars.
  Retired keys are deleted at boot (`:316-324`).
- Defaults exist in three hand-synced copies: member initialisers, `getX` fallbacks, and a
  few `else` literals. No `defaults()` function, no descriptor table (the only one is
  CRSF-side `FloatDefinition`, `CrsfParameterDevice.h:47-56`, unused by UI/web).
- Profiles: `Settings::DrivingProfile` (`Settings.h:13-31`, `version = 10`, 76 bytes:
  `uint32 version`, `char name[24]`, 4 floats, 8 int32). Up to 12, names user-supplied
  (Asphalt/Carpet/P-Tile are names the user types; no presets exist). Stored as **one NVS
  blob per profile** `prof0..prof11` (`persistProfile`, `:1806-1829`), so new fields cost no
  new keys. Count/active index in `profCnt`/`profAct`.
- `loadProfiles()` (`:1490-1767`) dispatches on `storedSize == sizeof(Layout)`: the current
  size accepts versions 8/9/10 and converts `gyroMaxCorrection` for 8/9 (`:1513-1544`); older
  layouts V1–V7 are snapshot structs (`:17-168`) with field-by-field branches. After loading,
  every profile is rewritten in the current format (`:1752-1755`). Undecodable blobs are
  silently dropped and the list compacts (`:1750`).
- Computed legacy sizes: V5 = 72, **V6 = 76 (same as current)**, **V1 = V7 = 80**, V2 = 84,
  **V3 = V4 = 88**. Because the `if/else if` chain tests one layout per size, V1, V3 and V6
  blobs can never load today (pre-existing). A new layout must not reuse any of these sizes.
- Per-profile = the 12 tune fields (`captureProfile` `:1769-1786`, `applyProfile`
  `:1788-1804`). Everything else is global. Live edits auto-persist into the active profile
  inside `save()` (`:661-673`).
- Change propagation: no callbacks. Setters clamp and set `dirty`; `update()` (`:487-496`)
  commits at most once per second (a rate limiter, not a settle debounce). The control task
  re-pushes every setting into `GyroController` each iteration (`main.cpp:832-896`).
- Adding a per-profile block touches: struct + version bump, snapshot of the old layout,
  new migration branch, 16 getter/setter decls + bodies, `begin()`/`save()` for globals,
  `captureProfile`/`applyProfile`, web render + parse, UI, docs. The last two additions
  (`c66e7f8` gyro LPF mode, `5f63095` CH3 gain min/max) touched 6–8 files each and were
  **not** added to the touch UI.

### A4. Touch menu

- Custom immediate-mode drawing on LovyanGFX into an off-screen `LGFX_Sprite`; not LVGL
  (`include/lv_conf.h` and `TFT_eSPI_Setup.h` are orphans). AMOLED canvas 456×280 landscape.
- Flat wrap-around carousel of 11 pages: constants `UI.cpp:6-16`, `totalPages` `UI.h:111`,
  dispatch `UI::drawPage` `UI.cpp:718-808`. No page registry, no back button, horizontal
  swipe only; vertical swipe is used only by the scrolling Profiles page.
- Widget pattern to copy: `drawExperimentalPage` (`UI.cpp:2474-2533`): AMOLED rows at
  y = 48 + 62·row with label (22, 58+62·row) size 2, value (146, 48+62·row) size 3, `-`/`+`
  buttons `drawAmoledButton(lcd, 276|364, y, 70, 48, …)`; round rows via `drawRoundAdjustRow`.
  Hitboxes in `repeatButtonAt` (`:5090-5262`; IDs 1–34 used), handlers in `applyRepeatButton`
  (`:5352-5601`), redraw dispatch `:5603-5627` (falls back to Response page if a page is not
  listed). Toggle pattern: `:1881-1897` + `actionButtonAt` `:5315-5316` + tap block; the
  AMOLED tap block returns early at `:6424-6428`, so AMOLED handlers go above it (WiFi/System
  examples `:6337-6422`). Enum cycling example `:6385-6387`.
- Three rows fit an AMOLED page; no page has more. Live readouts: 250 ms polling predicate
  at `:5744-5784` for DRIVE/RADIO/STEERING only, change-gated (`lastDrawnGainHundredths`).
- Page dots: AMOLED spacing 20 px centred (`:4928-4938`) → 12 pages span 220 of 456 px, fine;
  round 16 px → 208 of 240 px, tight. No literal page numbers exist besides 0 and
  `totalPages-1`, so inserting pages mid-list only renumbers the constants.
- Nothing shows battery voltage anywhere.

### A5. Web configurator

- Synchronous `WebServer` on port 80, softAP only (`OpenDrift`/`opendrift`, 192.168.4.1),
  **offline: no CDN can load**. No filesystem, no ArduinoJson, no WebSocket.
- Routes (`WebConfigurator.cpp:43-120`): `GET /` (`handleRoot` `:152-473`, one 8.7 KB
  `F()`-literal HTML string, `html.reserve(20000)` at `:167`), `POST /save` (`handleSave`
  `:524-827`, form-urlencoded, `getIntArg/getFloatArg` `:1131-1157`, checkboxes via
  `server.hasArg`), `GET /live-status` (`:476-521`, JSON built by hand, polled at 2 Hz by the
  one inline `<script>` at `:466`), profile create/activate/delete, `GET /blackbox.csv`,
  `POST /clear-log`.
- No schema sharing: each field is hand-rendered with `input()`/`checkbox()` (`:1078-1127`)
  and hand-parsed in `handleSave`. Missing form args fall back to the current value.
- No canvas/SVG/chart anywhere. A live curve preview must be an inline `<canvas>` with
  hand-written 2D drawing inside the `F()` strings (single quotes in JS, as at `:466`).
- Worst-case page today ≈ 19.5 KB (8 aux selects + 12 profiles), right at the reserve.
- CRSF parameter device (`CrsfParameterDevice.cpp`, 34 params) and EdgeTX `OpenDrift.lua`
  are a hand-maintained mirror; settings absent there simply do not appear on the radio.

### A6. Blackbox

- `BlackboxLogger::Record` (`BlackboxLogger.h:106-169`): 61 four-byte fields, no padding,
  **244 bytes**; five booleans bit-packed in `signalFlags`. Stored as a bare `Record[]` ring
  in PSRAM, 4 MiB preferred / 1 MiB minimum (`.cpp:383-417`). No header, magic or version.
- Rate: `loop()` gate `main.cpp:2035-2040` (`millis() - lastBlackboxLog >= 50`, ≈20 Hz, only
  while blackbox enabled and steering signal present). Single call site `main.cpp:2045-2111`
  with 63 positional arguments.
- Adding a field = 8 hand-kept positional edits: `log()` decl `.h:12-78`, def `.cpp:31-97`,
  struct, aggregate initialiser `.cpp:131-193`, `printf` format `.cpp:302`, arg list
  `.cpp:303-367`, header string `.cpp:8-9`, call site. Append at the end everywhere.
- Capacity: 4 MiB / 244 B = 17,189 records = 14.3 min (docs claim 18 min; stale). Five more
  fields → 264 B → 15,887 records = 13.2 min (−7.6 %); CSV grows ≈ +3 %.
- CSV row buffer `char line[672]` (`WebConfigurator.cpp:973`); typical row 349 B, worst
  case ≈ 845 B already; `formatCsvRecord` returns the would-be length on truncation
  (`.cpp:376-379`) and would then drop the newline. Widen the buffer alongside the record.
- No external parser; consumers are the docs tables (`Tuning.md:172-211`, README).

### A7. Float vs fixed point

Single-precision FPU; `double` is emulated. The hot path is clean `float` with `f`-suffixed
constants and `fabsf/expf/powf/sqrtf` (one `powf` per iteration at `GyroController.cpp:400-405`).
No shared math helpers: the house idioms are `x += (t−x)·(1−expf(−dt/tau))` (asymmetric
example `:574-591`), `constrain((x−onset)/span, 0, 1)`, countdown timers `max(0, t−dt)`,
and setters that clamp on entry. Arduino `constrain/min/max` are macros from `Arduino.h`,
so a host-buildable module needs its own tiny clamp.

### A8. Testing

Nothing host-side exists. `GyroController` depends on `Arduino.h` only for `micros()` and
the `constrain/min/max` macros, so it could be host-built behind a shim later. For this
feature the maths lives in a new module with zero Arduino includes, built and run with plain
`g++` from a Makefile under `OpenDrift/hosttest/` (kept outside `test/`, which is
PlatformIO's test root, so `pio test` never tries to build it for the ESP32; `pio run`
never builds either directory).

### A9. Risks and touched areas

1. Default PWM installs have no ESC output: the feature is inert unless GPIO 18/2 is in
   THROTTLE OUT mode or a CRSF build is used. Surface this in the web card and docs.
2. No sensing hardware exists; the feature ships behind a divider on GPIO 5–8.
3. Profile blob layout change: must add a size-distinct layout (116 B) and a migration branch;
   a downgrade to older firmware would drop 116-byte profiles (inherent in the scheme).
4. ESC write happens in `loop()` at UI-dependent cadence; compensation must be cheap and
   stateless per call so it adds no latency there.
5. Aux-output GPIO map on CRSF builds can collide with the sense pin.
6. Blackbox record widening is positional and fragile; retained history drops 7.6 %.
7. `html.reserve(20000)` is already exceeded on a CRSF AMOLED build with 12 profiles
   (≈ 26 KB); the new card needs a larger reserve and the graph JS must not live in that
   `String` (large `String` allocations go to PSRAM on this board, but reallocation churn is
   still avoidable).
8. Touch UI: the spec's nine controls do not fit one static page; the page must scroll like
   Profiles, which adds scroll-offset bookkeeping to the button mapping.
9. `loop()` can stall for seconds (CSV download yields only every 128 rows,
   `WebConfigurator.cpp:977-1008`); the filter update must clamp `dt` so one stalled tick
   cannot snap the resting estimate to a single sample.
10. Nothing in the codebase fixes "forward = pulse above 1500": the gyro uses the magnitude
    only (`GyroController.cpp:237`). A reversed throttle channel would get its brake side
    compensated unless the module knows the direction.
11. Downgrade: firmware older than this change drops 116-byte profile blobs and then saves
    `profCnt = 0`, orphaning them (`Settings.cpp:1750`, `:675-678`). Must be in the CHANGELOG.

Touched: `src/main.cpp`, `lib/Settings/*`, `lib/BlackboxLogger/*`, `lib/WebConfigurator/*`,
`lib/UI/*`, `lib/AuxChannelOutputs/*`, new `lib/BatteryCompensation/*`, new
`lib/BatterySense/*`, new `hosttest/*`, docs (`Tuning.md`, `Hardware.md`, `README.md`,
`CHANGELOG.md`). Not touched: `GyroController`, `EscOutput`, `CrsfInput`, CRSF parameter
device, EdgeTX Lua.

---
