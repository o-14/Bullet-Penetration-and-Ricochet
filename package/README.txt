Bullet Penetration and Ricochet 3.0.0
===================================

BPR adds material-aware bullet penetration and ricochets without an ESP or
per-weapon patch requirement.

Requirements
------------
- Fallout 4 1.10.163, 1.11.191, or 1.11.221
- Matching F4SE and Address Library versions
- Mod Configuration Menu is optional

One BPR.dll supports all three listed runtimes.

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

Features
--------
- Caliber-, projectile-, receiver-, material-, and thickness-aware penetration.
- Cumulative damage retention across multiple surfaces.
- Material- and angle-aware ricochets.
- Separate player, NPC, prop, and continuation-limit controls.
- Exact coverage for all 156 base-game and official DLC material records.
- Safe parent, pattern, and general fallback for additional material records.
- Preserves original projectile physics and shooter ownership.
- No permanent save data.

Detailed logging is disabled by default. When enabled, the log is written to:
  Documents/My Games/Fallout4/F4SE/BPR.log

Required license and third-party notices are included with this archive.
