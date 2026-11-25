# AGENTS.md — guide for AI agents working on 3D-Pinball-macos

Read this file fully before changing anything. Deeper reverse-engineering notes are in `research/NOTES.md`; this file holds the rules, the architecture, the decoded facts you need most often, the verification procedure and the known pitfalls.

## 1. What this project is

A from-scratch macOS (Apple Silicon) port of *3D Pinball for Windows – Space Cadet*, written in C++17 + SDL2. It loads the original game's own data (`PINBALL.DAT` etc.) at run time and re-implements the engine. Behaviour (physics constants, formulas, scoring values, file layout) was recovered by analysing the original 32-bit `pinball.exe` (Windows XP SP3 build, file version 5.1.2600.5512) with REA (Reverse Engineer Anything, an MCP server) driving Ghidra 12.2.

Status (v0.3.0): **feature-complete rules port, verified by automated tests only.** Implemented on top of v0.2.0: ramps with layers (main ramp, raised platform, ramp hole), the whole component rule table (`rules_game.cpp`), the mission machine (states 0-0x20: select, 17 missions, maelstrom stages), ranks/fuel, multipliers, bonus/jackpot lamps, extra balls, ball save, skill shot, hyperspace chute sequence, centre post, text boxes with a built-in 5x7 font, tilt warning, high scores (`highscores.txt` in the user's Application Support folder). **Not ported / approximated:** `s_ramp9` (right thin ramp) entry (its data disables entry and it is never reachable), multiball (third ball lock = extra ball), drop-target animation, most light chase animations (fill/drain only), exact fuel burn timing, exact timings of lamp effects, `FUN_01011985` score cycling.

The project has not been play-tested interactively by a person; verification is automated (section 6).

## 2. Hard rules (do not break these)

**Legal / content**
1. Never add, commit, copy into the repo or attach to a release any original game material: `PINBALL.DAT`, `FONT.DAT`, `*.WAV`, `*.MID`, `pinball.exe`, `table.bmp`, `wavemix.inf`, the icon extracted from the exe, screenshots or videos showing the game's artwork.
2. Never publish decompiler output (`research/decomp/`, Ghidra pseudocode) or paste long stretches of decompiled code anywhere. Record *facts* (constants, formulas, layouts, function addresses) in your own words. Reimplement; do not transliterate.
3. The public release (`scripts/package.sh --release`) must contain no game data and no game icon. The script enforces a scan; never weaken it. The personal build (`scripts/package.sh <game-folder>`) bundles the user's game files and must never be shared.
4. Do not copy code or assets from other pinball ports or any third-party project, and do not fetch game assets from the internet. Only use game files the user supplies locally.
5. The repository is MIT-licensed without a copyright line. The license never covers game assets. Keep the legal notice in the README accurate.

**Privacy**
6. Never write usernames, home-directory paths, e-mail addresses or machine names into any file. Tools read the game folder from `PINBALL_DIR` (default `./Pinball`); scripts use relative paths.

**Process**
7. Do not push, tag, publish a release or change remotes unless the repository owner explicitly tells you to, and tell you where. Commit only when asked. When you commit, end the message with the attribution line your environment specifies.
8. Work only inside the repository folder (and your own tool-config directory). Keep temporary files inside the repo (for example `research/out/`, `build/tmp.*`) and delete them when done: `build/`, `research/out/`, `dist/release/`, `dist/dmgroot/`, `research/mnt/`, `__pycache__/`, `.DS_Store`, and any disk image you mounted (`hdiutil detach`). Never leave volumes mounted.
9. Be honest about results. Report failing or skipped checks with their output. Do not claim human review or interactive play-testing that did not happen.
10. Keep `README.md` short and end-user oriented. Put technical depth here and in `research/NOTES.md`.

## 3. Repository layout

```
CMakeLists.txt          Release flags: -O2, LTO, -fno-exceptions -fno-rtti, dead_strip; links SDL2, AudioToolbox, CoreFoundation
LICENSE                 MIT (no copyright line)
README.md               end-user readme
AGENTS.md               this file
scripts/package.sh      --release (public .dmg, no game data) | <game-folder> (personal build with data)
src/
  scene.h/.cpp          component sprites (frames, per-pixel depth maps, timed effects, ball occlusion)
  rules.h/.cpp          Light, LightGroup, bumper levels and the ported rule handlers (SDL-free, unit-tested)
  main.cpp              args, game-folder lookup, window/render loop, sprites, HUD digits, key handling, built-in tests (selftest, soak, probes)
  partout.h/.cpp        PINBALL.DAT loader and attribute helpers
  physics.h/.cpp        World: build from data, 1 ms step, collisions, scoring, holes, gates, flippers, plunger, nudge/tilt
  camera.h              table-to-screen projection
  audio.h/.cpp          WAV mixer (8-bit mono 11025 Hz)
  music.h/.cpp          MIDI playback through macOS's built-in synth (AudioToolbox MusicPlayer)
  rules_data.h          per-component point values (from the exe's rules table)
research/
  NOTES.md              detailed findings with function addresses
  tools/                partout.py, render.py, overlay.py, vtable.py, rules_table.py, make_icon.py (personal build), make_release_icon.py (release), dat_names.py + annotate.py (name the descriptors in decompiled handlers)
research/decomp/        LOCAL ONLY, git-ignored: Ghidra pseudocode (see section 9)
dist/                   build output, git-ignored
```

## 4. Build, run, package

```sh
brew install cmake sdl2-compat sdl3
cmake -S . -B build && cmake --build build
./build/pinball ./Pinball                  # run from a source checkout (game files in ./Pinball)
./scripts/package.sh --release             # dist/3D-Pinball-macos-<version>.dmg
./scripts/package.sh ./Pinball             # personal build -> dist/3D Pinball.app (+ dist/3D-Pinball-personal.dmg)
```

Game-file lookup order (`findGameDir` in `main.cpp`): `$PINBALL_DIR`, `Contents/Resources` inside the app, `~/Library/Application Support/3D-Pinball-macos`, `../Pinball`. With none found the app shows a dialog and offers to open the Application Support folder (`PINBALL_NO_DIALOG=1` prints the message and exits with code 2 instead — use it in tests).

Binary options: `--selftest`, `--soak <seconds>`, `--debug` (collision overlay; F1 toggles), `--demo`, `--shot <file.bmp>` (render 3 frames, save, exit), `--ballat X,Y` (freeze the ball at a table position), `--no-intro` (skip the how-to-play alert), `-fullscreen`. Environment: `PINBALL_DIR`, `PINBALL_NO_DIALOG`, `SOAK_SEED`, `SOAK_TRACE`, `SOAK_EVERY`, `PROBE_X`/`PROBE_Y` (contacts at a point), `EJECT=<hole name>` (trace an ejection).

Controls (the original's defaults): Z / Left, `/` / Right = flippers; Space = plunger (hold, release); X, `.`, Up = nudge left / right / bottom; F2 new game; P or F3 pause (also on focus loss); F4 full screen; M music (off by default); F1 overlay; H how-to-play alert (also shown once at first start unless "Don't show again" is chosen); Esc quit.

macOS notes: there is no `timeout` command; use background runs or `perl -e 'select(undef,undef,undef,N)'` for short waits. `sips -s format png in.bmp --out out.png` converts screenshots; `sips -c H W --cropOffset Y X` crops. On a retina display the screenshot is 4× the 600×416 logical size.

## 5. Architecture and decoded facts

### 5.1 Data file (`PARTOUT(4.0)RESOURCE`)
Header 183 bytes (+ `extra` bytes at offset 181); group count is a u16 at offset 175. Each group: `u8 count`, then entries `u8 type` + payload. Length rules: a size table `{2,-1,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,0}` indexed by type; `-1` means a u32 length precedes the data. Type 1 (bitmap) has a u32 length, then a 14-byte header (res u8, w, h, x, y i16, size u32, flags u8) and 8-bit bottom-up pixels, stride `(w+3)&~3`. Types: 0 = u16 component kind, 1 bitmap, 3 name (C string), 5 palette (256×BGRX, in the `background` group), 9 string (e.g. `soundN.wav`), 10 int16 chain, 11 float32 chain, 12 depth map (used for ball occlusion, see `scene.cpp`).
- **Int attributes (type 10)** are flat `(id, value)` pairs (id 1500 consumes 9 shorts; the `table_objects` list starting with id 1025 is not a chain). Use `Group::intAttr(id)`. Key ids: 100 state count, 300 material group, 304 hit sound, 400 kicker group, 406 kick sound, 602 collision layer, 1027 light-group members.
- **Float attributes (type 11)** are `[id, values…]`. 600 = wall polyline `[count, x,y…]` (count 1 = circle `[1,x,y,r]`), 601 = sink circle, 800–805 flipper, 305/306/408 kick-out, 301/302 material, 401–406 kicker chain, 700 table projection centre, 701 drag, 900/901 light timing.
- Table build: the last group `table_objects` lists `(class id, group index)` pairs (340 components, 29 classes) used by the original's constructor switch.

### 5.2 Camera and layout
`camera_info`: 15 floats = 3×4 matrix `M` (f[0..11]) and focal length `F` = f[12] (−400). Projection: `p' = M·(x,y,z,1)`, `sx = cx + F·x'/z'`, `sy = cy + F·y'/z'`, with **centre (cx, cy) = table float attr 700 = (183, 238)** (the original overrides the 300,208 passed at setup). The table picture is drawn at screen (0,0); sprite bitmaps positions are `bitmap.x/y − (137, 2)`; the info panel bitmap is drawn at (386, 12); logical area 600×416. The ball centre is at **z = +radius (0.3)**; the ball has seven bitmaps (9…15 px) in groups `ball` and the following six, chosen by nearest reference y (float 501 `[x,y,z]`).
Number boxes (`score1`, `ballcount1`, `player_number1`): int record `[first digit group, x, y, w, h]`, digits are consecutive bitmap groups, drawn right-aligned.

### 5.3 Physics (all in table units, fixed 1 ms step)
- Ball radius 0.3; max speed 60 (= 200 × radius).
- Gravity: table attr 305 = `[25, 0.5, π/2]` → `gy = 25·sin(0.5) ≈ 11.9856`, `gx = 0`. Drag `c = 0.2` (attr 701). Acceleration: `ax = gx − (noise·speed + vx)·c`, `ay = gy − vy·c`, `noise = 0.5 − (rand() & 0x7fff)/32768`, refreshed 60 times per second. **Mask `rand()` with `0x7fff`** — Windows' rand is 15-bit, macOS's is 31-bit.
- Bounce (`respond` in `physics.cpp`): with unit normal `n` toward the ball, `u = −n·d`; if `u>0`: `d' = normalize(smooth·(d + u·n) + elast·u·n)`; `fn = u·speed`; `speed −= (1 − elast)·fn`; if `fn ≥ threshold`: `d' = normalize(n·kick + speed·d')`, `speed = |…|`. Material group (via int 300): 301 = smoothness (default 0.95), 302 = elasticity (default 0.6). Kicker group (via int 400): 401 = threshold (default 9e10), 402 = kick, 406 = sound. Impact sound: material 304 when `0.2 < fn ≤ threshold`, else kicker 406. *Port-specific:* contacts with impact `< 1` and `< threshold` only remove the normal velocity (otherwise a fixed step would damp a rolling ball every millisecond).
- Collision layers: int 602, layer bit `1 << value`, default 1; the ball lives on layer 1. A part may carry several 602 entries (mask = OR); a part meets the ball when `part.mask & ball.layer`. Layer 2 = ramp, layer 4 = raised platform (ramp rails carry both).
- Sensors, not walls: group `drain`, `s_trip*`, `a_roll*` polygons (score on entering), `a_flag*` lines (spinner: score on crossing).
- One-way gates `s_onewy*`: edge `p0→p1`, normal `n = (dy, −dx)`; collide only when the ball is on the `+n` side and moving against `n`; a pass sensor 0.8·radius toward `+n` scores and plays the gate sound when crossed in the open direction.
- Flippers (`a_flip1/2`): 800 pivot `[x,y,r]`; 801 rest tip, 802 raised tip; 803 hit strength (0.9); 804 = 0.04 s up, 805 = 0.08 s down; swing angle = **shortest** signed difference of the two tip angles (atan2 wraps at ±π). Moving-flipper hit kick = `dot(surface dir, n)·(swing/time)·(r/reach)·0.9` with threshold −1 (always kicks); a resting flipper is a plain bounce.
- Plunger: ball start `(−7.021, 10.085)` (plunger attr 601); pull grows 1 per 25 ms to 100 (2.5 s); release kick = `pull·(1…1.1)`, capped at max speed; only when the ball rests on the plunger face (`atPlunger`).
- Holes: kick-outs `a_kout*` (circle `[x,y,r]`, pull force = attr 305, capture radius = attr 306 · r, pull inside r: `a = unit(c−p)·force − v`), sinks `v_sink*` (attr 601 capture circle). Capture snaps the ball to the centre, scores, plays the material sound, holds 1.5 s, then ejects along the kicker's 404 direction rotated by a random ±405 rad at speed 402 ± 403 %. Capture only triggers when entering from outside (hole re-arms after the ball leaves). `a_kout1` (gravity well) starts **inactive**.
- Nudge: X/./Up apply `(dx,dy)·0.5` to the ball with `(2,1)/(−2,1)/(0,1)`, the reverse impulse 0.4 s later, picture shift `(floor(dx+0.5), floor(0.5−dy))` px; tilt meter +4/s while a nudge is active, −1/s otherwise, tilt above 1.0 (flippers dead until the ball drains).
- Scoring: `rules_data.h` holds state-0 point values per component name (a_bump1–4 500, a_bump5–7 1500, rebounds 500, a_roll 500–20000, a_kout1 50000, a_kout3 20000, v_sink 2500, …). Score cap 1,000,000,000; the mission multiplier system is not ported.

### 5.4 Source modules
- `World::build` creates segments/discs/gates/holes/flippers/polygons from the data; `World::step(dt)` is the whole simulation; `events` collects sound-group ids for the app to play; `newGame/resetBall/releasePlunger/nudge/addScore/atPlunger` are the gameplay entry points.
- `main.cpp` runs the SDL loop with a 1 ms accumulator (clamped to 0.1 s), draws table → flipper/plunger sprites → ball → digits, and hosts the headless tests.

## 6. Verification procedure (run before reporting any change as done)

1. `cmake --build build` with no errors.
2. `./build/pinball ./Pinball --selftest` must print nineteen `test…` lines with the expected values (tests 10-13 below): 1 drop reaches the drain; 2 plunger shot reaches y ≈ −12; 3 flipper throws a resting ball up (≈ 18 u/s); 4 audio device opens and sounds are triggered; 5 pop bumper scores 500; 6 rollover scores 2000; 7 `a_kout3` captures, holds 1.50 s, ejects, gravity well inactive; 8 MIDI loads and plays (position ≈ 1.2 s after 1.2 s); 9 nudge adds +1.00 vx, shift returns to (0,0) after 0.4 s, three quick nudges tilt; 10 flippers are dead while tilted and work again after the drain with keys still held; 11 two quick nudges leave no picture shift; 12 lanes/flipper rotation/bumper level-up and decay; 13 target bank lamps and rebound flash.
3. `./build/pinball ./Pinball --soak 600`: four seeded simulated players; require **0 stuck events and no NaN**, several complete games each. Investigate any "stuck near (x,y)" with `PROBE_X/PROBE_Y` and the geometry scripts before changing physics.
4. Rendering changes: `--ballat X,Y --debug --shot f.bmp`, convert with `sips`, and look at the picture. Check the ball at the plunger rest `(−7.021, 11.675)`, a bumper centre `(0.01, −3.72)` and the left flipper pivot `(2.489, 12.063)`; collision lines and the ball must sit on the artwork.
5. Packaging changes: build `--release`, mount the image at a project-local mountpoint (`hdiutil attach -mountpoint research/mnt …`), verify there are no `*.dat *.wav *.mid *.exe *.bmp` files, `codesign -v`, no `/opt/homebrew` in `otool -L` of the executable and both dylibs, run the app with a temporary `HOME` containing the game files (symlink under `Library/Application Support/3D-Pinball-macos`) for `--selftest`, test the missing-files path with `PINBALL_NO_DIALOG=1` (exit code 2, folder created), launch with `open -n --env HOME=… <app>` and confirm it is still running after 5 s, then unmount and delete the temporary folders.
6. Add a self-test for every new behaviour. Keep the simulation deterministic per seed.

## 7. Known pitfalls

- Input state (`inL/inR`) is kept separate from `Flipper.pressed`; tilt clears only the latter. Nudge undo is one pending entry per nudge. Mixing these brought back the old stuck-controls bug.
- Component sprites are not in the table picture; they are drawn by `Scene` and the ball is re-occluded by nearer depth pixels. Class switch for initial visibility is in `Scene::load`.

- `rand()` must be masked (`& 0x7fff`) to match Windows.
- Flipper angles from `atan2` wrap; always use the shortest signed difference.
- The projection centre is attr 700 (183, 238), not the screen centre; the ball z is **+radius**.
- Int attributes are chains — a lookup that only checks the first id will miss data (bumpers' kicker refs, walls' materials).
- A fixed-step overlap solver re-applies a bounce every step: keep the slow-contact rule or rolling balls stick.
- Hole capture must be an edge crossing, otherwise an ejected ball is re-captured instantly.
- Homebrew's `sdl2-compat` dlopens a file named exactly `libSDL3.dylib` beside itself (not `libSDL3.0.dylib`); arm64 binaries must be (ad-hoc) signed after `install_name_tool`.
- Python tools: run with `PYTHONPATH=research/tools python3 …` (not `-I`, which drops the script directory); set `PINBALL_DIR`.
- `Application Support` first-run folder is created by the app; tests must set `HOME` to a project-local folder instead of touching the real one.
- Unmount every image you attach; macOS may also auto-mount a freshly created `.dmg` under `/Volumes`.

## 8. Approximations (labelled; do not present them as original behaviour)

Light message 0xf and group 0x10 (blink, then settle), hit-effect frame timings, the 60 s bumper decay refresh; flipper material (0.95 smoothness / 0.45 elasticity default) when a group has none; slingshot plates `a_kick*` kick on any hit above 0.5 (the data has a kick but no threshold); sinks and kick-outs use the default 1.5 s hold; scoring uses state 0 for every component and no multipliers; the tilt warning text box is omitted; the ball sprite is chosen by nearest reference y (the original's exact switch points are unknown); layer-2/4 parts are skipped.

## 9. Continuing the reverse engineering

1. Obtain the exe locally (never commit it). Configure REA with Ghidra 12.2+ (`GHIDRA_INSTALL_DIR`, a real JDK in `JAVA_HOME`). `open_binary` with the Ghidra provider, then `decompile_all` over `0x1001000–0x101cd00` into `research/decomp/` (git-ignored) and search it with ordinary tools. Function addresses in notes refer to the XP SP3 build.
2. Useful anchors: loader `0x10064e2`; table constructor `0x101aaf3`; ball constructor `0x1019309`; tick `0x1014bf9` → physics step `0x1014a68` → ray step `0x1014956` / grid walk `0x100b486`; bounce `0x1017883`; table acceleration `0x10173e7`; table layer constructor `0x101a5ab`; flippers `0x101c59d` (ctor), `0x101c2ba`, `0x101be06`, `0x101bf98`, `0x101b1f7`; holes `0x1019884`, `0x1016745`, `0x101685b`, `0x10167dd`, `0x1015935`; gates `0x1019204`, `0x1018032`; plunger `0x101a4f3`, `0x10185ad`, `0x101875f`, `0x1017314`; nudge `0x1014dcf`, tilt `0x1017791`; keys `0x1015072`, defaults `0x1005f69`; score add `0x1017526`; rules table walked by `0x100c01d` (88 records at `0x1023ef0`: name descriptor, handler, count, value array); mission dispatcher `0x1011bb5`.
3. Mission system (ported in `rules_game.cpp`; addresses kept for reference): a table-level state `*(DAT_01023bbc+6)` in 0…0x20 dispatches to ~33 handlers (`0x100e657`, `0x100e6d2`, `0x100ebf3`, … `0x1011985`); ~73 component rule handlers (`0x100c44b`–`0x100e0ed`) send numbered messages to components (0x13 on, 0x14 off, 0x15 toggle, 0x22/0x25/0x26/0x27 group queries, 0x2c, 0x30, 0x37 timed eject, 0x3f collision/score, 0x3f3 tilt). Light component (`0x1009ae3`, class 1002, ctor `0x100ad21`): 0 off, 1 on, 4/7 flash, 0xb set bitmap index, 0xc/0xd next/previous. Light group (`0x100a166`, class 1026, ctor `0x100adb1`): members from int 1027, period from float 903, many animation messages (0x18–0x2e). Static `DAT_0102xxxx` pointers in handlers are the cache fields of name descriptors; read the name pointer at `address − 4` in the exe to learn which component each refers to.
4. Ramps (ported in `physics.cpp`: triangles with plane + slope gravity, portal lines for entry/exit, `ramp_hole` drop): class 1021 (`ramp`, `s_ramp9`, ctor `0x1019bb8`), 1022 (`ramp_hole`, ctor `0x1019a1c`); geometry in float attrs 1300–1305; layers 2/4 via int 602.
5. Remaining ideas: multiball, s_ramp9 entry, exact animation timings, drop-target physics. Selftests 14-19 cover ramps, missions, multiplier, ball flow, hyperspace/skill/fuel and a mission smoke run. Lights, light groups and the handlers listed in NOTES.md are done; use `research/tools/annotate.py` to read handlers with component names. Derive behaviour from the code, add a self-test, label anything approximate, and record new facts in `research/NOTES.md`.

## 10. Release checklist

1. Section 6 steps 1–5 pass. 2. Version string updated in `scripts/package.sh`. 3. `git status` shows no game files, no `dist/`, no `research/decomp/`, no personal info (search the tracked files for home-directory paths, usernames and e-mail addresses). 4. The release `.dmg` contains the app, an Applications link, `README.txt`, `LICENSE.txt`, `THIRD-PARTY-NOTICES.txt` (SDL zlib licenses). 5. Report the SHA-256 of the image to the owner. 6. Wait for the owner's instruction before pushing or publishing anything.
