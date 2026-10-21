# ATTiny24-ghost-display

## Visual preview

Run `powershell -ExecutionPolicy Bypass -File tools\preview.ps1` from the project
directory. Requires `g++` and Node.js on PATH. This compiles the actual
`src/main.cpp` and controller with `tools/HalHost.cpp`, then runs a local bridge
at `http://127.0.0.1:8080`. Keep the terminal open; Ctrl+C stops the bridge and
C++ process. Rerun after changing the frame/font/animation configuration.

The page provides circular and unwrapped live views, pause/resume, RPM changes,
and mounting orientation. There are no exported frames or replay timeline.
The browser keeps physical angular column values across sensor pulses. Each
live write overwrites only its own angular column, including writes of zero.
Step column and Step frame (one sensor rotation) pause playback and advance the
actual C++ event loop. They do not skip an entire animation entry.
At the sensor pulse (position zero), the primary strip points horizontally left;
the secondary strip points right, 180 degrees apart. PA0 is innermost and PA7
outermost on both strips. Default rotation is clockwise; UI orientation controls
are optional overrides. The unwrapped view places PA7 on top and PA0 on bottom.
`include/DisplayLayout.hpp` defines the core geometry and 128-column resolution;
the host reports it in the live stream. Font row 0 maps to PA7 (outermost).
TwoHalves starts at the left sensor edge, traverses the upper half, then reverses
column order and radial bits on the lower half to keep its content upright.

### Display content

```cpp
makeFrame("CONTINUOUS CIRCLE") // One 128-column circular frame.
makeTwoHalves(makeHalfFrame("TOP"), makeHalfFrame("BOTTOM"))
virtualFrame<0>(255, Animation::None, Strip::Second)
```

Each HalfFrame holds 64 columns and validates its own text capacity. TwoHalves
centers text in each half by default; pass TextAlignment::Left or
TextAlignment::Right to makeHalfFrame to override. Strip selection belongs to
AnimationEntry, not Frame: reuse the same content on either strip using the
third virtualFrame argument. Physical frames contain only content and mode.
requires two explicit HalfFrames; an empty string leaves that half blank. Use
the same HalfFrame twice only when duplication is intentional. The composed
Frame stores both halves in one 128-column buffer, without additional RAM.
Animation effects run independently within each half's 64-column bounds, using
the timeline entry's shared effect and progress. Circular effects retain the
full 128-column bounds. The old single-text TwoHalves constructor is removed.
The circular geometry illustrates persistence of vision, not physical brightness
or an optical simulation.
The host HAL's `idle()` reads commands from stdin and dispatches simulated
rotation, overflow and compare events serially. Each `writeColumn()` emits a JSON
line to stdout. The Node bridge reads these lines and streams them to the browser
using Server-Sent Events. Only hardware is replaced; main and controller are real.
The bridge advances simulated time every 20 ms, independent of OS timer accuracy.
Pause freezes simulated time. RPM alters sensor pulse spacing, including controller
resynchronization behavior. Set `PREVIEW_TRACE=1` to also print JSON in the terminal.
For console-only operation, run `.pio\preview\preview_host.exe` and enter commands
such as `advance 12500` or `period 6250` (ticks are 8 microseconds).

## Portable display HAL

`include/Hal.hpp` defines the link-time hardware contract using portable types.
`src/HalATTiny24.cpp` owns AVR registers, interrupts, sleep and flash reads.
`src/DisplayController.cpp` contains the shared rotation/display state machine.
No virtual dispatch, heap allocation or AVR headers are required by the controller.

Run from the project directory:

```text
pio run -e attiny24
pio test -e native
pio test -e native_hal
```

The controller test supplies a mock HAL and drives the real rendering pipeline.
For a separate CLI or GUI, compile `src/DisplayController.cpp` with your own
implementation of the functions in `include/Hal.hpp`, excluding firmware `main`
and `HalATTiny24.cpp`. Initialize the HAL, reset the controller, then dispatch
rotation, column-deadline and timer-overflow events through
`include/DisplayController.hpp`. Serialize these events on one thread. Capture
`writeColumn` and `selectStrip` to visualize output. Use the same wrapping tick
clock and deadline-rejection rules documented in `Hal.hpp`; the test mock is an
example. The offline preview above provides a graphical consumer of captured output.

`HAL_STORAGE` preserves flash placement on AVR without exposing AVR headers.
Host builds use ordinary memory reads. AVR timestamp reads protect the timer
snapshot against interrupts. Timeout checks retain the firmware's timer-overflow
cadence rather than introducing a new periodic timer.


# avr-gcc-16.1
Toolchain can be downloaded from https://github.com/ZakKemble/avr-gcc-build

## Animation cost measurements

Run `python tools/measure_animation_size.py` to build isolated copies of the
firmware with individual and leave-one-out animation families. The actual
configuration is not changed. Results are written under `.pio/effect-size`.
Measurements include schedule metadata and compiler/LTO interactions; family
costs are not additive.

After rotation-state hoisting and the regular Blink implementation, the controlled
all-family image is 1856 bytes. Removing each family saves: Wipe 10 bytes,
Walk 36 bytes, Typewriter/ForwardDelete 50 bytes, Blink 28 bytes, OneByOne 132
bytes. These figures describe the tool's synthetic schedule, not arbitrary user
configurations. Unconfigured families are compile-time excluded.

Blink now uses four equally spaced off intervals over its duration. Each off
interval occupies 1/16 of the entry (75% visible overall), and both endpoints
are visible. Duration is measured in rotations, not wall-clock time; very short
entries can sample past intervals. Unlike the old pseudo-random dropout envelope,
it never deliberately spends the middle of the effect almost entirely blank.

OneByOne shares Typewriter's column-progress cursor and `roundToEmpty` boundary
snapping. It displays only the last character revealed by that cursor, holding
it across empty gaps. It starts blank and ends on the last character. Characters
retain their original positions, and each half resolves its own boundaries.
Timing is column-based rather than equal time per character; no character-count
pass is needed.
