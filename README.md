<h1 align="center">3D-Pinball-macos</h1>

<p align="center">
  A native macOS port of <i>3D Pinball for Windows – Space Cadet</i>,<br>
  rebuilt from scratch in C++17 and SDL2 for Apple Silicon.
</p>

<p align="center">
  <img alt="Platform" src="https://img.shields.io/badge/platform-macOS%2012%2B%20(Apple%20Silicon)-lightgrey">
  <img alt="Language" src="https://img.shields.io/badge/C%2B%2B-17-blue">
  <img alt="License" src="https://img.shields.io/badge/license-MIT-green">
</p>

> [!IMPORTANT]
> **This repository does not contain the game.** You need your own copy of the original game files (`PINBALL.DAT`, `FONT.DAT`, the `.WAV` and `.MID` files). The port loads them at run time.

> [!NOTE]
> The contents of this project, including the code, tools and documentation were written entirely by Claude Sonnet 5.5 using [Reverse Engineer Anything](https://github.com/morluto/rea) with [Ghidra](https://github.com/nationalsecurityagency/ghidra).

## Features

- Loader for the original data file; the table, scoring values, sounds and music come from your copy of the game
- Physics constants and formulas recovered from the original: gravity, drag, bounce and kick, flippers, plunger, bumpers, slingshots, gates, spinners, rollovers, kick-outs
- Table components drawn as in the original: bumper caps, gates, spinner flags, lane plates and lamps, with the ball passing behind them; hit effects
- Lamp engine and part of the table rules: top lanes and bumper upgrades, flipper lane rotation, target banks
- Nudging and tilt, scoring, WAV effects, MIDI music, full screen, auto-pause, a short how-to-play alert

**Implemented:** the left ramp and the raised platform with its ramp hole, all 17 missions and the final maelstrom chain, ranks and fuel, field multipliers, bonus, jackpot lamps, extra balls and ball save, the skill shot, the hyperspace chute sequence, text messages, tilt warning and a top-five high-score table (F5 shows it; you type a name after a qualifying game).

**Approximations:** the right-hand thin ramp (`s_ramp9`) cannot be entered, multiball (ball locks) is replaced by an extra ball, drop targets are not animated, and light chase animations are simplified. The game has been verified with automated tests only, not by a person playing it.

## Install

1. Download `3D-Pinball-macos-<version>.dmg` from Releases and drag the app to *Applications*.
2. Start it once; it shows where your game files belong: `~/Library/Application Support/3D-Pinball-macos`.
3. Copy the game files from your own copy of the Windows game into that folder and start the app again.

The app is ad-hoc signed, not notarized. If macOS blocks it, right-click the app and choose **Open**, or run `xattr -dr com.apple.quarantine /Applications/3D-Pinball-macos.app`.

## Build from source

Requires macOS 12+ on Apple Silicon, Xcode command line tools, CMake and Homebrew.

```sh
brew install cmake sdl2-compat sdl3
./scripts/package.sh --release     # dist/3D-Pinball-macos-<version>.dmg (contains no game data)
./scripts/package.sh ./Pinball     # personal build with your game files bundled in; never share it
```

## Controls

| Key | Action |
|---|---|
| <kbd>Z</kbd> / <kbd>←</kbd> , <kbd>/</kbd> / <kbd>→</kbd> | Left / right flipper |
| <kbd>Space</kbd> | Plunger: hold to pull back, release to launch |
| <kbd>X</kbd> / <kbd>.</kbd> / <kbd>↑</kbd> | Nudge left / right / from below (too much tilts the table) |
| <kbd>F2</kbd> · <kbd>P</kbd> · <kbd>F4</kbd> · <kbd>M</kbd> · <kbd>H</kbd> · <kbd>Esc</kbd> | New game · pause · full screen · music (off by default) · help · quit |

## Technical notes

How the data format, physics and scoring were reconstructed is documented in [`research/NOTES.md`](research/NOTES.md).

## Legal notice

No game assets are included or distributed. The game's own files credit **Cinematronics** (© 1994, 1995); the Windows release was distributed by **Microsoft**, and the table artwork carries **Maxis** branding. All of that belongs to its owners and must come from your own legitimate copy. This project is not affiliated with or endorsed by any of them; "3D Pinball" and "Space Cadet" are used only to describe compatibility. The code is a new implementation that reproduces constants, formulas and file layouts observed in the original program, analysed from decompiled output rather than in a clean-room process. This notice is not legal advice.

## License

[MIT](LICENSE). It covers only the original code and documentation here, never the game's data, art, sound or music.

## Acknowledgements

[Claude](https://www.anthropic.com/claude) and Claude Code · REA (Reverse Engineer Anything) · [Ghidra](https://ghidra-sre.org/) (Apache-2.0) · [SDL](https://libsdl.org/) (zlib license)

If you are an AI agent, refer to [AGENTS.md](AGENTS.md).