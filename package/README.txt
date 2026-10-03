Bullet Penetration and Ricochet 3.1.0
===================================

BPR adds material-aware bullet penetration and ricochets without an ESP or
per-weapon patch requirement.

Requirements
------------
- Fallout 4 1.10.163, 1.10.984, 1.11.191, 1.11.221, or 1.11.240
- Matching F4SE and Address Library versions
- Mod Configuration Menu is optional

One BPR.dll supports all five listed runtimes.

Installation
------------
Remove any older BPR installation, then install this archive with a mod
manager. Do not install multiple BPR versions together.

Configuration
-------------
Main settings:
  Data/F4SE/Plugins/BPR.ini

Material, ammunition, and optional compatibility layers:
  Data/F4SE/Plugins/BPR/*.ini

Layer files load alphabetically. Later filenames override earlier files. MCM
changes apply when the Pause menu closes; no save reload is required.

Material-specific ricochet angles and loss are optional and off by default.
Armament, Munitions, and Caliber Complex patches are separate downloads.
Install only the patches for ammunition mods you use.

Features
--------
- Caliber-, projectile-, receiver-, material-, and thickness-aware penetration.
- Cumulative damage retention across multiple surfaces.
- Material- and angle-aware ricochets.
- Optional, scalable angles for eligible material families, including terrain and water.
- Vegetation permits penetration but does not ricochet.
- Small material-specific ricochet loss.
- Separate player, NPC, prop, and continuation-limit controls.
- Exact coverage for all 156 base-game and official DLC material records.
- Safe parent, pattern, and general fallback for additional material records.
- Preserves original projectile physics and shooter ownership.
- No permanent save data.

Detailed logging is disabled by default. When enabled, the log is written to:
  Documents/My Games/Fallout4/F4SE/BPR.log

Version 3.1.0
--------------------
- Fine-tunes vanilla and supported mod ammunition penetration.
- Adds optional material-specific ricochet angles and mild damage/power loss.
- Disables vegetation ricochets without changing vegetation penetration.
- Adds Fallout 4 1.10.984 support through the existing NG runtime path.
- Widens the opt-in material-specific ricochet windows while preserving distinct families.
- Allows very shallow ballistic ricochets from water and other soft surfaces.
- Scales every material angle with the player's global Ricochet Angle setting.
- Keeps the global ricochet chance universal across every material.

Required license and third-party notices are included with this archive.
