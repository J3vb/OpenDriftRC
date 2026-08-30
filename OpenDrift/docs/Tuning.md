# OpenDrift v1.0 Tuning

> **Technical reference:** This document records the complete tuning behavior
> and current test workflow. The shorter public guide is available at
> [opendriftrc.com/tuning](https://opendriftrc.com/tuning/).

OpenDrift v1.0 is a clean-sheet controller built from extensive track testing.
It uses a deliberately short control path so each adjustment has a clear job.

OpenDrift v1.0 runs the IMU, control calculation, and steering output in a dedicated
250 Hz or 333 Hz task. Display, touch, Wi-Fi, web configuration, and logging run outside
the control task. The steering servo output uses the same selected rate; throttle passthrough
remains 50 Hz.

## Mechanical baseline first

The controller cannot repair an unstable chassis or servo.

Before tuning:

- Confirm the servo is stable with its internal anti-wobble setting just below
  the point where it buzzes.
- Confirm the receiver has no hidden stability assistance.
- Check steering endpoints for binding.
- Establish sensible front toe, rear toe, camber, ride height, and damping.
- Verify the same chassis can drive cleanly with a known-good gyro.
- Calibrate steering and gyro direction with the wheels safely off the ground.

Open **Physical Endpoints** and position the wheels at their
safe physical full-left stop, neutral, and safe full-right stop before capturing
each point. Each target changes from red to green; do not drive until all
three are green and the page reports `SAVED - TAP TO RESET`. CRSF users can make
the same captures from `OpenDrift.lua`. Its `END: NO/PART/YES` status and the
AMOLED status use the same saved calibration and update each other.

Rear toe-in accidentally present during development made the car and controller
fight one another. Correcting the alignment materially improved both OpenDrift
and commercial gyros.

## v1.0 signal path

The active path is intentionally short:

1. Read receiver steering, throttle, and IMU yaw.
2. Apply deadband and one time-based yaw low-pass.
3. Estimate short-horizon yaw from filtered yaw acceleration.
4. Extend that prediction briefly when throttle announces a chassis-load
   change.
5. Hold a longer prediction envelope after throttle lift so off-throttle load
   changes do not arrive as a surprise.
6. Taper optional prediction during deliberate transitions so prediction does
   not run ahead of the physical yaw reversal.
7. Track and isolate the measured 2.5-3.6 Hz wheel-wobble band during a quiet
   settled drift, then use Anti Wobble to control notch depth while
   preserving slow chassis yaw and useful correction outside that band. A
   shallow guard remains during entry and transitions so the resonance cannot
   restart while frequency tracking is intentionally frozen.
8. Apply direct yaw correction using Gain.
9. While the driver and throttle are quiet, learn a slow drift reference.
10. Add optional Countersteer Assist from that slow reference.
11. Apply Drift Memory only to deviation from that reference.
12. Clamp correction with saturation-aware memory behavior.
13. Add correction to receiver steering and send it to the servo.

Driver steering activity and throttle changes make the slow reference follow
the car quickly. They do not disable the fast direct damping path.

## Active settings

| Setting | v1.0 behavior |
|---|---|
| Gain | Direct correction per degree/second of predicted yaw |
| Deadband | Removes very small corrected yaw near zero |
| Max Correction | Maximum gyro movement as a percentage of calibrated physical steering travel |
| Smoothing | The only yaw low-pass; larger values add more filtering |
| Countersteer Assist | Adds slow settled-drift countersteer without increasing fast damping; `0` preserves the base response |
| Prediction | Continuous yaw-acceleration look-ahead from 0–100 |
| Hold Assist | Controls how slowly a quiet-drift reference follows yaw |
| Drift Memory | Feedback strength for error from the quiet-drift reference |
| Memory Limit | Maximum Drift Memory contribution in microseconds |
| Steering Travel | Scales driver steering only; it does not reduce gyro authority |
| Transition Speed | Centered transition damping adjustment; `50` is neutral, lower is slower, higher is faster |
| Anti Wobble | Depth of the phase-aware dynamic 2.5-3.6 Hz wheel-wobble notch; `0` bypasses it, `50` is the recommended starting point, and `100` applies maximum depth |

Throttle prediction remains active when a valid throttle signal is present,
even with Prediction set to zero. The Prediction setting adds general
yaw-acceleration look-ahead; throttle temporarily extends that horizon before
the chassis response develops.

### Transition Speed

Transition Speed is tuned after the core settings. Keep it at `50` for neutral
response. Lower values add yaw damping through the complete direction change;
higher values reduce damping and release some transition authority for faster
rotation. It follows both the driver's transition intent and the measured yaw
reversal, then fades out before the next settled drift. Test `25`, `50`, and
`75` at the same tune first, then refine the preferred direction.

## Safe first test

Use a stand or hold the chassis with the wheels clear before driving.

| Setting | Initial value |
|---|---:|
| Gain | `1.50` |
| Deadband | `4` |
| Max Correction | `50%` |
| Smoothing | `0.01` |
| Countersteer Assist | `0` |
| Prediction | `0` |
| Hold Assist | `0` |
| Drift Memory | `0.00` |
| Memory Limit | `80` |
| Servo Quiet | `0` |
| Control / servo rate | `250 Hz` |
| Transition Speed | `50` |
| Anti Wobble | `50` |

Check that rotating the chassis produces steering correction in the direction
that opposes the rotation. Reverse gyro correction if it assists the rotation.

Begin with low-speed entries:

1. Raise Gain until the car catches rotation decisively.
2. If the response is strong but runs out of steering, raise Max Correction in
   small steps.
3. If rapid chassis motion looks nervous, add Prediction in steps of `5`.
4. If the signal is visibly noisy, add Smoothing in steps of `0.01`. Do not use
   smoothing to repair a control oscillation.
5. If the car is stable but the driver carries too much of the settled countersteer, add Countersteer Assist in steps of `10`.
6. Once entries and transitions are clean, add Hold Assist in steps of `5`.
7. Add Drift Memory last in steps of `0.05`.

Change one setting at a time.

## Interpreting symptoms

| Symptom | First adjustment |
|---|---|
| Car does most of the work but gyro feels weak | Raise Gain |
| Initial response is strong but authority stops building | Raise Max Correction carefully |
| Fast response overshoots before settling | Add a small amount of Prediction |
| Prediction makes direction changes sharp or nervous | Lower Prediction |
| Transition happens too quickly or overshoots | Lower Transition Speed |
| Transition feels held back or rotates too slowly | Raise Transition Speed |
| Stable drift requires too much sustained driver countersteer | Raise Countersteer Assist |
| Gyro feels too hands-on after the drift settles | Lower Countersteer Assist |
| Long drift slowly wanders after entries are already good | Add Hold Assist |
| Quiet drift reference is present but does not correct enough | Add Drift Memory |
| Transition carries the old drift | Lower Hold Assist or Drift Memory |
| Mid-drift wheel oscillation | Lower Gain first and verify the servo and chassis; then raise Anti Wobble from its default `50` in steps of `10` |
| Correction sits at Max Correction | More gain will not add authority; inspect travel and geometry |

## Control and servo rate

Use **250 Hz** unless the servo documentation explicitly lists 333 Hz support. It is the compatibility setting and works with a broader range of digital servos. **333 Hz** shortens the command interval from 4 ms to about 3 ms and may improve response on a supported fast servo. Sending 333 Hz to an unsupported servo can cause buzzing, heat, erratic motion, or damage. Restart OpenDrift after changing the rate.

## Current v1.0 CSV fields

The `blackbox-v11.csv` logger records the current controller channels without
the retired alpha-era tuning fields:

| Field | Meaning |
|---|---|
| `gyro_raw_us`, `gyro_correction_us` | Controller correction before final steering mix |
| `predicted_yaw` | Filtered yaw plus short-horizon prediction |
| `drift_reference_yaw` | Learned quiet-drift yaw reference |
| `reference_error` | Filtered yaw minus drift reference |
| `reference_lock` | Continuous quiet-drift confidence |
| `throttle_prediction` | Active throttle load-change prediction blend |
| `direct_correction_us` | Gain-based direct correction |
| `countersteer_assist` | Saved Countersteer Assist setting from 0–100 |
| `countersteer_us` | Additional slow-reference countersteer contribution |
| `memory_feedback_us` | Drift Memory correction after its limit |
| `driver_activity_blend` | Driver steering-change activity |
| `steering_activity_us_s` | Filtered receiver steering rate |
| `transition_speed` | Saved centered response setting; `50` is neutral |
| `transition_speed_blend` | Instantaneous signed transition adjustment from -1 to 1 |
| `transition_authority_blend` | Detected driver/chassis transition envelope from 0 to 1 |
| `transition_prediction_scale` | Optional prediction multiplier; reduced while transitioning |
| `hunt_suppression` | Confidence-weighted attenuation applied to a confirmed periodic residual |
| `hunt_frequency_hz` | Detected settled-drift oscillation frequency |
| `hunt_residual_dps` | Fast yaw component around the slow sustained-drift baseline |
| `hunt_removed_us` | Correction removed from only that oscillating component |
| `hunt_consistent_half_cycles` | Number of consecutive frequency-consistent half-cycles observed |
| `hunt_latch` | Confirmed-event hold from 0 to 1; transitions clear it immediately |
| `anti_wobble` | Saved Anti Wobble notch-depth setting from 0-100; default `50` |

The stage-one onboard logger stores fixed-size binary records entirely in a
4 MB circular PSRAM buffer. It performs no internal-flash or filesystem writes
while driving. At the current 20 Hz sample rate, the complete telemetry set
retains approximately the newest 18 minutes of a run. Once full, the oldest
records are overwritten so the most recent behavior remains available.

Use **Download CSV** in the web configurator before removing power. CSV text is
generated from the binary records only during the download. The buffer is
volatile and is cleared by a power cycle or **Clear RAM Log**. A later SD-card
stage will continuously persist this same binary stream without returning to
internal FFat storage.
