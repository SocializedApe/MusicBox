# Timers

Chip: 16 MHz. A timer = a counter that goes +1 every few clock ticks (prescaler).

| What | Clock | Job |
|---|---|---|
| Timer0 | 250 kHz | interrupt every 1.024 ms: tick + servo |
| Timer1 | 2 MHz | two notes: D9 (A) + D10 (B) |
| Timer2 | `tone()` | bass on D4 |
| INT1 | D3 edge | button play / pause |

## Timer0 — tick (~1 ms)

**Hardware**
- `TCNT0`: 8 bit, counts 0 → 255 → 0.
- Already started by Arduino for `millis()`: 16 MHz / 64 = 250 000 counts/s → 1 count = 4 µs.
- One round = 256 counts = **1.024 ms**.

**Setup**
```cpp
OCR0A = 128;               // compare value
bitSet(TIMSK0, OCIE0A);    // enable compare A interrupt
```
→ when the counter passes 128, `TIMER0_COMPA_vect` runs. Once per round = every 1.024 ms. `millis()` still works.

```
TCNT0:  0 ... 128 ... 255 → 0 ... 128 ... 255 → 0
                ↑ ISR             ↑ ISR
time:          0 ms            1.024 ms
```

**ISR**
- `tick = true` → `loop()` knows 1 ms passed.
- Servo: `servoCount` 1 → 20. At 20: pin HIGH, back to 0. At `servoPulse`: pin LOW.
  - `servoPulse = 1` → ~1 ms pulse → stick **up**.
  - `servoPulse = 2` → ~2 ms pulse → stick **down** (hit).
  - One frame = 20 ticks ≈ 20.5 ms.

**loop() on each tick**
- `hitLeft` −1 → at 0 servo back up (80 ticks ≈ 82 ms down).
- `clickLeft` −1 → at 0 relay off (30 ticks ≈ 31 ms on).
- `noteLeft[v]` −1 for all 5 voices → at 20 note stops (gap), at 0 next note.
- Note length: `len * (15000 / 1.024 / BPM)` ticks. 145 BPM → one 16th = **101 ticks** ≈ 103 ms.

## Timer1 — two notes (D9, D10)

**Hardware**
- `TCNT1`: 16 bit, counts 0 → 65535 → 0.
- Two alarms: `OCR1A` → pin **D9**, `OCR1B` → pin **D10**.

**Setup**
```cpp
TCCR1A = 0;               // normal mode, pins not connected
TCCR1B = 0;
bitSet(TCCR1B, CS11);     // prescaler 8 → 2 MHz → 1 count = 0.5 µs
bitSet(TIMSK1, OCIE1A);   // interrupt on A
bitSet(TIMSK1, OCIE1B);   // interrupt on B
```
Counter never resets, wraps every 32.8 ms.

**Start a note** (`playNote`, v 1), e.g. 880 Hz:
```
half  = 1000000 / 880 = 1136 counts (568 µs)
OCR1A = TCNT1 + 1136      // counter at 30000 → first flip at 31136
COM1A0 = 1                // D9 connected: toggle on every match
```
- `noInterrupts()` around it so the ISR can't use a half-written value.
- `1000000` = 2 MHz / 2 (half a period). `UL` because it doesn't fit in `int`.

**Over time**
```
TCNT1:  30000 ... 31136 ... 32272 ... 33408
D9:     LOW       HIGH      LOW       HIGH
ISR:              OCR1A=32272  OCR1A=33408  OCR1A=34544
```
- Hardware flips the pin. ISR only moves the alarm: `OCR1A += melodyHalf`.
- 2 flips = 1 wave = 1.136 ms → 880 Hz.
- Wrap is fine: 65000 + 1136 → 600 (16 bit), counter also reaches 600 after its wrap.

**Two notes**
- B = same thing with its own step: D10 flips every `harmHalf` counts.
- One counter, two alarms → two pitches.

**Stop:** `bitClear(TCCR1A, COM1A0)` → D9 disconnected → silent.

**Limits:** max half 65535 → lowest note ≈ 15 Hz. 1 kHz note = 2000 ISRs/s, few µs each.

## Timer2 — bass (D4)

**Hardware:** 8 bit, CTC mode: counts 0 → `OCR2A`, then resets itself.

**`tone(D4, f)`** (Arduino library)
- Tries prescalers 1, 8, 32, 64, 128, 256, 1024 until `16 MHz / prescaler / (2f)` fits in 255.
- e.g. 220 Hz: prescaler 256 → 16 000 000 / 256 / 440 ≈ 142 → `OCR2A = 141`.
- Each match: counter resets, ISR flips D4.

```
TCNT2: 0 → 141 → 0 → 141 → 0
D4:        flip      flip      → 220 Hz
```

**`noTone()`** stops it.

Difference to Timer1: any pin, but one note only. Timer1 flips its own pins in hardware, two notes.

## INT1 — button (D3)

**Setup**
```cpp
pinMode(BUTTON_PIN, INPUT_PULLUP);   // D3 sits HIGH
bitSet(EICRA, ISC11);                // falling edge (HIGH → LOW)
bitSet(EIMSK, INT1);                 // enable INT1
```

**One press**
```
D3:   HIGH ─┐ ┌┐ ┌─┐ ┌──
            └─┘└─┘ └─┘ LOW
edges:      ↓  ↓   ↓        ~4 edges from bounce
ISR:        flip ignored ignored   (millis() - lastPress < 200)
```

**ISR:** flips `paused`, saves time. `loop()` sees `paused != wasPaused`:
- pause → stop notes, relay off, servo up, save bar to EEPROM
- play → `seek()` to saved bar, start

## All together: one 16th (~103 ms)

```
Timer1:  D9 + D10 flip in hardware all the time → melody + harmony
Timer2:  D4 flips → bass
Timer0:  101 ticks. Each tick: servo pulse (ISR), noteLeft[v] −1 (loop)
         tick 81  (noteLeft = 20): notes stop → short gap
         tick 101 (noteLeft = 0):  next notes → new OCR1A / OCR1B / tone / relay / servo
INT1:    only on button press
```

## volatile
Changed in an ISR or read by one: `tick`, `servoPulse`, `melodyHalf`, `harmHalf`, `paused`, `lastPress`.
