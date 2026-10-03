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
- Pinned commit: `2aaefd104754b59b16e435b2859b299bb68dd8a2`
- Copyright: Copyright (c) 2019 ryan-rsm-mckenzie
- Root license at the pinned commit: GPLv3 with the included Modding Exception
  and additional linking exception (`lib/commonlibf4/LICENSE` and `EXCEPTIONS`).

Its commonlib-shared dependency is pinned to
`e30b310a19621ff9f635cf2c456fe633559c1c24` and is licensed under GPLv3 with
the Modding Exception and additional linking exception.

The CommonLibF4 source tree also pins DearModdingUI-API commit
`9ddb9a8dacef8c5a116fabd3fe3a453cc446f830` as a header-only build dependency.
BPR does not include or call the DearModdingUI API.

## spdlog

CommonLibF4/commonlib-shared links spdlog 1.16.0 for logging.

- Source: <https://github.com/gabime/spdlog/tree/v1.16.0>
- Copyright: Copyright (c) 2016 Gabi Melman
- License: `licenses/spdlog-MIT.txt`
- Build configuration: compiled library using the C++ standard formatting library

Binary packages include the applicable MIT, GPLv3, and exception texts.
