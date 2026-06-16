# SGNode Plug Controller Tuning

This note explains the Plug controller parameters available from SGNode Base.
The goal is practical tuning during water tests and later real fermentation.

## Where The Settings Live

- Open Base `Details`.
- Tap the `PLUG` block 5 times.
- The Plug setup page opens.
- Change a value with `UP`, `DN`, `-`, `+`.
- Press `SAVE`.

Base stores the global controller settings on the SD card:

```text
/data/plug/gov_settings.json
```

These settings are Plug/hardware settings, not batch settings. A test schedule
should only define beer targets and hold times. Controller tuning comes from
`gov_settings.json` and is sent to the Plug with every command.

## Mental Model

Base sends the beer target. The Plug measures:

- `Beer`: sensor in the liquid
- `Air`: sensor in the fridge air

The Plug computes an `Air target`. The compressor relay then follows the air
target with hysteresis.

Simplified:

```text
beer error = beer target - beer actual

air target = beer target
           + P correction
           + I correction
           + D brake
```

Interpretation:

- Beer warmer than target: air target goes colder.
- Beer colder than target: air target goes warmer.
- Beer falling fast: D brake raises air target to reduce undershoot.
- Beer rising fast: D can gently lower air target to reduce overshoot.

The relay itself does not look directly at beer temperature. It cools the air:

```text
relay ON  if air > air target + Air on
relay OFF if air < air target + Air off
```

`Air off` and `Air on` are intentionally above the air target to compensate for
fridge aftercool after the relay switches off.

## First Parameters To Touch

Most tests should start with only these values:

| Menu label | JSON key | Start value | What it does |
| --- | --- | ---: | --- |
| `P` | `kp` | `0.45` | Direct response to beer temperature error. Higher reacts harder. |
| `I Tn` | `integralTnHours` | `0.75h` | Integral speed when near target. Lower means faster correction. |
| `D brake` | `dBrakeHours` | `0.80h` | Brakes fast beer temperature movement. Higher brakes earlier/stronger. |
| `Air off` | `airTurnOffAboveTargetC` | `0.50K` | Relay switches off at air target + this value. Higher reduces undershoot. |
| `Air on` | `airTurnOnAboveTargetC` | `1.10K` | Relay switches on at air target + this value. Higher reduces cycling. |
| `Beer lock` | `beerUndershootLockoutC` | `0.10K` | Blocks cooling when beer is already below target by this amount. |
| `D max` | `maxDOffsetC` | `0.90K` | Maximum D correction. Lower avoids excessive air target jumps. |
| `D warm` | `warmingDFactor` | `0.25` | D strength while beer is warming. Keep much smaller than cooling D. |

For the current 13 L water/fridge setup, based on the stable 9 C hold and the
clean 4 C hold seen on the earlier SD data, use:

```text
P          0.45
I Tn       0.75h
D brake    0.80h
Air off    0.50K
Air on     1.10K
Beer lock  0.10K
D max      0.90K
D warm     0.25
```

## Full Parameter Reference

### P

Direct proportional correction.

- Increase if beer target is missed for a long time and air target looks too timid.
- Decrease if air target moves too aggressively or the relay cycles too hard.
- Good test range: `0.30` to `0.70`.

### I Tn

Integral time constant in hours. Active only near target.

- Lower value: integral corrects faster.
- Higher value: integral corrects slower.
- `0` disables integral action.
- Good test range: `0.5h` to `2.0h`.

Use lower `I Tn` when the hold phase sits consistently too cold or too warm.
Use higher `I Tn` if the air target slowly walks away after a disturbance.

### D brake

Derivative brake based on measured beer rate in K/h.

- Beer falling: D raises air target to reduce cold undershoot.
- Beer rising: D is multiplied by `D warm`, so it brakes warm overshoot gently.
- Good test range: `0.5h` to `1.5h`.

If cooling undershoots below target, increase `D brake` or reduce `D max` less.
If the controller becomes hesitant and never cools hard enough, lower `D brake`.

### Air off

Relay-off threshold relative to air target.

```text
relay OFF if air < air target + Air off
```

Increasing `Air off` turns the compressor off earlier/warmer. This is useful
when the fridge keeps cooling strongly after relay off.

### Air on

Relay-on threshold relative to air target.

```text
relay ON if air > air target + Air on
```

Increasing `Air on` makes the compressor wait longer before restarting.
This reduces short cycling.

Keep `Air on` greater than or equal to `Air off`. A useful gap is `0.4K` to
`0.8K`.

### Min on / Min off

Minimum relay on/off times.

- `Min on`: compressor must stay on at least this long.
- `Min off`: compressor must stay off at least this long.

Current defaults:

```text
Min on   120s
Min off  300s
```

For a real fridge, do not make `Min off` too short.

### I cold band

Integral is active when beer is too cold but close enough to target:

```text
0 <= beer error <= I cold band
```

Beer error is `target - beer`.

Increase if cold undershoot recovery is too slow near target.
Decrease if the integral fights too early during larger ramps.

### I warm band

Integral is active when beer is too warm but close enough to target:

```text
beer is above target by up to I warm band
```

Increase if warm-side steady-state error remains too long.
Decrease if it winds up during cooling ramps.

### I max + / I max -

Limits for integral correction.

- `I max +`: maximum warm air offset from integral.
- `I max -`: maximum cold air offset from integral.

`I max -` is negative. Keep it modest so the controller does not keep pulling
the air target colder after the fridge aftercool is already doing the work.

### I leak

How fast the integral decays when it is outside its active band.

- Higher: old integral memory disappears faster.
- Lower: old correction persists longer.

Good range: `0.5` to `2.0` per hour.

### Cross keep

How much integral remains when the beer error changes sign.

- `0.00`: erase integral at crossing.
- `0.25`: keep 25 percent.
- `1.00`: keep all integral.

Low values reduce overshoot after crossing the target.

### D max

Maximum absolute D correction in K.

If beer rate is noisy, D can cause large air target jumps. `D max` caps that.

Good range: `0.6K` to `1.2K`.

### D warm

Multiplier for D while beer is warming.

Example:

```text
D brake = 0.80h
D warm  = 0.25
effective warming D = 0.20h
```

Keep this weaker than cooling D. Passive warmup is slow; too much warming D can
start cooling too early.

### Beer lock

Compressor lockout when beer is already below target:

```text
cooling blocked if beer < beer target - Beer lock
```

Lower value protects against undershoot earlier.

For the 4 C hold, `0.10K` is more suitable than `0.20K`, because the observed
stable band was already slightly below target.

### Warm rate

Exception for upward target steps. If beer is below target but warming faster
than this rate, the Plug may allow cooling to prevent overshoot.

Keep around `1.0K/h` for now. The observed passive warm ramp was around
`0.3K/h`, so cooling stayed blocked, which was correct.

### Under err / Under air

Strong undershoot recovery.

If beer is colder than target by more than `Under err`, the air target is forced
to at least:

```text
beer target + Under air
```

This is mainly a recovery helper after a large cold overshoot.

### Air min / Air max

Hard clamp for computed air target.

- `Air min`: coldest allowed air target.
- `Air max`: warmest allowed air target.

Defaults are intentionally broad:

```text
Air min  1.0 C
Air max  30.0 C
```

Do not use these as normal tuning knobs unless a hardware limit requires it.

### Target step

Beer target change that resets integral memory.

If the target changes more than this amount, the controller clears integral
state so the old phase does not pollute the next one.

Default `0.25K` is reasonable.

## Tuning Recipes

### Beer sits stable below target

Example: target `4.0 C`, beer pendles between `3.7` and `4.0 C`.

Try in this order:

1. Lower `Beer lock` to `0.10K`.
2. Raise `Air off` by `+0.1K` to `+0.2K`.
3. Raise `Air on` by the same amount or a little more.
4. If still too cold after several cycles, lower `D max` slightly or reduce `D brake`.

### Beer overshoots cold during cooling ramp

Try in this order:

1. Increase `D brake`.
2. Increase `Air off`.
3. Reduce `Beer lock`.
4. If D jumps are too harsh, reduce `D max`.

### Beer never reaches target

Try in this order:

1. Increase `P`.
2. Lower `I Tn` if it is already near target.
3. Lower `Air off` and `Air on` slightly.
4. Raise `D max` if D is clipping too early.

### Relay cycles too often

Try in this order:

1. Increase gap between `Air off` and `Air on`.
2. Increase `Min off`.
3. Slightly lower `P`.

### Warm ramp risks overshoot

Try in this order:

1. Keep `D warm` low, around `0.20` to `0.35`.
2. Keep `Warm rate` around `1.0K/h`.
3. Do not cool during normal passive warmup around `0.3K/h`.

## Suggested Water Test Procedure

Change only one or two parameters per phase.

For every change, note:

```text
timestamp
beer target
beer min/max
air min/max
air target min/max
duty
parameter changes
```

Judge the controller by the last 2 to 3 hours of a hold phase, not by the first
cooling transient. The 9 C and 4 C holds looked stable enough to keep the
current defaults. Further tests should mainly check other fill levels and real
fermentation heat, not chase the current water-test hold behavior.

For the current setup, the most interesting validation windows are:

- 4 C hold after the controller has settled.
- Upward ramp from 4 C to 15 C.
- The next cooling ramp back down to 4 C.
