# Architecture

How the O_C / Hemisphere firmware in `firmware/` works, for someone new to it, and where
our code hooks in. Paths are relative to `firmware/software/src/` unless stated otherwise.
Line numbers refer to the vendored `T32` snapshot (commit `9c870503`) and drift on upstream pulls.

## Big picture

```
            Teensy 3.2 (Cortex-M4, 120 MHz, no FPU)
 ┌───────────────────────────────────────────────────────────────────┐
 │ CORE_timer_ISR @ 16666 Hz  (Main.cpp:69)                          │
 │   display::Flush / DAC::Update / display::Update  (SPI, shared)   │
 │   ADC::Scan_DMA, DigitalInputs::Scan                              │
 │   OC::apps::ISR() ─► HEMISPHERE_isr() ─► BaseController()         │
 │        ClockSetup_instance.Controller()   (HS::clock_m tick)      │
 │        HS::frame.Load()    inputs → gate_high/inputs/clocked[]    │
 │        Controller()  ─► left applet  ->Controller()               │
 │                      ─► right applet ->Controller()               │
 │        HS::frame.Send()    outputs (slew/atten) → DAC             │
 ├───────────────────────────────────────────────────────────────────┤
 │ UI_timer_ISR (Main.cpp:56)  encoders/buttons → event queue        │
 ├───────────────────────────────────────────────────────────────────┤
 │ loop() (Main.cpp:177), lowest priority                            │
 │   GRAPHICS_BEGIN_FRAME → current_app->DrawMenu() → applet View()  │
 │     weegfx draws into 1-bit framebuffer (128x64)                  │
 │   GRAPHICS_END_FRAME   → frame handed to ISR, sent page by page   │
 │   current_app->loop(), UI events → OnButtonPress/OnEncoderMove    │
 └───────────────────────────────────────────────────────────────────┘
        │ SPI/DMA                         │ SPI            │ ADC/GPIO
   SH1106 OLED                       DAC (4 out)     4 CV in / 4 trig in
```

## Layers

**Core / HAL.** `Main.cpp` sets up SPI, calibration (`OC_calibration.*`), ADC
(`src/drivers/ADC/`), DAC (`OC_DAC.*`), digital inputs (`OC_digital_inputs.*`) and starts
`CORE_timer` at `OC_CORE_TIMER_RATE` (60 µs, `OC_config.h:18-19`). The ISR interleaves the
DAC update and display DMA on the shared SPI bus.

**Apps.** `OC_apps.cpp` declares the top-level apps with `DECLARE_APP` (Setup/About,
Hemispheres, Calibr8or, ...). Each app provides `isr`, `loop`, `DrawMenu`, event handlers,
and storage. We only care about the **Hemispheres** app (`APP_HEMISPHERE.h`).

**Hemisphere manager.** `APP_HEMISPHERE.h` holds the two applet slots (left/right; more
slots on larger hardware), presets (`HEM_NR_OF_PRESETS` = 4, 8 with `MOAR_PRESETS`,
`APP_HEMISPHERE.h:81-83`), applet selection, fullscreen/zoom, the input-map editor and the
ClockSetup overlay. `HSApplication.h:77` `BaseController()` is the per-tick sequence shown above.

**IOFrame.** `HSIOFrame.h:472` `struct IOFrame` (`HS::frame`) is a shared per-tick cache:
`inputs[]`, `gate_high[]`, `clocked[]`, `outputs[]` (slewed), trigger countdowns, MIDI
state. `HS::IOFrame::Load()` (`HemisphereApplet.cpp:86`) pre-computes `clocked[ch]` for every
applet input channel: from the internal clock when that channel has a ClockSetup multiplier
set, otherwise from its trigger map (`HS::trigmap[ch].Clock()`, `CVInputMap.h:96`). Applets
read the frame through helpers; they never touch hardware.

**Applets.** Subclasses of `HemisphereApplet` (`HemisphereApplet.h`): `Start`, `Controller`
(every tick), `View` (each redraw), `OnButtonPress`, `OnEncoderMove`, `OnDataRequest` /
`OnDataReceive` (one packed `uint64_t`), `SetHelp`. Helpers: `In`, `Out`, `Clock`, `Gate`,
`ClockOut`, `GateOut`, `gfx*`. The registry in `hemisphere_config.h` (`AppletRegistry reg{
DeclareApplet<Class>{id, categories}... }`) decides which applets are compiled in.

**Clock.** `HSClockManager.h` `class ClockManager` (`HS::clock_m`): tempo, tap, shuffle,
external sync PPQN, MIDI clock, per-channel multipliers. Edited in `applets/ClockSetup.h`,
a built-in overlay opened by pressing both buttons (`APP_HEMISPHERE.h:671`). Full analysis
in `docs/clock/MASTER_CLOCK_CONCEPT.md`.

**Display.** `src/drivers/weegfx.*` is pure software rendering (lines, text, bitmaps) into a
1-bit framebuffer; `src/drivers/display.cpp:26-30` holds `graphics` and a double-buffered
`FrameBuffer<SH1106_128x64_Driver::kFrameSize, 2>`. `SH1106_128x64_driver.*` streams the
finished frame to the OLED from the ISR. The left hemisphere draws at x 0..63, the right at
64..127 (the applet API offsets x automatically).

**Persistence.** Hemisphere presets live in EEPROM (via the app storage in `OC_apps.cpp`).
A preset stores: both applet IDs, one `uint64_t` per applet, the ClockSetup `uint64_t`
(`HEMISPHERE_CLOCK_DATA1..4`, `APP_HEMISPHERE.h:57-60`), a "globals" word and the
trigger/CV input maps. Saves happen only when data changed (`APP_HEMISPHERE.h:277-305`).

## Where our code hooks in

```
custom/core/*.h          pure algorithms (no Arduino/O_C includes)  ── host/ tests
      ▲
custom/applets/*.h       HemisphereApplet subclasses, IDs 200+
      ▲
custom/hemisphere_config_custom.h   our AppletRegistry (minimal set)
      ▲
firmware/ (upstream)     selected via the custom_min env; see BUILD.md / UPSTREAM.md
```

- **Applets**: new files in `custom/applets/`, registered in
  `custom/hemisphere_config_custom.h`. No upstream file needs editing to add an applet.
- **Cores**: `custom/core/`, compiled into the firmware via the applet include and into host
  tests via `host/`. See `docs/HOST_SIM.md`.
- **Clock buses** (planned): require edits to `HSClockManager.h`, `CVInputMap.h`,
  `HemisphereApplet.cpp`, `applets/ClockSetup.h`, `APP_HEMISPHERE.h`. These are the only
  intended invasive upstream edits; they are delimited and listed in `docs/UPSTREAM.md`.
