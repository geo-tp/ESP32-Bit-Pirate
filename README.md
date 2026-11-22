# ESP32 Bit Pirate — Weekly Firmware Health

Last update: `2026-11-22T08:03:45Z`

Source: [`58df0f1`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/58df0f11bae8360af768011d61cb8b3bea858c78)

Full workflow logs and artifacts: [GitHub Actions run](https://github.com/geo-tp/ESP32-Bit-Pirate/actions/runs/45001021250)

## Overall status

✅ **Current Bit Pirate firmware is healthy.**

- Boards: **19/19** build successfully
- Native tests: **✅ passed**
- Latest pioarduino: **⚠️ build incompatible**
- Direct library updates available: **4**
- Library update build regressions: **1**

## Current reference build

| Item | Value |
|---|---:|
| Environment | `s3-devkit` |
| Arduino framework | `3.3.7` |
| Platform | `Espressif 32 (55.3.37) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)` |
| Static RAM | 88,540 B |
| Flash | 3,832,183 B |

## Supported environment builds

| Environment | Status | Static RAM | Flash |
|---|---|---:|---:|
| `custom` | ✅ | 88,780 B | 3,857,707 B |
| `cardputer` | ✅ | 91,732 B | 4,120,811 B |
| `cardputer-adv` | ✅ | 91,732 B | 4,121,839 B |
| `m5stack-sticks3` | ✅ | 93,312 B | 4,101,395 B |
| `s3-devkit` | ✅ | 88,540 B | 3,832,183 B |
| `s3-devkit-n16-r8` | ✅ | 89,012 B | 3,837,409 B |
| `s3-supermini` | ✅ | 88,628 B | 3,835,745 B |
| `m5stack-stamps3` | ✅ | 91,628 B | 4,007,319 B |
| `atom-lite-s3` | ✅ | 91,628 B | 4,009,915 B |
| `t-display-s3` | ✅ | 89,464 B | 4,003,579 B |
| `waveshare-s3-geek` | ✅ | 89,136 B | 3,996,323 B |
| `t-embed-s3` | ✅ | 89,100 B | 3,994,139 B |
| `t-embed-s3-cc1101` | ✅ | 89,100 B | 3,994,711 B |
| `t-embed-s3-cc1101plus` | ✅ | 89,100 B | 3,995,047 B |
| `xiao-esp32s3` | ✅ | 88,460 B | 3,831,199 B |
| `vision-master-t190` | ✅ | 89,392 B | 3,987,299 B |
| `heltec_wifi_lora_32_V4` | ✅ | 88,548 B | 3,834,921 B |
| `heltec_wifi_lora_32_V3` | ✅ | 88,404 B | 3,825,691 B |
| `um_pros3` | ✅ | 88,628 B | 3,835,489 B |

## Pioarduino framework compatibility

| Build | Arduino | Platform | RAM | Flash |
|---|---|---|---:|---:|
| Current pinned | `3.3.7` | `Espressif 32 (55.3.37) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)` | 88,540 B | 3,832,183 B |
| Latest stable | `4.0.0` | `Espressif 32 (61.4.0) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)` | n/a | n/a |

⚠️ The currently pinned framework builds, but latest stable pioarduino does not.

<details>
<summary>Latest framework build errors</summary>

```text
.pio/libdeps/s3-devkit/FastLED/src/platforms/esp/32/audio/devices/idf5_i2s_context.hpp:121:21: error: 'i2s_port_t' does not name a type; did you mean 'i2s_role_t'?
*** [.pio/build/s3-devkit/lib36e/FastLED/fl/audio_input.cpp.o] Error 1
```

</details>

## Direct dependency health

Latest versions are tested individually against the currently pinned framework.

| Library | Declared | Resolved | Latest | Test environment | Compatibility | RAM Δ | Flash Δ |
|---|---:|---:|---:|---|---|---:|---:|
| `adafruit/Adafruit Si4713 Library` | `^1.2.3` | `1.2.4` | `1.2.4` | `s3-devkit` | ✅ up to date | — | — |
| `autowp/autowp-mcp2515` | `^1.2.1` | `1.3.1` | `1.3.1` | `s3-devkit` | ✅ up to date | — | — |
| `bblanchon/ArduinoJson` | `^7.3.0` | `7.4.3` | `7.4.3` | `s3-devkit` | ✅ up to date | — | — |
| `crankyoldgit/IRremoteESP8266` | `^2.9.0` | `2.9.0` | `2.9.0` | `s3-devkit` | ✅ up to date | — | — |
| `ewpa/LibSSH-ESP32` | `^5.6.0` | `5.10.0` | `5.10.0` | `s3-devkit` | ✅ up to date | — | — |
| `fastled/FastLED` | `3.10.3` | `3.10.3` | `3.10.10` | `s3-devkit` | ⚠️ update fails to build | — | — |
| `gilman88/XModem` | `^1.0.3` | `1.0.3` | `1.0.3` | `s3-devkit` | ✅ up to date | — | — |
| `hideakitai/ESP32SPISlave` | `^0.6.8` | `0.6.9` | `0.8.0` | `s3-devkit` | ✅ update builds | +0 B | +0 B |
| `m5stack/M5Unified` | `^0.2.7` | `0.2.29` | `0.3.0` | `m5stack-stamps3` | ✅ update builds | +8 B | +1,576 B |
| `mathertel/RotaryEncoder` | `1.5.3` | `1.5.3` | `1.6.0` | `t-embed-s3` | ✅ update builds | +0 B | +52 B |
| `miq19/eModbus` | `^1.7.4` | `1.7.4` | `1.7.4` | `s3-devkit` | ✅ up to date | — | — |
| `paulstoffregen/OneWire` | `^2.3.8` | `2.3.8` | `2.3.8` | `s3-devkit` | ✅ up to date | — | — |
| `pstolarz/OneWireNg` | `^0.14.0` | `0.14.1` | `0.14.1` | `s3-devkit` | ✅ up to date | — | — |
| `sparkfun/SparkFun External EEPROM Arduino Library` | `^3.2.10` | `3.2.13` | `3.2.13` | `s3-devkit` | ✅ up to date | — | — |
| `throwtheswitch/Unity` | `^2.6.1` | `2.6.1` | `2.6.1` | `native-tests` | ✅ up to date | — | — |

### Dependency compatibility regressions

#### `fastled/FastLED`

`3.10.3` → `3.10.10`

<details>
<summary>Build errors</summary>

```text
src/Services/LedService.cpp:19:9: error: 'delay' was not declared in this scope; did you mean 'fl::delay'?
src/Services/LedService.cpp:217:13: error: 'delay' was not declared in this scope; did you mean 'fl::delay'?
src/Services/LedService.cpp:227:13: error: 'delay' was not declared in this scope; did you mean 'fl::delay'?
src/Services/LedService.cpp:234:13: error: 'delay' was not declared in this scope; did you mean 'fl::delay'?
src/Services/LedService.cpp:240:13: error: 'delay' was not declared in this scope; did you mean 'fl::delay'?
src/Services/LedService.cpp:249:13: error: 'delay' was not declared in this scope; did you mean 'fl::delay'?
*** [.pio/build/s3-devkit/src/Services/LedService.cpp.o] Error 1
```

</details>


## Git-based dependencies

| Source | Environment | Pinning |
|---|---|---|
| `https://github.com/lovyan03/LovyanGFX` | `custom` | ⚠️ unpinned HEAD |
| `https://github.com/m5stack/M5Cardputer` | `cardputer` | ⚠️ unpinned HEAD |
| `https://github.com/m5stack/M5Cardputer` | `cardputer-adv` | ⚠️ unpinned HEAD |
| `https://github.com/m5stack/M5Unified.git` | `m5stack-sticks3` | ⚠️ unpinned HEAD |
| `https://github.com/lovyan03/LovyanGFX` | `t-display-s3` | ⚠️ unpinned HEAD |
| `https://github.com/lovyan03/LovyanGFX` | `waveshare-s3-geek` | ⚠️ unpinned HEAD |
| `https://github.com/lovyan03/LovyanGFX` | `t-embed-s3` | ⚠️ unpinned HEAD |
| `https://github.com/lovyan03/LovyanGFX` | `t-embed-s3-cc1101` | ⚠️ unpinned HEAD |
| `https://github.com/lovyan03/LovyanGFX` | `vision-master-t190` | ⚠️ unpinned HEAD |

## PlatformIO package update report

<details>
<summary>Current / Wanted / Latest</summary>

```text
Checking

Semantic Versioning color legend:
<Major Update>  backward-incompatible updates
<Minor Update>  backward-compatible features
<Patch Update>  backward-compatible bug fixes

Package        Current    Wanted    Latest    Type     Environments
-------------  ---------  --------  --------  -------  ----------------------
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  custom
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  cardputer
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  cardputer-adv
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  m5stack-sticks3
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  s3-devkit
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  s3-devkit-n16-r8
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  s3-supermini
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  m5stack-stamps3
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  atom-lite-s3
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  t-display-s3
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  waveshare-s3-geek
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  t-embed-s3
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  t-embed-s3-cc1101
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  t-embed-s3-cc1101plus
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  xiao-esp32s3
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  vision-master-t190
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  heltec_wifi_lora_32_V4
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  heltec_wifi_lora_32_V3
ESP32SPISlave  0.6.9      0.6.9     0.8.0     Library  um_pros3
FastLED        3.10.3     3.10.3    3.10.10   Library  custom
FastLED        3.10.3     3.10.3    3.10.10   Library  cardputer
FastLED        3.10.3     3.10.3    3.10.10   Library  cardputer-adv
FastLED        3.10.3     3.10.3    3.10.10   Library  m5stack-sticks3
FastLED        3.10.3     3.10.3    3.10.10   Library  s3-devkit
FastLED        3.10.3     3.10.3    3.10.10   Library  s3-devkit-n16-r8
FastLED        3.10.3     3.10.3    3.10.10   Library  s3-supermini
FastLED        3.10.3     3.10.3    3.10.10   Library  m5stack-stamps3
FastLED        3.10.3     3.10.3    3.10.10   Library  atom-lite-s3
FastLED        3.10.3     3.10.3    3.10.10   Library  t-display-s3
FastLED        3.10.3     3.10.3    3.10.10   Library  waveshare-s3-geek
FastLED        3.10.3     3.10.3    3.10.10   Library  t-embed-s3
FastLED        3.10.3     3.10.3    3.10.10   Library  t-embed-s3-cc1101
FastLED        3.10.3     3.10.3    3.10.10   Library  t-embed-s3-cc1101plus
FastLED        3.10.3     3.10.3    3.10.10   Library  xiao-esp32s3
FastLED        3.10.3     3.10.3    3.10.10   Library  vision-master-t190
FastLED        3.10.3     3.10.3    3.10.10   Library  heltec_wifi_lora_32_V4
FastLED        3.10.3     3.10.3    3.10.10   Library  heltec_wifi_lora_32_V3
FastLED        3.10.3     3.10.3    3.10.10   Library  um_pros3
M5Unified      0.2.29     0.2.29    0.3.0     Library  m5stack-stamps3
M5Unified      0.2.29     0.2.29    0.3.0     Library  atom-lite-s3
RotaryEncoder  1.5.3      1.5.3     1.6.0     Library  t-embed-s3
RotaryEncoder  1.5.3      1.5.3     1.6.0     Library  t-embed-s3-cc1101
RotaryEncoder  1.5.3      1.5.3     1.6.0     Library  t-embed-s3-cc1101plus
```

</details>

## Native tests

✅ Native test suite passed.

## Development activity — last 7 days

- [`58df0f11`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/58df0f11bae8360af768011d61cb8b3bea858c78) forward improvements from ultimate remote to the infrared mode — geo-tp
- [`9cf2efc8`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/9cf2efc8b96171eacb740a31bcf74a47daa90bec) change to no screen view by default for s3devkit — geo-tp
- [`7cc03a6c`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/7cc03a6cd79a915750c7843c8715121b62ed3aa1) remove old rgb view files — geo-tp
- [`2a5bd64f`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/2a5bd64f853ff6f4970acf7df93c1438675fda49) rename rgb led device view — geo-tp
- [`3a82e0b7`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/3a82e0b7823bf17cec852ed4f67acf0b3e0cd006) add missing uart sniffing test file — geo-tp
- [`73b51271`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/73b51271c83f9f15961df46870db88428b6012ec) Merge pull request #200 from fdufnews/pioarduino — Geo
- [`3fb00040`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/3fb00040e823a9a612fde8a9eb01481cf24f5ec5) change wifi config to a dedicated hostname — geo-tp
- [`fbab9804`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/fbab9804a3cd979ef733f7a2c03e3db57b1e29dd) Merge pull request #171 from vcarriere/hostname-change — Geo
- [`d5a16774`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/d5a16774dcd8c2f3aa55280bf085b5c468caddb2) Support of M5Stack Cap Lora-1262 — fdufnews

## Recent resource history

| Date | Commit | RAM | Flash | Boards | Tests |
|---|---|---:|---:|---:|---|
| 2026-10-04 | `8022f80` | 97,328 B | 3,772,120 B | 18/18 | ✅ |
| 2026-10-11 | `8ebef51` | 88,540 B | 3,800,651 B | 19/19 | ✅ |
| 2026-10-18 | `8a91778` | 88,540 B | 3,804,351 B | 19/19 | ✅ |
| 2026-10-25 | `8a91778` | 88,540 B | 3,804,351 B | 19/19 | ✅ |
| 2026-11-01 | `8f42d9f` | 88,540 B | 3,805,059 B | 19/19 | ✅ |
| 2026-11-08 | `8f42d9f` | 88,540 B | 3,805,059 B | 19/19 | ✅ |
| 2026-11-15 | `8f42d9f` | 88,540 B | 3,807,775 B | 19/19 | ✅ |
| 2026-11-22 | `58df0f1` | 88,540 B | 3,832,183 B | 19/19 | ✅ |

---

This branch is generated automatically. Do not edit it manually.

Static RAM is PlatformIO's compile-time `.data + .bss` measurement; it is not runtime heap consumption after boot.
