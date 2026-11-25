> **Note for readers of this repository:** this file refers to local analysis output (`research/decomp/functions/*.c`, Ghidra pseudocode of the original program) and to a local game folder. The decompiler output is **not published** because it is derived from copyrighted code; regenerate it with REA + Ghidra from your own copy of `pinball.exe`. Function addresses (e.g. `0x1017883`) refer to the Windows XP SP3 build of `pinball.exe` (file version 5.1.2600.5512). Python tools read the game folder from the `PINBALL_DIR` environment variable. Paths starting with `port/` correspond to this repository's root.

# Port notes (derived only from ./Pinball: pinball.exe via REA/Ghidra + data files)

## Data format
See research/tools/partout.py and src/partout.cpp. PINBALL.DAT: 541 groups; last group `table_objects`.

## Table construction (FUN_0101aaf3, 0x101aaf3)
`table_objects` type-10 field = [0x401 header][(class_id, group_index) x 340].
Constructor switch by class id -> component object. Component classes in the table:
1000 x45 | 1001 | 1002 x140 | 1003 | 1004 | 1005 x7 | 1006 x9 | 1007 | 1010 x19 | 1011 | 1012 x2 | 1013 x2
1014 x2 | 1015 x17 | 1016 x9 | 1017 x4 | 1018 x2 | 1019 x13 | 1020 | 1021 x2 | 1022 | 1023 | 1024 x5
1026 x18 | 1028 x2 | 1029 | 1030 | 1031 x30 | 1033 x2
Constructors (ctor addr, sizeof): 1001 0x101a4f3/0x6a; 1002 0x100ad21/0xde; 1003,1004 0x101c59d/0x6a;
1005 0x101944a/0x96; 1006 0x10195e0/0x72; 1007 0x101a4b4/0x5a; 1010 0x10191ba/0x5a; 1011 0x1019677/0x72;
1012 0x1019884/0xea; 1013 0x10196e5/0x66; 1014 0x101983d/0x5e; 1015 0x1019771/0x57; 1016 0x1019204/0x6e;
1017 0x101a416/0x9e; 1018 0x10194b0/0x76; 1019 0x101961f/0x62; 1020 0x10197bb/0x5f; 1021 0x1019bb8/0x9a;
1022 0x1019a1c/0x8e; 1023 0x101a2b5/0x82; 1026 0x100adb1/0x8a; 1028 0x1018ffd/0x36; 1030 0x100ae72/0xa6;
1031 0x101aa18/0x2e; 1032 0x101aa56/0x2e.
Table builder is a TPinballTable-like object (vtable PTR_FUN_01002790); fields +0x32 score, +0x36 ballcount, +0x3a player.

## Decompiled corpus
port/research/decomp/functions/*.c (546 functions, Ghidra pseudocode, address-named).

## Type 12 = per-sprite blob (w,h,...,depth map); type 11 = floats (positions/physics); type 10 = int16 attrs.
Loader error strings (loader_kicker etc.) are unreferenced in the exe, so collision field layouts must come from the component constructors.

## Attribute records (verified 2026-11-24 by overlaying on the table image: research/tools/overlay.py)
Each group may hold several type-10 (int16) and type-11 (float32) records. First value = attribute id, rest = payload.
- Group's type-0 value = component kind (200 visual, 300 edge/material, 400 other, 0x190 etc.).
- int 100: state count; 1025(0x401): table_objects; 1027, 1500, 1100, 602, 304, 300, 400, 5: refs/misc (not yet decoded).
- float 600 / 603: collision geometry: [id, count, then count x (x,y), tail]. count==1 => circle [id,1,x,y,radius,tail];
  tail is -1.0 or another number (meaning TBD). 600 = walls (143 records), 603 = rollover polygons (18). 601 = sink circle [x,y,r,-1].
  Coordinates are table units (about +-12 x, +-12 y); z=0 plane.
- float 301: [elasticity, 302, smoothness]; 305/306/408 kicker: [force?], [..], [x,y,z,..]; 401/402: id/value chains (flipper/kicker params).
- float 900/901: x140 (class 1002 lights): [0.1] each (timing). 903: [0.2]. 904 fuel_bargraph [3,14,3,12,...].
- Camera (group 'camera_info', 15 floats): 3x4 matrix M = f[0..11], focal F = f[12] (-400.0). The projection centre is OVERRIDDEN by the table's float attr 700 = (183, 238) (see correction below).
  Projection: p' = M*(x,y,z,1); screen = (183,238) + F * (x'/z', y'/z') in table-picture coordinates; the table picture is drawn at (0,0).

## Packaging (scripts/package.sh -> dist/3D Pinball.app)
- Bundle = port executable + libSDL2 (Homebrew sdl2-compat) + libSDL3 + game files copied from ../Pinball into Contents/Resources (all except pinball.exe). Ad-hoc signed.
- sdl2-compat dlopens a file named exactly `libSDL3.dylib` next to itself (@loader_path); `libSDL3.0.dylib` is NOT found -> "Failed loading SDL3 library". Fixed by installing as libSDL3.dylib.
- App finds data via SDL_GetBasePath()/../Resources; `--debug` shows collision geometry (F1 toggles), `--shot file.bmp` saves a screenshot and exits.
- Verified: runs from an unrelated cwd, loads only bundled libs. Not yet verified: Finder double-click (Gatekeeper right-click > Open may be needed).

## Open question: screen layout
Table bitmap is at (137,2) 365x470 and sprites (flippers etc.) share that space, but the left/right info panel bitmap is at (386,12) 203x394 and overlaps the table's right part. Real layout unresolved; check the renderer (FUN_01004870 / FUN_01013705 callers).
pinball/table.bmp is a labelled component diagram (55 numbered components) - handy for naming classes.

## Layout of this folder (cleanup)
port/src (C++17 sources), port/scripts/package.sh (creates build/ on demand), port/dist/3D Pinball.app (built app),
port/research/{tools (Python parser/renderer/overlay), decomp (546 Ghidra pseudocode files), NOTES.md}.
`build/` is disposable. Run the Python tools with `PYTHONPATH=research/tools python3 ...` (not -I).

## Physics prototype (src/physics.*) - APPROXIMATE, not yet the original's code
Geometry is real data (attr 600 walls/circles, 800/801/802 flipper pivot+tip circles; wall material via int attr 300 -> group float 301).
Tuning constants are guesses: gravity 9.0, flipper swing 0.08s, plunger speed 20..33, restitution default 0.55, flipper 0.45.
Not solid: group `drain`, `s_trip*` (sensors). One-way gates: `s_onewy*` (pass while vy<0). Not modelled yet: v_* parts, ramps, kickers, a_roll, bumper kicks.
Original engine (found, not yet ported): ray-cast + grid broad-phase FUN_0100b486, flipper ray test FUN_0101b825,
key handler FUN_01015072, table ctor FUN_0101aaf3, ball ctor FUN_01019309 (group `ball`: attr 500 = radius 0.3).
Flipper attrs (a_flip1/2): 800 pivot [x,y,r]; 801/802 tip circle at the two swing ends; 803..805 unknown (0.9, 0.04, 0.08).
Self-test: `3DPinball --selftest` (drop reaches drain; plunger shot reaches y=-10.9).
Known layout bug: the info panel (386,12) is clipped under the table (137,2); real arrangement unresolved.

## Original physics, decoded (verified by reading the code; ported in src/physics.cpp)
- Tick FUN_01014bf9: dt = min(elapsed_ms,100)/1000 -> FUN_01014a68 steps every ball: accel from grid fields, v += a*dt, then up to 10 ray-cast moves (FUN_01014956 -> grid walk FUN_0100b486).
- Max ball speed = ball radius * 200 = 60 (set in FUN_01015424). Ball radius 0.3 (group `ball`, attr 500).
- Table layer FUN_01017xxx / ctor FUN_0101a5ab: table attr 305 = [25.0, 0.5, 1.5708] -> gravity g=(25*sin(0.5)*cos(pi/2), 25*sin(0.5)*sin(pi/2)) = (0, 11.9856). Attr 701 = 0.2 drag c.
  Acceleration (FUN_010173e7): ax = gx - (noise + dirx)*speed*c, ay = gy - diry*speed*c, noise = 0.5 - rand()/32768 (Windows rand is 0..32767; macOS rand() is 31-bit, so MASK IT with &0x7fff).
  Terminal speed = gy/c = 59.9 ~= max speed 60 (consistency check).
- Bounce FUN_01017883(ball, pos, n, elast=p4, smooth=p5, thresh=p6, kick=p7): u=-(n.d); if u>0: d=((u n + d)*smooth + u n*elast) normalised; fn=u*speed;
  speed -= (1-elast)*fn; if thresh<=fn: d = n*kick + speed*d, speed=|d|.
- Material group (type0==300): attr 301 = smoothness, 302 = elasticity (defaults 0.95 / 0.6). Kicker group (type0==400) chain: 401 = threshold (default 9e10), 402 = kick, 404 = 3 values, 405, 406 sound.
- Component int attrs (type-10) are FLAT CHAINS of (id,value) pairs (100 state count, 300 material ref, 400 kicker ref, 304 sound, 602 collision-group bit, 1100/1101 bitmap refs, 1500 = 9 shorts). Use Group::intAttr(id).
- Examples: pop bumpers (a_bump*) kick 12..17 at threshold 2.5; outer table walls kick 4 at threshold 5.
- Not yet ported: ray-cast continuous collision, flipper motion (FUN_0101b825 family; params 800-805), kickouts (a_kout*, attr 305=eject speed, 306=hold time?), sinks (v_sink*, 601), ramps, gates, targets, lights, scoring.
- Known prototype limits: a ball can rest in a pocket at (-4.92, 7.35) in the drop self-test (probably a gate/ramp/kickout region that the original handles).

## Flipper, decoded (ported in src/physics.cpp; ctor FUN_0101c59d -> motion class FUN_0101c2ba)
- Group a_flip1/2 attrs: 800 pivot [x,y,r]; 801 tip A = REST position; 802 tip B = FLIPPED position; 803 = hit strength (0.9);
  804 = time to flip UP (0.04 s); 805 = time to fall back (0.08 s). Material via int 300 (smoothness 0.5, elasticity 0.75 here).
- Swing angle A = angle between pivot->801 and pivot->802 (shortest way; the raw atan2 values wrap at +-pi: 2.531 vs -2.536 -> A = 1.216 rad. Using the long way was a bug).
- Motion is linear in time (FUN_0101b1f7): state 0 idle, 1 rising, 2 falling; duration = |a1-a0|/A * T. Collision radii are inflated by the ball radius (table +0x11a).
- Hit (FUN_0101be06): if the flipper is moving and the contact is outside the pivot radius: kick = dot(surface direction, normal) * (A/T) * (r/R) * 0.9, r = contact distance from pivot, R = flipper reach incl. radii;
  the response is FUN_01017883 with threshold -1 (always kick). A resting flipper just bounces with its material.
- Decompile range note: game code extends to ~0x101cd00; decomp now covers 0x1001000-0x101cd00 (566 functions).
- Self-tests (`3DPinball --selftest`): drop drains; plunger shot reaches y~-12.6; resting ball is thrown up at ~18 u/s by a flip.
- Rollover groups (a_roll*) carry their 600 polygon as a SENSOR, not a wall (they trapped the ball when treated as solid).

## Layout SOLVED (supersedes the "known layout bug" notes above)
Sprites and the table are positioned relative to the table bitmap origin (137,2) (FUN_0101a5ab stores it at table+0xea/+0xee; FUN_01017c6d subtracts it). So the table is drawn at screen (0,0) and the camera centre is (300-137, 208-2); the info panel is blitted directly at (386,12) and all number boxes (score1/ballcount1/player_number1) lie inside it.
Number boxes: int record [first digit group, x, y, w, h]; digits 0-9 are bitmaps in consecutive groups; drawn right-aligned (FUN_01013d2d).

## Scoring (src/rules_data.h from research/tools/rules_table.py)
FUN_0100c01d walks an 88-record table at 0x1023ef0 (16 bytes each: descriptor{char* name,cache}, handler, count, int* values). `values` = per-state points (a_bump1..4 500..2000, a_bump5..7 1500.., v_rebo 500, a_targ 500..1000, a_roll 500..20000, a_kout1 50000, a_kout3 20000, a_kout2 10000.., v_sink 2500..7500, ...).
Score add FUN_01017526 = table multiplier * points + bonus, capped; hits send message 0x3f to the rules handler (0x100c4f5 etc.). Port uses state 0 and no multipliers/missions yet.
Rollover groups a_roll* carry a closed 600 polygon = SENSOR (score when the ball enters; sound = int attr 304).

## Holes (kick-outs a_kout*, sinks v_sink*)  (FUN_01019884 ctor, FUN_0101685b field, FUN_01016745 capture, FUN_010168e3 msg, FUN_010167dd eject, FUN_01015935 throw)
- a_kout: 600 circle [x,y,r]; attr 305 = pull force, 306*r = capture radius; inside r the ball accelerates toward the centre by unit(c-p)*force - v (gravity well).
- Capture snaps the ball to the centre, plays material sound (304), sends 0x3f (score); eject after 1.5 s default (timer 0x37): direction = kicker record 404 triple rotated by random +-405 rad, speed = 402 * (1 +- 403%), eject sound = 406.
- v_sink: 601 [x,y,r] = capture radius; same eject scheme. ramp_hole not ported (needs ramps). a_kout circles are NOT solid bumpers (they were wrongly solid before).

## Sound (src/audio.*)
Sound groups: type0 == 0xca + type9 'soundN.wav' (8-bit mono 11025 Hz). Impact sound: material 304 (fn between 0.2 and threshold) else kicker 406 (FUN_01017e85). Plunger release = plunger int 406.

## Self-tests: `3DPinball --selftest` (7 checks: drop, plunger, flipper, audio, bumper score 500, rollover 2000, gravity well). `--demo --shot f.bmp` renders a sample HUD with flippers raised.
## Still not ported: ramps/ramp_hole, gates (v_gate*), blockers (v_bloc1), targets' drop animation, lights (lite*) and mission/bonus logic, multipliers, extra balls, tilt, high-score table, music (MIDI), plunger/lane details, multiball. Continuous ray-cast is not needed at 1 ms steps (max 0.06 units/step < radius 0.3).

## One-way gates, flags, layers, holes (final physics details)
- s_onewy* (class 1016, FUN_01019204): two TLine edges per gate: A = p0->p1 offset +r along its normal n=(dy,-dx) (FUN_01008beb), B = reversed, offset -0.8r (i.e. +0.8r on A's side). Edge test FUN_01008c8f only collides when dot(ray,n)<0 (approaching the front).
  FUN_01018032: hit on B = pass through (sound int 304 + message 0x3f = score); hit on A = normal bounce. => ball passes moving along +n, is blocked coming back. Ported as: skip collision unless side>0 && v.n<0; pass sensor at +0.8r (one direction only).
- a_flag* (class 1018, spinners): 600 record is a 2-point SENSOR line, not a wall (scores 500 / 2500 by state, sound int 304).
- Collision layers (int attr 602): layer bit = 1<<value (default 1). Ball starts on layer 1; parts on layer 2/4 (ramp, ramp_hole, a_bump5-7, a_roll110-112, group 103/106/306-309/326-328, s_onewy12) are skipped until ramps are ported.
- Holes: capture is a collision with a small circle EDGE, so it only triggers when the ball enters from outside: a hole re-arms only after the ball has left its capture radius (ejecting a ball at 0.35 from the centre inside a 0.37 capture radius otherwise re-captured it instantly).
- a_kout1 (the central gravity well) is class 1029 = constructed INACTIVE (FUN_01019884 param_3==0 clears the active flag); a mission rule enables it. a_kout2/3 (class 1012) and v_sink* (class 1017) are active.
- Slingshots a_kick*: kicker record has 402 (kick) but no 401 threshold; port fires on any hit above 0.5 (approximation).

## Verification tools (built into the app binary)
`3DPinball --selftest` (8 checks incl. MIDI), `--soak <seconds>` (4 seeded simulated players; env SOAK_SEED/SOAK_TRACE/SOAK_EVERY), `PROBE_X/PROBE_Y` (contacts at a point), `EJECT=<hole>` (trace an ejection), `--demo --shot f.bmp`, `--debug` (F1 overlay).
Soak result at final build: 3000 s x 4 seeds = 75 complete games, 0 stuck events, 0 NaN, best score ~294k.

## Music
PINBALL.MID plays in a loop through macOS's built-in MIDI synth (src/music.cpp, AudioToolbox MusicPlayer); M toggles. Controls: Z / Left, / or Right = flippers, Space = plunger (hold to charge), F2 = new game, P or F3 = pause, F1 = collision overlay, Esc = quit.
App icon: extracted from the exe's own icon resource (research/tools/make_icon.py -> assets/AppIcon.icns).

## Plunger, start position, nudge/tilt, keys (decoded; ported)
- Plunger ctor FUN_0101a4f3 (class 1001): attr 601 [x,y,r] = BALL START POSITION (-7.021, 10.085), stored at table+0x11e/+0x122 (the ball is live from the start and settles onto the plunger face at y=11.975).
  Pull (FUN_01017314, timer every 0.025 s): pull += step, step = ftol(100.0 / (frameCount * 8.0)) = 1 with 8 frames, max 100 -> 2.5 s to full pull. Sprite frame = floor((frames-1)*pull/100).
  Release: face collision (FUN_0101875f) = bounce(elast 0.5, smooth 0.5) with kick = pull*(1+0.1*rand), threshold 0; next physics step caps speed at 60. Port: speed = min(60, pull*(1+0.1r)) if the ball rests on the plunger.
- Default keys (FUN_01005f69, registry names "Left Flippper key" etc.): Left flipper Z (0x5a), right flipper / (0xbf), plunger Space (0x20), left table bump X (0x58), right table bump . (0xbe), bottom table bump Up (0x26). Defaults also: Sounds=1, Music=0 (off), Players=1.
- Nudge FUN_01014dcf(dx,dy): left (2,1), right (-2,1), bottom (0,1); every live ball gets v += (dx*0.5, dy*0.5); picture shift (floor(dx+0.5), floor(0.5-dy)) px; 0.4 s later the opposite (-dx,-dy) is applied (timers 0x1014f20 etc.).
  Tilt meter (tick FUN_01014bf9): +4*dt while a nudge flag is active, -dt otherwise; >0.5 = warning, >1.0 = tilt (FUN_01017791: tilt sound = table int 406, flippers dead until the ball is lost). Port skips the warning text box.
- Mission system (not ported): the table has a mission state machine, *(DAT_01023bbc+6) in 0..0x20, dispatching to 33 handlers (FUN_0100e657, 0x100e6d2, 0x100ebf3 ... 0x1011985) via FUN_01011bb5; 73 component rule handlers (0x100c44b..0x100e0ed) send messages (0x13,0x14,0x22,0x25,0x27,0x2c,0x30,0x37,0x3f...) to components. Light component messages (FUN_01009ae3): 0 off, 1 on, 4/7 flash, 0xb set bitmap index, 0xc/0xd next/prev, 0xe reset. This is the remaining big piece (lights, missions, bonuses, multipliers).


## CORRECTION: projection centre and ball placement (supersedes earlier camera notes)
- FUN_01015424 first calls FUN_01008e4b(matrix, F, 300, 208), but FUN_0101a5ab (table layer ctor) then calls FUN_01008eba(attr700[0], attr700[1]) which REPLACES the centre with the table's float attr 700 = (183, 238). The earlier derived centre (300-137, 208-2) = (163, 206) was wrong by about (-20, -32) px: the collision overlay and the ball sprite were drawn offset from the artwork. With (183,238): left flipper pivot (2.489,12.063) -> (132.6,384), inside its sprite at (124..167, 376..412); plunger lane centre -> x=324; bumper holes line up with their collision circles.
- The ball centre is at height z = +radius (0.3) above the table (the ball's reference points in group float 501 use z = 0.3; z' = ... - 0.9135*z, so positive z is toward the camera). Earlier ports used -radius.
- The ball has one bitmap per depth: groups `ball` + the following six (9,10,11,12,13,14,15 px). Float 501 = [x, y, z] reference point of each; the port picks the sprite whose reference y is nearest to the ball's y.
- Visual check tool: `pinball --ballat X,Y --debug --shot out.bmp` freezes the ball at a table position with the collision overlay.

## v0.2.0: sprites, depth, lights and rules
- Component sprites: each component's own group plus the following unnamed bitmap groups are its frames (bumper caps, targets, gates, spinner flags, lane plates, lights). Initial visibility follows each class's constructor: classes 1005 (bumper), 1006, 1013, 1014, 1015, 1018, 1019 show frame 0; the rest start hidden until a rule shows them. None of these bitmaps are part of the table picture, which is why they were missing.
- Depth map (entry type 12): u16 w,h,stride + 8 header bytes + h rows of `stride` u16 (0xffff = none; orientation auto-detected). depth = (z' - 19.5013) * 4303.97 with z' the camera-space depth; smaller = nearer. Checked on a_bump1 (map 28472-33093, computed 28781 at its location). The ball (centre z = +radius) is redrawn behind any sprite pixel that is nearer.
- Hit effects: bumper cap frames (state*2+1 for 0.1 s), spinner flag frame 1 for 0.15 s, target/rollover flashes 0.12-0.2 s. Frame timings are approximations.
- Lights: class 1002 messages (handler 0x1009ae3): 0 off, 1 on, 4/7 blink (value = seconds), 8/9 force dark/lit for `value` seconds (end callback 0x1009a2a resets the display), 0xe stop blinking, 0x13 on+reset, 0x14 off+reset, 0x15 toggle. 0xf (used by target banks, value 2.0) is modelled as "switch on, blink for the value"; the exact original is not confirmed. Blink timing from float attrs 900/901.
- Light groups (class 1026, handler 0x100a166, members = int 1027): codes below 0x18 are broadcast; 0x18/0x19 rotate the lit pattern (refused while a member blinks); 0x25 lit count; 0x26 size; 0x2d progress level; 0x10 modelled as blink-then-off. The many chase animations (0x1a-0x20) are not ported.
- Ported rule handlers: top lanes a_roll1-3 -> lite10/9/8 (0x100c939): all three lit -> blink 5 s, attack_bumpers level +1 (max 3), 60 s decay timer (0x100cabf); flippers rotate bmpr_inc_lights / ramp_bmpr_inc_lights (0x100d53a/0x100d572); target banks a_targ10-12, 13-15, 16-18, 19-21 -> lite70-72, 101-103, 104-106, 107-109 (0x100d8de, 0x100d9c2, 0x100da94, 0x100db68); rebounds v_rebo1/2 flash lite84/85. Bumper level selects the score value state and the cap frame pair.
- Not ported: a_targ1-9 banks, skill-shot / trek / sink / hyperspace lights, ramps (classes 1021/1022), the 33-state mission machine (0x1011bb5), bonus and multiplier rules, info text box messages.
- Tools: research/tools/dat_names.py (maps DAT_0102xxxx descriptor addresses to component names), annotate.py (rewrites decompiled handlers with those names), both need the local decompiler output; their JSON outputs are git-ignored.



## v0.3.0: ramps, rules and missions
- Parts carry a layer mask = OR of every (602,v) entry (`Group::layerMask`); ball layer 1 table, 2 on the ramp, 4 raised platform. Ramp rails (group 103) are 6 = both upper layers.
- Ramp (class 1021, ctor 0x1019bb8): float 1300 = count + 13-float records [plane A,B,C, 3 vertices, slope tilt a, direction b, derived gx, gy]; slope gravity = 25*(sin a cos b, sin a sin b), drag 701, no noise. 1301 = entry portal, 1302 = exit portal ([layer, enabled, ?, x0,y0,x1,y1, z]), 1303 = scoring line on the platform. Crossing the entry line from layer 1 puts the ball on the ramp, the exit line moves it to layer 4; crossing back reverses. Platform height = plane height at the exit (about 1.06). `s_ramp9` has its entry disabled in the data.
- ramp_hole (0x1018476/0x10169c0): ball on layer 4 inside the circle is held, drops to the table at rest after about 0.5 s, then message 0x3a (lamp lite54 blinks).
- Rules: every scoring component hit calls `Rules::hit`; handlers in `rules_game.cpp` mirror 0x100c44b..0x100e0ed, 0x101240d, 0x1012570, 0x1011f1a, 0x10126ba; the mission machine mirrors 0x1011bb5 and its 33 state handlers (state = mission id 2..18, sub-states 0x14-0x1f, 0x20 game over). Mission choice: rank (middle_circle lit count) x ramp-bank target hit picks the mission; the ramp accepts it.
- Tables: field multipliers 1,2,3,5,10 (0x1024758); mission accept award 10000 (ids 2-5), 20000 (6-14), 30000 (15-18).
- Text ids come from the exe's string table; this project writes its own wording.
