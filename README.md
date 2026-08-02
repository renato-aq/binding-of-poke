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

### Milestone 2 — Combat Room: Complete

- Yellow player with independent movement and aiming
- Player and hostile projectiles with centralized damage rules
- Chaser and Spitter enemy behaviors
- Fixed-capacity entity pools with generation-checked handles
- Solid room bounds, authored walls, and collision layers
- Doors locked during combat and opened when the room is cleared
- Health, damage invulnerability, death, and combat restart flow

### Milestone 3 — Procedural Floor: Complete

- Deterministic ten-room floors generated from a reusable seed
- Connected start, combat, reward, shop, secret, and boss rooms
- Three authored room templates selected by the generator
- Door-based room transitions and persistent clear state
- Minimap discovery and room-type markers
- Automatic reachability and door validation with a fallback layout
- Generation regression coverage across more than 100 seeds

### Milestone 4 — Run Economy and Builds: Complete

- Persistent coins, keys, bombs, health, and room-clear rewards
- Deterministic reward, shop, secret, and item pools
- Purchasable shop offers and keyed reward choices
- Damage, attack-rate, movement-speed, piercing, and health upgrades
- Charged active item powered by room clears
- Bomb explosions that damage enemies and reveal secret rooms
- Exact build statistics and nearby pickup descriptions in the window title

### Milestone 5 — Boss and Complete Run: Complete

- Dedicated Containment Guardian with spread, telegraphed charge, and summon states
- Faster second phase with additional radial projectiles
- Boss health bar, contact damage, and interaction with shots, bombs, and active items
- Victory and game-over states with immediate restart
- Successful runs advance to the next seed
- Versioned, atomic save data for completed runs and the boss-defeated unlock
- Persistent win count displayed alongside the current build

### Milestone 6 — Stabilization: Complete

- Keyboard, mouse, and hot-pluggable SDL2 gamepad support
- Reduced-flash accessibility setting persisted with schema migration
- Isolated release and debug build artifacts
- AddressSanitizer and UndefinedBehaviorSanitizer verification
- Ten-thousand-seed floor stress testing and repeated-run lifecycle testing
- One-command full verification through `make check`

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

To replay a particular floor seed:

```sh
./build/release/bind-of-poke 424242
```

Press Escape or close the window to exit.

Prototype controls:

- Move with WASD or the arrow keys.
- Shoot with IJKL or hold the left mouse button and aim with the mouse.
- Press P to pause or resume.
- Press R after dying to restart the combat room.
- Press E to buy or unlock a nearby reward.
- Press B to place a bomb.
- Press Space to use a fully charged active item.
- Press F to toggle reduced flashes.
- Press Escape to quit.

Gamepad controls:

- Move with the left stick and aim/shoot with the right stick.
- Press A to interact, X to place a bomb, and Y to use the active item.
- Press Start to pause and B to restart after a completed run.

For a debug build:

```sh
make debug
```

Run the pure-C gameplay tests without opening a window:

```sh
make test
```

Run every release, debug, sanitizer, and stress check:

```sh
make check
```
