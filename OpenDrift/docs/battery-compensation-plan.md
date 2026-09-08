# Battery Voltage Compensation — plan

Phase 2 output for the Battery Voltage Compensation feature, based on the findings in
[battery-compensation-research.md](battery-compensation-research.md). The spec leads:
every setting it lists is stored per profile with the spec's ranges and presets. The
departures the firmware forces are named in B0 with their reasons.

Work is done on the branch `feature/battery-compensation`, cut from `clean-upstream-v1.0.8`,
as one commit per task in B9.

## Plan

### B0. Decisions

Answers to the three design questions the spec delegated:

1. **Resting voltage drives compensation** (default on; the spec's "optional resting-voltage
   measurement" is a per-profile toggle). The asymmetric filtered voltage is the source when
   the toggle is off and is always the displayed/logged trend. A 1 s drop tau follows 45 % of
   a 0.6 s burst sag (8.20 → 7.50 V pulls it to 7.88 V), i.e. ≈3.5 % of physical
   compensation swinging mid-corner. A resting estimate that only updates while
   |throttle| < 5 % has held for ≥ 0.25 s never sees the sag. Drift runs lift every few
   seconds, so it refreshes.
2. **Strength (0–100 %, spec range) is a percentage of the physical amount, with End Voltage
   as the target.** Motor speed and torque both scale ≈ duty × V, so the physically correct
   low-throttle scale is `endV / V`. Over 7.4–8.4 V the deficit `1 − endV/V` is within 0.4 %
   of a straight line, so one formula covers both views: a linear ramp from 0 at endV to
   `1 − endV/startV` at startV, times strength/100. 100 % = "feels exactly like endV";
   lower values are the manual override the spec's Part 2 asked for. No separate
   target-voltage setting. Maximum compensation (100 %, endV 7.0, startV 8.4) is 17 % at zero
   throttle; verified numerically over the whole settings space: never non-monotonic, never
   above the input, 2000 → 2000 always, brake unchanged. Alternative if a literal raw
   percentage is preferred: `cMax = strength/100` directly, where sensible values are 5–15 %.
3. **Shapes: Linear (default), Expo, Custom**, exactly the spec's three. One function
   `w(t) = clamp((1−t)/(1−K), 0, 1)^p` with (K=0,p=1), (K=0,p=2), (K=knee, p=1). The spec does
   not define Custom, so Custom exposes one number, the knee (0–90 %, default 50): "full
   compensation up to K % throttle, then fade to zero", which is the shape of the example
   table. With physical strength the top-end slope of the linear taper is `1 + c` ≤ 1.17:
   not noticeable in practice. Expo makes the slope exactly 1 at 100 % but keeps only 25 % of
   `c` at mid throttle. Both documented and user-testable.

Spec items followed as written:

4. All eight menu settings (Enabled, Start Voltage, End Voltage, Strength, Curve Type,
   Voltage Filter, Drop Rate, Recovery Rate) plus the resting toggle and the knee are stored
   **per profile**, with the spec's ranges: voltages 7.0–8.4 V in 0.1 V steps, strength
   0–100 %, filter presets 0.5/1/2/5/10 s, drop 0.5–10 s, recovery 1–30 s.
5. The three time knobs keep distinct jobs: Voltage Filter preset = resting-estimator tau
   (accumulated lift time, default 2 s); Drop Rate (default 1 s) and Recovery Rate (default
   10 s) = the live asymmetric filter.
6. Brake/reverse untouched (pulse on the brake side of 1500 returned as is). Neutral stays
   1500, so the CRSF arming band is unaffected.
7. Cell count: fixed 2S plausibility window 5.5–9.2 V. A 3S pack (≥ 11 V) is a fault
   ("voltage out of range"), compensation fades to zero, reason shown on web and UI. Never
   clamped.
8. Faults fade, never step: a health blend `h` slews ±1.0/s toward 1 when readings are valid
   and the feature is enabled, else toward 0. Effective compensation is `c × h`, so fault,
   recovery, sensor-off, and toggling Enabled are all 1 s ramps.
9. Filter init: first valid sample sets `vFilt = vRest = vRaw` exactly.
10. Blackbox gets the spec's five quantities. Throttle Input is the existing
    `throttle_raw_us` column (already the driver's pulse; renaming it would break the docs);
    the five new columns are `battery_raw_v, battery_filtered_v, battery_resting_v,
    battery_comp_pct, throttle_out_us`, resting voltage included because it is what drives
    the compensation. `throttle_out_us` is 0 whenever no ESC output is active.

Departures the firmware forces, and additions the spec does not mention:

11. **Touch menu shape.** The UI is a flat swipe carousel with no submenus and room for
    three rows per page (research A4). The spec's single "Battery Compensation" submenu becomes one
    carousel page of that name that **scrolls vertically** through all its rows, the same
    mechanism the Profiles page uses. Three rows are visible at a time with a scrollbar.
12. **Hardware settings, global** (the spec does not list them, and they describe the car,
    not the surface): sense pin (Off / GPIO 5 / 6 / 7 / 8, default Off, runtime-selectable
    like the GPIO 18 mode; GPIO 8 is the documented choice for the user's V2 board), voltage
    scale (default 4.133 for the 47k/15k divider in B10, with a web "measured pack voltage"
    calibrate field), and throttle reversed (default off, because nothing in the codebase
    fixes which side of 1500 is forward; the gyro only uses the magnitude).
13. **Out of scope:** CRSF parameter device and EdgeTX Lua exposure (not in the spec; the Lua
    asset ships separately), 3S support, ESC/CRSF telemetry as a voltage source. Listed as
    follow-ups in the plan doc.
14. **Frozen round builds:** no free ADC pin, so the sense-pin select offers only Off; the
    page compiles in its round `#else` branch and compensation stays inert there.

### B1. Maths (module `BatteryCompensation`, pure float, no Arduino includes)

Inputs per update: `vRaw` (V), `rawValid`, `throttleInUs`, `dt` (s, **clamped 0..0.1** so a
stalled `loop()` cannot snap a filter to one sample or satisfy the settle time in one step).
`alpha(tau) = 1 − expf(−dt/tau)`. All constants `f`-suffixed. No member or function is named
`abs/min/max/constrain/round` (Arduino macros); the module uses a private `clampf` and `lroundf`.

```
valid = rawValid && senseEnabled && 5.5 ≤ vRaw ≤ 9.2
if (valid && !initialised) { vFilt = vRest = vRaw; initialised = true; }

if (valid) { tau = vRaw < vFilt ? dropTau : recoveryTau; vFilt += (vRaw − vFilt) × alpha(tau); }

lifted    = |throttleInUs − 1500| ≤ 50            // same band as CRSF arming
liftTimer = lifted ? liftTimer + dt : 0
if (valid && liftTimer ≥ 0.25) vRest += (vRaw − vRest) × alpha(restTau)      // else hold

hTarget = (valid && enabled && startV − endV ≥ 0.2) ? 1 : 0
h      += clamp(hTarget − h, −dt, +dt)

V     = useResting ? vRest : vFilt
x     = clamp((V − endV) / (startV − endV), 0, 1)
cMax  = (strength / 100) × (1 − endV / startV)
c     = cMax × x × h                                                          // ≤ 0.17

apply(in): in = clamp(in, 1000, 2000)                 // CRSF delivers 988–2012
           fwd = reversed ? (1500 − in) : (in − 1500)
           if fwd ≤ 0 → in                            // neutral, brake, reverse untouched
           t = fwd / 500;  u = clamp((1 − t)/(1 − knee), 0, 1);  w = expo ? u² : u
           fwdOut = lroundf(500 × t × (1 − c × w))
           out = reversed ? 1500 − fwdOut : 1500 + fwdOut
```
Guarantees (asserted by the host tests): `out(2000) = 2000` for all V/strength/curve;
`d out/dt = 1 − c·w − c·t·w′ ≥ 1 − c > 0` so monotonic; `c·w ≥ 0` so `out ≤ in`; `x = 0`
at/below endV; a sag under throttle cannot move `vRest`; `h` moves at most `dt` per update.

Telemetry getters: raw/filtered/resting volts, `compensationPercent` (100·c·w at the last
input), last input/output µs, health, `faultReason` enum {none, sensorOff, noSample,
outOfRange, badSettings}.

### B2. Settings

Per-profile, appended to `DrivingProfile` (all 4-byte so the blob stays padding-free; new
`sizeof` = 76 + 10 × 4 = **116**, distinct from 72/76/80/84/88; `version` 10 → 11). Also
mirrored as global NVS keys (≤ 15 chars) like every other profile field, so the "Current
Tune" state with no active profile keeps them too:

| Field | NVS key | Type | Range | Default | Unit |
|---|---|---|---|---|---|
| `batteryCompEnabled` | `batEnabled` | int32 (0/1) | 0–1 | 0 | – |
| `batteryCompStartVoltage` | `batStartV` | float | 7.0–8.4 step 0.1 | 8.4 | V |
| `batteryCompEndVoltage` | `batEndV` | float | 7.0–8.4 step 0.1 | 7.4 | V |
| `batteryCompStrength` | `batStrength` | int32 | 0–100 | 100 | % of physical |
| `batteryCompCurve` | `batCurve` | int32 | 0 Linear / 1 Expo / 2 Custom | 0 | – |
| `batteryCompKnee` | `batKnee` | int32 | 0–90 | 50 | % throttle |
| `batteryCompFilterMs` | `batFilterMs` | int32 | 500/1000/2000/5000/10000 | 2000 | ms |
| `batteryCompDropMs` | `batDropMs` | int32 | 500–10000 | 1000 | ms |
| `batteryCompRecoveryMs` | `batRiseMs` | int32 | 1000–30000 | 10000 | ms |
| `batteryCompUseResting` | `batResting` | int32 (0/1) | 0–1 | 1 | – |

Global hardware settings (not per profile):

| Member | Key | Type | Range | Default |
|---|---|---|---|---|
| `batterySensePin` | `batPin` | u8 | 0, 5, 6, 7, 8 | 0 (off) |
| `batteryVoltageScale` | `batScale` | float | 1.0–10.0 | 4.133 |
| `batteryThrottleReversed` | `batThrRev` | bool | – | false |

Setters clamp and set `dirty` (existing convention). `applyProfile` clamps all ten new
fields (it currently trusts the blob for most fields; the module's `startV − endV ≥ 0.2`
health gate is the second net). On non-AMOLED builds `setBatterySensePin` accepts only 0.

### B3. Migration of saved profiles

- Snapshot the current 76-byte layout as `DrivingProfileV10` in the anonymous namespace
  (`Settings.cpp:17-168`), next to V1–V8.
- The primary branch keeps `storedSize == sizeof(DrivingProfile)` (116 B) and accepts
  version 11 only; stamps `version = 11`. The literal `10` changes to `11` in all four places:
  `Settings.h:15`, `Settings.cpp:1520`, `:1525`, `:1773`.
- New `else if(storedSize == sizeof(DrivingProfileV10))` inserted **directly after the
  primary branch** (between `:1545` and `:1546`; if it came after the V6 branch, 76-byte
  blobs would hit V6's size test first and be dropped): copy the 12 fields, seed the ten
  new ones with defaults (Enabled = 0, so behaviour is unchanged after upgrade), and carry
  the existing v8/v9 `gyroMaxCorrection` conversions (`:1528-1541`). In the same 76-byte
  branch also accept `version == 6` with the V6 field meaning (`gyroHuntStrength = 50`), since
  that layout has the same size and is unreachable today; noted in the summary as a small
  in-branch fix rather than a separate refactor.
- `captureProfile` stamps `version = 11` and copies the new fields; `applyProfile` applies
  them with clamps.
- `static_assert(sizeof(DrivingProfile) == 116)`, `static_assert(sizeof(DrivingProfileV10) == 76)`,
  and one assert that 116 differs from every legacy size. (A blanket "no two legacy sizes
  collide" assert cannot compile: V1/V7 and V3/V4 already collide.) Older branches untouched.
- Boot rewrite at `:1752-1755` then persists every loaded profile as v11 (NVS dedupes
  identical blobs, so later boots are no-ops; the 20 KB partition has room for 12 × 116 B).
  Global keys are new and default when absent. Nothing existing is renamed or removed.
- Downgrade hazard goes in the CHANGELOG: older firmware drops 116-byte blobs and then
  writes `profCnt = 0`.

### B4. Failure behaviour

| Condition | Detection | Effect |
|---|---|---|
| Sense pin Off | setting | `faultReason = sensorOff`, `h → 0` over 1 s |
| No sample yet (boot) | `!initialised` | `noSample`, `h = 0` until first valid sample, then `h` ramps up over 1 s |
| Reading outside 5.5–9.2 V (open divider ≈ 0 V, 3S pack ≈ 11–12.6 V) | per sample | `outOfRange`, filters hold last value, `h → 0` |
| `startV − endV < 0.2` | settings | `badSettings`, `h → 0` |
| Compensation disabled in profile | setting | `h → 0` (no step when toggled live) |
| `loop()` stall (`dt` > 0.25 s) | clamp | filters advance by at most 0.25 s |
| ESC output inactive (PWM gain-input mode, CRSF unarmed/failsafe) | existing branches | `apply()` is not reached; failsafe writes 1500 directly |

### B5. Hook points and ordering

- `loop()` gains, just before the throttle-output section (`main.cpp:1938-1940`):
  `batterySense.update()` (one calibrated `analogReadMilliVolts` per 20 ms tick; in core
  2.0.x each call re-runs the ADC characterisation, so bursts of 8 would cost ~1 ms, and
  the ≥ 0.5 s filters already average dozens of samples) and
  `batteryComp.update(volts, valid, throttleInputUs, dt)` with `dt` from `micros()`, plus a
  settings push (`batteryComp.configure(...)` from the `Settings` getters, cheap).
  `BatterySense` discards the first two samples after a pin (re)configuration, because a
  pin released by the aux outputs is left in `INPUT_PULLDOWN` until the next analog read
  resets its mode (`AuxChannelOutputs.cpp:234-237`).
- CRSF: `updateCrsfThrottleOutput()` `main.cpp:676-682` becomes
  `throttleOutput.writeMicroseconds(batteryComp.apply(constrain(throttlePulse,1000,2000)))`.
  Failsafe (`:600-613`) and pre-arm neutral writes (`:625-628`) stay literal 1500; the arming
  neutral check (`:615-617`) still sees the driver's pulse.
- PWM: `main.cpp:1982-1985` becomes `writeMicroseconds(batteryComp.apply(throttleRadio.getPulseWidth()))`;
  the signal-loss branch (`:1987-2001`) is untouched.
- `GyroController::update()` keeps receiving the raw driver throttle. Nothing in the control
  task changes. `ControlTelemetry` is not extended: throttle in/out are loop-side values.
- Aux outputs: `AuxChannelOutputs::setReservedPin(gpio)` (static) consulted by
  `isPinAvailable()`; `main.cpp` calls it each loop from the setting. `update()`
  (`AuxChannelOutputs.cpp:81-98`) already detaches any attached slot whose pin is no longer
  available, so a pin that becomes reserved releases itself on the next loop.
- `BatterySense` configures the pin with `analogSetPinAttenuation(pin, ADC_11db)`; a pin
  change takes effect on the next sample. Both APIs exist in Arduino core 2.0.x, which this
  project is on (`EscOutput.cpp` uses the 2.0.x `ledcSetup`/`ledcAttachPin`).
- `EscOutput` gains a 3-line `getPulse()` returning `currentPulse` so the blackbox logs what
  was actually written, including failsafe 1500 writes; logged as 0 when the output is
  inactive.
- `UI` gains `setBatteryCompensation(const BatteryCompensation&)` (pattern:
  `setThrottleRadio`, `UI.h:30-32`, `UI.cpp:812-817`), called from `setup()` after
  `main.cpp:1771-1785`. The module has no Arduino dependency, so `UI` can include it.

### B6. Web configurator

- New card "Battery Compensation" between Gain Channel Calibration and Aux outputs,
  **rendered and parsed on every build** (checkboxes save `false` when absent, so the card
  and its `hasArg` calls must always be present; round builds just offer pin Off): live
  readouts (raw / filtered / resting V, applied %, status, which side is forward),
  the per-profile inputs in the spec's order (Enabled, Start V, End V, Strength, Curve
  `<select>`, Knee, Voltage Filter `<select>`, Drop, Recovery, plus the resting-voltage
  toggle), and the global hardware inputs (Sense pin `<select>`, Voltage scale, "Measured
  pack voltage" calibrate field that rescales on save, Throttle reversed). Warning text on
  PWM builds when `getThrottleOutputEnabled()` is false, naming
  GPIO 18 or GPIO 2 per build (pattern `:207-213`); note that the ESC must be wired through
  OpenDrift.
- Two `<canvas>` graphs drawn by JS that mirrors `apply()`: voltage (7.0–8.4 V) vs
  compensation % at zero throttle with a marker at the live resting voltage, and
  throttle-in vs throttle-out for the current voltage with the identity line. Redrawn on
  every `input` event and on each 2 Hz poll. The JS is served from a new `GET /battery.js`
  route via `server.send_P` from a raw string literal, not appended to the page `String`
  (a CRSF AMOLED page with 12 profiles is already ≈ 26 KB against `reserve(20000)`).
- `handleLiveStatus` gains `vraw, vfilt, vrest, comp, bstat, fwd`; `reserve(72)` → 192.
- `html.reserve(20000)` → 30000. Aux `<select>` shows "Reserved for battery sense" for the
  sense pin; `handleSave` already skips unavailable pins.
- `WebConfigurator::begin()` takes a `BatteryCompensation&` and `BatterySense&`; both call
  sites change (`main.cpp:1737-1744` in `setup()` and `:1923-1930` in `loop()`).

### B7. Touch UI

One page `PAGE_BATTERY` "Battery Compensation" inserted after Transition (constants
renumbered; `totalPages` 11 → 12; page order comment `UI.h:105-106` updated). It holds the
spec's menu as a vertically scrolling list, the mechanism the Profiles page already uses
(`profileScroll` `UI.h:139`, clamp `:2554-2557`, vertical-swipe handler `:5979-6030`,
scrollbar `:2649-2667`):

| Row | Control | Step / values |
|---|---|---|
| 0 | ENABLED | wide ON/OFF button (green/magenta) |
| 1 | START VOLTAGE | −/+ 0.1 V, 7.0–8.4 |
| 2 | END VOLTAGE | −/+ 0.1 V, 7.0–8.4 |
| 3 | STRENGTH | −/+ 1 %, 0–100 |
| 4 | CURVE | −/+ cycles Linear / Expo / Custom |
| 5 | KNEE | −/+ 5 %, 0–90 (shown only while Curve = Custom) |
| 6 | VOLTAGE FILTER | −/+ steps 0.5 / 1 / 2 / 5 / 10 s |
| 7 | DROP RATE | −/+ 0.5 s, 0.5–10 s |
| 8 | RECOVERY RATE | −/+ 1 s, 1–30 s |

Header line shows the live voltage and applied percentage, or the fault text ("NO SENSOR",
"OUT OF RANGE"), refreshed every 250 ms with a change gate (pattern `lastDrawnGainHundredths`,
`UI.h:127`, predicate `:5744-5757`) using a partial redraw. Three rows are visible at a time
(AMOLED y = 48 + 62·row; round `drawRoundAdjustRow` at 61 + 55·row), a scrollbar on the right
shows position, and a vertical swipe on a non-button area scrolls one row per 40–62 px, as
on Profiles. The resting-voltage toggle stays web-only (like the Drift Memory limit).

Mechanics: `uint8_t batteryScroll`; `repeatButtonAt` returns IDs 35–40 by *visible* row
(AMOLED branch `:5095-5172` at (276|364, 48+62·row, 70, 48); round branch after it at
(26|170, 61+55·row, 44, 30)); `applyRepeatButton` maps visible row + `batteryScroll` to the
logical row (IDs in use today: 1–8, 15–20, 23–34); its redraw dispatch `:5603-5627` gets a
branch for the page (the `else` paints the Response page otherwise); `drawPage` gets a case
(no `default`: a missing case leaves a black canvas); the ENABLED toggle uses the existing
wide-button pattern (`actionButtonAt` `(276, 48, 158, 48)` like Core's `(276, 172, 158, 48)`;
registration there is what stops a tap from becoming a swipe, `:5806-5810`) handled in the
AMOLED tap block above `:6424` and in the round tap block; the vertical-swipe predicate is
extended from `isProfilesPage()` to the battery page. Round dot spacing 16 → 14 px keeps
12 dots inside the visible disc. Live voltage:
add `PAGE_BATTERY` to the refresh predicate with a change gate on hundredths of a volt and a
partial `fillRect` + `drawFloat` redraw. Voltage source stays web-only.

### B8. Blackbox

Append five members to `Record` (`float ×4, int32 ×1`), five parameters at the end of `log()`
(declaration and definition), five initialiser entries, `,%.3f,%.3f,%.3f,%.1f,%ld` at the end
of the format string, five args, five header names (65 → 70 columns). Call site passes
`batteryComp.getRawVolts()` etc. and `throttleOutput.isActive() ? throttleOutput.getPulse() : 0`.
Widen `char line[672]` to 800 (rows are ≈ 520 B today, ≈ 550 B after; a truncated row loses
its newline and merges with the next). Update the retained-history figures in `Tuning.md`
(18 → 13.2 min) and the CSV field tables.

### B10. Hardware design — battery sense divider (user runs AMOLED V2)

Why it is needed: the Waveshare board is fed regulated 5 V from the BEC and no pin on it or
on the daughter board sees the pack. ESP32-S3 pins take 3.3 V maximum, so 8.4 V must be
divided down before it reaches an ADC pin.

**Pin: GPIO 8** (ADC1 channel 7). Free on V2 in both PWM and CRSF builds, WiFi-safe (ADC1),
not a strapping pin, and on the daughter-board header at J6 pin 5 (the header row runs
GPIO 18, 17, 16, 15, 8, 7, 6, 5, 3, 2, 1 from pin 1). GPIO 5–7 stay for aux outputs. The
firmware default remains Off; select GPIO 8 in the web configurator after fitting the divider.

**Circuit** (three parts, all JLCPCB basic parts):

```
pack +  ──[ R1 47 kΩ 1 % ]──┬──[ R2 15 kΩ 1 % ]── GND (OpenDrift ground)
                            │
                            ├──[ C1 100 nF ]────── GND
                            │
                            └───────────────────── GPIO 8 (J6-5)
```

| Item | Value | Reason |
|---|---|---|
| Ratio | (47 + 15) / 15 = 4.133 | 8.4 V → 2.03 V at the pin, inside the ADC's linear range with margin; the firmware default scale becomes 4.133 |
| Drain | 8.4 V / 62 kΩ = 0.14 mA | Negligible while driving; unplug the sense lead with the pack if the car is stored with the battery connected |
| Source impedance | 47k ∥ 15k = 11.4 kΩ | With C1 the sampling capacitor is fed from C1, so readings stay clean without a buffer |
| Fault ceiling 9.2 V | 2.23 V at the pin | Anything above is rejected in firmware |
| 3S by mistake (12.6 V) | 3.05 V at the pin | Still below 3.3 V; read, reported, rejected |
| 4S by mistake (16.8 V) | 4.06 V, limited by R1 to ≈ 10 µA into the pin's protection diode | Harmless; optional BAT54S clamp to 3V3/GND on the next board revision makes it bulletproof |
| Reverse polarity | R1 limits current to ≈ 0.2 mA | Survivable; the optional clamp covers it too |
| Resolution | 1 ADC step ≈ 3 mV of pack voltage | Far below the 0.1 V setting steps |
| Tolerance | 1 % resistors → up to ±2 % ratio error (±0.17 V) | Removed by the one-time calibration below; after it, error is the ADC's ±1 % plus noise, ≈ ±0.05 V |

**Where the pack + comes from.** The cleanest tap is the 2S balance plug (JST-XH, three
pins: black = GND, middle = cell 1, outer = pack +). Use a JST-XH 2S extension or a
balance-lead breakout, take only the outer pack + wire to R1, and leave the balance GND
unconnected: OpenDrift's ground is already the battery negative through the ESC, and a
second ground path only adds a loop. Under heavy load the ESC's negative lead drops
0.1–0.3 V, which shows up as a small under-load measurement offset; the resting-voltage
mode measures during lifts, when that drop is zero, which is one more reason it is the
default. Alternative tap: the ESC's battery + input or the ESC side of the power switch.

**Now (hand-wired):** R1 and R2 soldered inline in the sense wire or on a scrap of perfboard
with C1, heat-shrunk, output to header pin J6-5 and ground to any PDB GND pin. Route the
sense wire away from the motor wires.

**Next daughter-board revision:** add a 2-pin JST-PH "VBAT" input (pack +, GND), R1/R2/C1
on the board, optional BAT54S, trace to the GPIO 8 header pin. I do not edit the KiCad
files in this work (PCB layout needs a human in KiCad); the schematic change is the three
parts above and is documented in `Hardware.md` so it can be drawn in minutes.

**Calibration procedure (web configurator, once):** power the car, read the pack with a
multimeter at the balance plug, type it into "Measured pack voltage", save. The firmware
sets `scale = measured / (pin millivolts / 1000)`. The live readout should then match the
meter within ±0.05 V at rest.

**Wiring of the throttle path on V2** (unchanged by this feature, but required for it to
act): CRSF build → ESC signal on GPIO 16, already through OpenDrift. PWM build → GPIO 2
must be in THROTTLE OUT mode and the ESC plugged into OpenDrift's throttle output, which
gives up the gain-channel input on that pin.

### B9. Commit-sized tasks

0. **Branch:** `git checkout -b feature/battery-compensation clean-upstream-v1.0.8`; every
   commit below lands there; push with `git push -u origin feature/battery-compensation`
   after the first commit and open a draft PR against `clean-upstream-v1.0.8`.
1. **Docs:** add `docs/battery-compensation-research.md` (Part A) and
   `docs/battery-compensation-plan.md` (Part B including the B10 hardware design), and add
   the "Battery voltage sense" wiring section to `docs/Hardware.md` (pin, parts, balance-plug
   tap, calibration, fault ceiling). Verify: files render, links resolve.
2. **Pure module + host tests:** `lib/BatteryCompensation/BatteryCompensation.{h,cpp}`,
   `hosttest/Makefile`, `hosttest/test_battery_compensation.cpp`, `hosttest/` build output
   in `OpenDrift/.gitignore`. Verify: `make -C OpenDrift/hosttest test` passes with
   `-std=c++17 -Wall -Wextra -Werror`; the module also compiles with
   `-fsingle-precision-constant -Wdouble-promotion -Werror` (no double promotion); a second
   translation unit `#define`s the Arduino macros (`abs/min/max/constrain/round`) before
   including the header to prove no name clash. Assertions: 2000 → 2000 at every V, strength (0–100),
   curve and both throttle directions; non-decreasing output; out ≤ in; zero compensation at
   or below endV; brake/neutral unchanged; 0.6 s sag under throttle moves compensation
   < 0.5 % in resting mode (and ≈ 45 % of the dip in filtered mode, documenting the trade);
   fault/enable/disable ramps `h` by at most `dt` per update; `dt` clamp; first-sample init;
   3S rejected; `startV − endV < 0.2` rejected.
3. **Settings:** struct v11 (ten fields), snapshot V10, migration branch, static_asserts,
   NVS keys for the ten profile fields and three hardware globals, getters/setters,
   capture/apply with clamps. Verify: a host-compiled copy of the migration
   dispatch (struct layouts + conversion helpers + the branch bodies, behind a tiny
   `Preferences` stub) fed synthetic v6/v8/v9/v10 blobs yields the expected v11 profiles.
4. **Sense + hook:** `lib/BatterySense/*` (per-board pin gate, one read per 20 ms, discard
   two samples after reconfiguration), `EscOutput::getPulse()`, `main.cpp` sampling +
   settings push + `apply()` at both ESC writes + aux-pin reservation + boot log line,
   `AuxChannelOutputs` reserved pin, `UI::setBatteryCompensation`. Verify: review of both
   write sites and the failsafe branches; host tests cover the module; target compile if
   PlatformIO can be installed here (attempt `pip install platformio` and
   `pio run -e waveshare_amoled_164 -e waveshare_amoled_164_crsf -e waveshare_128`; the
   toolchain download may be blocked, in which case the user compiles and I say so).
5. **Blackbox:** five fields end to end, `line` buffer 800, docs tables. Verify: host
   `static_assert(sizeof(Record) == 264)` compiled from the header behind an `Arduino.h`
   shim; a Makefile check that header column count = format conversions = 70.
6. **Web:** card, inputs, parse, live JSON, `/battery.js`, canvases, aux reservation label,
   reserve bumps, both `begin()` call sites. Verify: a small script extracts the raw JS
   literal from the `.cpp` into a scratch `preview.html` with representative form values,
   Chromium via Playwright screenshots both graphs at 8.4 / 8.0 / 7.4 V and I inspect them;
   the JS curve is cross-checked against the C++ module by printing ten sample points from
   the host test and comparing.
7. **Touch UI:** the scrolling Battery Compensation page, buttons, toggle, live readout,
   scrollbar, dots, page renumbering. Verify: target compile (here if PlatformIO works,
   otherwise the user); on-device checklist in the plan doc (swipe every page both
   directions, scroll the list to both ends, hold ± for auto-repeat on the first and last
   row, tap the toggle without starting a swipe, live voltage updates, Knee row appears
   only with Custom).
8. **Docs/CHANGELOG:** README features / web list / blackbox rows, `Hardware.md` divider
   wiring and pin table, `Tuning.md` section + corrected history figure, `CHANGELOG.md`
   "Unreleased" entry including the downgrade hazard, boot banner line.

Each task is one commit with a "what changed and why" note, as requested.

---

## Verification

- Host: `make -C OpenDrift/hosttest test` runs the assertions listed in B9 task 2 plus the
  migration test on synthetic blobs (task 3) and the blackbox size/column check (task 5).
- Target: attempt PlatformIO install and `pio run` for the PWM and CRSF AMOLED envs plus the
  round env (must still compile). If not possible here, list the exact commands for the user.
- Device checklist (in the plan doc): sense pin Off → status "sensor off", ESC output equals
  input; pin on with divider → live voltage within ±0.05 V of a multimeter after calibrate;
  enable at 8.3 V with defaults → 50 % stick (1750 µs) gives ≈ 47 % output (1737 µs) on
  the web graph and in the blackbox `throttle_out_us`; full stick gives 2000 µs; a 0.6 s
  full-throttle burst moves the resting voltage < 0.02 V (≈ 0.2 % of duty); power cycle keeps settings and profiles;
  switching profiles switches compensation settings; blackbox CSV shows the five columns.
