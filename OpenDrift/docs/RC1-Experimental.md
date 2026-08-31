# Experimental Transition Response

Transition Speed is a centered direction-change adjustment. A value of `50`
leaves the normal direct gyro correction and Max Correction authority intact.

Unlike the former Tail Slide Speed experiment, the adjustment follows the
complete transition envelope: driver steering announces the transition and
measured chassis yaw keeps it active through the physical direction reversal.
Values below 50 add fast yaw damping for a slower, more deliberate transition.
Values above 50 reduce damping for a faster transition. Transition Speed never
changes the hard Max Correction ceiling. It does not command rotation, reverse
gyro correction, or change steady Countersteer Assist.

Transition prediction is also tapered while a direction change is in progress.
If acceleration prediction reaches the new yaw direction before the measured
chassis does, the controller now falls back to continuous measured-yaw damping
instead of briefly dropping correction to zero.

## First test

1. Keep the proven track profile unchanged and set Transition Speed to `50`.
2. Confirm that entries and transitions are predictable with the neutral path.
3. Test `25` for slower rotation and `75` for faster rotation using the same
   entries and transitions. The wide first comparison is intentional.
4. Work back toward `50` in steps of `5` or `10` once the preferred direction
   is obvious.
5. Capture a blackbox log at `50` and at the preferred experimental value.

Compare transition duration, peak yaw rate, overshoot after the direction
change, steering activity, `transition_speed_blend`,
`transition_authority_blend`, and `transition_prediction_scale`. Avoid changing
Gain, Prediction, Hold Assist, or Countersteer Assist during the comparison.

## Blackbox fields

- `transition_speed`: saved setting from 0-100, centered at 50.
- `transition_speed_blend`: instantaneous signed -1 to 1 response adjustment.
  Negative values add damping; positive values release it.
- `transition_authority_blend`: detected transition envelope from 0 to 1.
- `transition_prediction_scale`: multiplier applied to optional acceleration
  and throttle look-ahead. It approaches 0.25 during a full transition.

## Settled-drift residual damping

Blackbox runs 39 through 47 identified the remaining wheel wobble as a
closed-loop mode centered near 3.2 Hz. Broad residual suppression improved it
around strength 70, but higher values removed useful fast gyro damping and made
the car worse. Control now uses a narrow notch that starts at 3.2 Hz and slowly
tracks confirmed oscillation between 2.5 and 3.6 Hz whenever the car is settled
and driver input is quiet. A deliberate transition or strong steering input
freezes frequency tracking while retaining a shallow guard notch. Suppression
moves smoothly between the transition guard, entry depth, and full settled
depth instead of abruptly releasing the resonant band at each state change.

Anti Wobble is the only control. It defaults to `50`. At `0`, the notch is bypassed. Higher values
increase notch depth, up to the complete tracked-notch response at `100`. Slow
chassis yaw, fast corrections outside the wobble band, Countersteer Assist,
and Prediction remain active. The frequency detector and latch remain in the
blackbox as diagnostic telemetry but do not gate control.

- `hunt_residual_dps`: instantaneous yaw isolated by the tracked notch.
- `hunt_removed_us`: direct correction withheld from that residual.
- `hunt_consistent_half_cycles`: consecutive half-cycles agreeing on frequency.
- `hunt_latch`: remaining confirmed-event hold from 0 to 1.
- `anti_wobble`: saved Anti Wobble notch-depth setting from 0-100.
- `hunt_residual_envelope_dps`: continuous residual-energy diagnostic.
- `hunt_notch_center_hz`: slowly tracked center frequency currently applied
  by the dynamic notch.
