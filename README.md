# Bind of Poke

A top-down, room-based action roguelike written in C17, using SDL2 only as its
platform layer. The private learning prototype stars Pokémon and focuses on
procedural floors, real-time combat, and highly interactive upgrade builds.

The reference properties are used only as design inspiration. Do not add
copyrighted game assets or copied source material. A public release must replace
Pokémon branding and characters with original content.

## Development status

### Milestone 1 — Foundation: Complete

- Modular platform, input, game, and renderer boundaries
- Fixed 60 Hz simulation with bounded catch-up
- Resizable window with a fixed 960×540 logical canvas
- Correct mouse-to-game coordinate conversion
- Pause and clean shutdown behavior
- Pure-C gameplay tests available through `make test`

## Requirements

- A C17 compiler
- GNU Make
- SDL2 and `sdl2-config`

On macOS with Homebrew:

```sh
brew install sdl2
```

## Build and run

```sh
make
make run
```

Press Escape or close the window to exit.

Prototype controls:

- Move with WASD or the arrow keys.
- Shoot with IJKL or hold the left mouse button and aim with the mouse.
- Press P to pause or resume.
- Press Escape to quit.

For a debug build:

```sh
make debug
```

Run the pure-C gameplay tests without opening a window:

```sh
make test
```
