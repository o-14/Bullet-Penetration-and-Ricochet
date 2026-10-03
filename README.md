# Bullet Penetration and Ricochet

BPR 3.1.0 is an ESP-free F4SE plugin that adds material-aware bullet penetration
and ricochets to Fallout 4.

## Requirements

- Fallout 4 `1.10.163`, `1.10.984`, `1.11.191`, `1.11.221`, or `1.11.240`
- Matching F4SE version
- Matching Address Library
- Mod Configuration Menu is optional

One `BPR.dll` supports all five listed runtimes.

## Features

- Caliber-based penetration depth independent of gameplay damage.
- Projectile-profile, receiver-instance, material, and measured-thickness modifiers.
- Cumulative damage retention across multiple surfaces.
- Material- and angle-aware ricochets with configurable chance and direction variation.
- Optional, globally scalable angles for eligible material families, including terrain and water.
- Vegetation permits penetration but does not ricochet.
- Deliberately mild material-specific ricochet loss.
- Separate player and NPC controls, limits, repeat-actor protection, and prop ricochet control.
- Exact coverage for all 156 base-game and official DLC material records.
- Parent and deterministic pattern fallback for Creation Club and mod-added materials.
- Preserves custom projectile physics, weapon instance, ammunition, shooter, and actor cause.
- Compatible with damage overhauls because damage does not determine maximum penetration depth.
- No save forms, ESP dependency, or per-weapon patch requirement.

## Installation

Remove any older BPR installation, then install the release archive with a mod
manager. Do not install multiple BPR versions together.

Settings are stored in:

- `Data/F4SE/Plugins/BPR.ini`
- `Data/F4SE/Plugins/BPR/*.ini`
- `Data/MCM/Settings/BPR.ini` when MCM is used

Additional INI files load alphabetically; later files override earlier files.
Changes made through MCM apply when the Pause menu closes.

Detailed logging is off by default. The material-specific ricochet angle and
loss options are opt-in. Optional Armament, Munitions, and Caliber Complex
patches are available separately; install only the patches for mods you use.

BPR adds no permanent save data. Allow active projectiles to finish before uninstalling.

## Building

Requirements: Visual Studio 2022 with C++23 support, XMake 3.0 or newer, and Git.

```powershell
git submodule update --init --recursive
xmake f -m release
xmake build BPR
```

The dependency commits are pinned by the repository. See
`THIRD_PARTY_NOTICES.md` for licensing details.

## License and credits

BPR — Copyright © 2026 o14

BPR is licensed under GNU GPL version 3 with the additional permissions stated
in the included `EXCEPTIONS` file.

See `LICENSE`,
`EXCEPTIONS`, `COPYRIGHT.md`, `THIRD_PARTY_NOTICES.md`, and `CREDITS.md`.
