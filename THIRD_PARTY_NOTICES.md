# Third-party notices

## Penetration System

BPR's Fallout 4 projectile-interoperability work was informed by and, where
stated in the source, adapted from Penetration System by jarari under the MIT
License.

- Source: <https://github.com/jarari/PenetrationSystem>
- Reviewed commit: `0f694649b0140fe737d2243e94376a2b3d88778a`
- Copyright: Copyright (c) 2025 jarari
- License: `licenses/PenetrationSystem-MIT.txt`

## CommonLibF4

BPR uses Dear Modding FO4's multi-runtime CommonLibF4 fork.

- Source: <https://github.com/Dear-Modding-FO4/commonlibf4>
- Pinned commit: `20b2727ed42455afbe8bf23ab1066b238335d2b5`
- Copyright: Copyright (c) 2019 ryan-rsm-mckenzie
- Root license: MIT

Its commonlib-shared dependency is pinned to
`f0b1670ee9caac2e349497f6f3c08a69633a8ea7` and is licensed under GPLv3 with
the Modding Exception and additional linking exception.

## spdlog

CommonLibF4/commonlib-shared links spdlog 1.16.0 for logging.

- Source: <https://github.com/gabime/spdlog/tree/v1.16.0>
- Copyright: Copyright (c) 2016 Gabi Melman
- License: `licenses/spdlog-MIT.txt`
- Build configuration: compiled library using the C++ standard formatting library

Binary packages include the applicable MIT, GPLv3, and exception texts.
