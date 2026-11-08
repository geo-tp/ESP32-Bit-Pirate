# ESP32 Bit Pirate — Weekly Firmware Health

Last update: `2026-11-08T08:04:05Z`

Source: [`8f42d9f`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/8f42d9f40fc86f97b6b1247d039a3cd899c5d442)

Full workflow logs and artifacts: [GitHub Actions run](https://github.com/geo-tp/ESP32-Bit-Pirate/actions/runs/42461312234)

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
| Flash | 3,805,059 B |

## Supported environment builds

| Environment | Status | Static RAM | Flash |
|---|---|---:|---:|
| `custom` | ✅ | 88,780 B | 3,829,827 B |
| `cardputer` | ✅ | 91,732 B | 4,093,735 B |
| `cardputer-adv` | ✅ | 91,732 B | 4,094,359 B |
| `m5stack-sticks3` | ✅ | 93,312 B | 4,076,067 B |
| `s3-devkit` | ✅ | 88,540 B | 3,805,059 B |
| `s3-devkit-n16-r8` | ✅ | 89,012 B | 3,810,297 B |
| `s3-supermini` | ✅ | 88,628 B | 3,808,625 B |
| `m5stack-stamps3` | ✅ | 91,628 B | 3,979,975 B |
| `atom-lite-s3` | ✅ | 91,628 B | 3,982,551 B |
| `t-display-s3` | ✅ | 89,464 B | 3,976,255 B |
| `waveshare-s3-geek` | ✅ | 89,136 B | 3,968,855 B |
| `t-embed-s3` | ✅ | 89,100 B | 3,966,643 B |
| `t-embed-s3-cc1101` | ✅ | 89,100 B | 3,967,283 B |
| `t-embed-s3-cc1101plus` | ✅ | 89,100 B | 3,967,627 B |
| `xiao-esp32s3` | ✅ | 88,460 B | 3,804,079 B |
| `vision-master-t190` | ✅ | 89,392 B | 3,959,819 B |
| `heltec_wifi_lora_32_V4` | ✅ | 88,548 B | 3,807,805 B |
| `heltec_wifi_lora_32_V3` | ✅ | 88,404 B | 3,799,031 B |
| `um_pros3` | ✅ | 88,628 B | 3,808,137 B |

## Pioarduino framework compatibility

| Build | Arduino | Platform | RAM | Flash |
|---|---|---|---:|---:|
| Current pinned | `3.3.7` | `Espressif 32 (55.3.37) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)` | 88,540 B | 3,805,059 B |
| Latest stable | `unknown` | `unknown` | n/a | n/a |

⚠️ The currently pinned framework builds, but latest stable pioarduino does not.

<details>
<summary>Latest framework build errors</summary>

```text
Package download failed: Got the unrecognized status code '504' when downloaded https://github.com/espressif/arduino-esp32/releases/download/3.3.12/esp32-core-3.3.12.tar.xz. Retrying in 16s... (attempt 4/5)
Could not install framework-arduinoespressif32 for patching: Got the unrecognized status code '504' when downloaded https://github.com/espressif/arduino-esp32/releases/download/3.3.12/esp32-core-3.3.12.tar.xz
Tool Manager: Installing https://github.com/espressif/arduino-esp32/releases/download/3.3.12/esp32-core-3.3.12.tar.xz
Package download failed: Got the unrecognized status code '504' when downloaded https://github.com/espressif/arduino-esp32/releases/download/3.3.12/esp32-core-3.3.12.tar.xz. Retrying in 2s... (attempt 1/5)
Package download failed: Got the unrecognized status code '504' when downloaded https://github.com/espressif/arduino-esp32/releases/download/3.3.12/esp32-core-3.3.12.tar.xz. Retrying in 4s... (attempt 2/5)
Package download failed: Got the unrecognized status code '504' when downloaded https://github.com/espressif/arduino-esp32/releases/download/3.3.12/esp32-core-3.3.12.tar.xz. Retrying in 8s... (attempt 3/5)
Package download failed: Got the unrecognized status code '504' when downloaded https://github.com/espressif/arduino-esp32/releases/download/3.3.12/esp32-core-3.3.12.tar.xz. Retrying in 16s... (attempt 4/5)
PackageException: Got the unrecognized status code '504' when downloaded https://github.com/espressif/arduino-esp32/releases/download/3.3.12/esp32-core-3.3.12.tar.xz
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
| `fastled/FastLED` | `3.10.3` | `3.10.3` | `3.10.9` | `s3-devkit` | ⚠️ update fails to build | — | — |
| `gilman88/XModem` | `^1.0.3` | `1.0.3` | `1.0.3` | `s3-devkit` | ✅ up to date | — | — |
| `hideakitai/ESP32SPISlave` | `^0.6.8` | `0.6.9` | `0.8.0` | `s3-devkit` | ✅ update builds | +0 B | +0 B |
| `m5stack/M5Unified` | `^0.2.7` | `0.2.29` | `0.3.0` | `m5stack-stamps3` | ✅ update builds | +8 B | +1,480 B |
| `mathertel/RotaryEncoder` | `1.5.3` | `1.5.3` | `1.6.0` | `t-embed-s3` | ✅ update builds | +0 B | +52 B |
| `miq19/eModbus` | `^1.7.4` | `1.7.4` | `1.7.4` | `s3-devkit` | ✅ up to date | — | — |
| `paulstoffregen/OneWire` | `^2.3.8` | `2.3.8` | `2.3.8` | `s3-devkit` | ✅ up to date | — | — |
| `pstolarz/OneWireNg` | `^0.14.0` | `0.14.1` | `0.14.1` | `s3-devkit` | ✅ up to date | — | — |
| `sparkfun/SparkFun External EEPROM Arduino Library` | `^3.2.10` | `3.2.13` | `3.2.13` | `s3-devkit` | ✅ up to date | — | — |
| `throwtheswitch/Unity` | `^2.6.1` | `2.6.1` | `2.6.1` | `native-tests` | ✅ up to date | — | — |

### Dependency compatibility regressions

#### `fastled/FastLED`

`3.10.3` → `3.10.9`

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
FastLED        3.10.3     3.10.3    3.10.9    Library  custom
FastLED        3.10.3     3.10.3    3.10.9    Library  cardputer
FastLED        3.10.3     3.10.3    3.10.9    Library  cardputer-adv
FastLED        3.10.3     3.10.3    3.10.9    Library  m5stack-sticks3
FastLED        3.10.3     3.10.3    3.10.9    Library  s3-devkit
FastLED        3.10.3     3.10.3    3.10.9    Library  s3-devkit-n16-r8
FastLED        3.10.3     3.10.3    3.10.9    Library  s3-supermini
FastLED        3.10.3     3.10.3    3.10.9    Library  m5stack-stamps3
FastLED        3.10.3     3.10.3    3.10.9    Library  atom-lite-s3
FastLED        3.10.3     3.10.3    3.10.9    Library  t-display-s3
FastLED        3.10.3     3.10.3    3.10.9    Library  waveshare-s3-geek
FastLED        3.10.3     3.10.3    3.10.9    Library  t-embed-s3
FastLED        3.10.3     3.10.3    3.10.9    Library  t-embed-s3-cc1101
FastLED        3.10.3     3.10.3    3.10.9    Library  t-embed-s3-cc1101plus
FastLED        3.10.3     3.10.3    3.10.9    Library  xiao-esp32s3
FastLED        3.10.3     3.10.3    3.10.9    Library  vision-master-t190
FastLED        3.10.3     3.10.3    3.10.9    Library  heltec_wifi_lora_32_V4
FastLED        3.10.3     3.10.3    3.10.9    Library  heltec_wifi_lora_32_V3
FastLED        3.10.3     3.10.3    3.10.9    Library  um_pros3
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

No commits during the last 7 days.

## Recent resource history

| Date | Commit | RAM | Flash | Boards | Tests |
|---|---|---:|---:|---:|---|
| 2026-09-26 | `d972aa8` | 97,328 B | 3,772,136 B | 18/18 | ✅ |
| 2026-09-27 | `d972aa8` | 97,328 B | 3,772,136 B | 18/18 | ✅ |
| 2026-10-04 | `8022f80` | 97,328 B | 3,772,120 B | 18/18 | ✅ |
| 2026-10-11 | `8ebef51` | 88,540 B | 3,800,651 B | 19/19 | ✅ |
| 2026-10-18 | `8a91778` | 88,540 B | 3,804,351 B | 19/19 | ✅ |
| 2026-10-25 | `8a91778` | 88,540 B | 3,804,351 B | 19/19 | ✅ |
| 2026-11-01 | `8f42d9f` | 88,540 B | 3,805,059 B | 19/19 | ✅ |
| 2026-11-08 | `8f42d9f` | 88,540 B | 3,805,059 B | 19/19 | ✅ |

---

This branch is generated automatically. Do not edit it manually.

Static RAM is PlatformIO's compile-time `.data + .bss` measurement; it is not runtime heap consumption after boot.
