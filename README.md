# ESP32 Bit Pirate — Weekly Firmware Health

Last update: `2026-09-27T08:05:59Z`

Source: [`d972aa8`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/d972aa830e3bfb11021b6dc751063a8d8c859236)

Full workflow logs and artifacts: [GitHub Actions run](https://github.com/geo-tp/ESP32-Bit-Pirate/actions/runs/36305122054)

## Overall status

✅ **Current Bit Pirate firmware is healthy.**

- Boards: **18/18** build successfully
- Native tests: **✅ passed**
- Latest pioarduino: **⚠️ build incompatible**
- Direct library updates available: **3**
- Library update build regressions: **1**

## Current reference build

| Item | Value |
|---|---:|
| Environment | `s3-devkit` |
| Arduino framework | `3.3.7` |
| Platform | `Espressif 32 (55.3.37) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)` |
| Static RAM | 97,328 B |
| Flash | 3,772,136 B |

## Supported environment builds

| Environment | Status | Static RAM | Flash |
|---|---|---:|---:|
| `custom` | ✅ | 97,540 B | 3,796,563 B |
| `cardputer` | ✅ | 100,180 B | 4,026,363 B |
| `cardputer-adv` | ✅ | 100,180 B | 4,026,363 B |
| `m5stack-sticks3` | ✅ | 101,792 B | 4,011,547 B |
| `s3-devkit` | ✅ | 97,328 B | 3,772,136 B |
| `s3-devkit-n16-r8` | ✅ | 97,780 B | 3,777,362 B |
| `m5stack-stamps3` | ✅ | 100,084 B | 3,913,827 B |
| `atom-lite-s3` | ✅ | 100,084 B | 3,916,399 B |
| `t-display-s3` | ✅ | 98,216 B | 3,941,659 B |
| `waveshare-s3-geek` | ✅ | 97,888 B | 3,932,059 B |
| `t-embed-s3` | ✅ | 97,852 B | 3,929,883 B |
| `t-embed-s3-cc1101` | ✅ | 97,852 B | 3,930,351 B |
| `t-embed-s3-cc1101plus` | ✅ | 97,852 B | 3,930,715 B |
| `xiao-esp32s3` | ✅ | 97,248 B | 3,771,284 B |
| `vision-master-t190` | ✅ | 98,144 B | 3,922,495 B |
| `heltec_wifi_lora_32_V4` | ✅ | 97,332 B | 3,775,010 B |
| `heltec_wifi_lora_32_V3` | ✅ | 97,192 B | 3,765,764 B |
| `um_pros3` | ✅ | 97,412 B | 3,775,546 B |

## Pioarduino framework compatibility

| Build | Arduino | Platform | RAM | Flash |
|---|---|---|---:|---:|
| Current pinned | `3.3.7` | `Espressif 32 (55.3.37) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)` | 97,328 B | 3,772,136 B |
| Latest stable | `3.3.12` | `Espressif 32 (55.3.312) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)` | n/a | n/a |

⚠️ The currently pinned framework builds, but latest stable pioarduino does not.

<details>
<summary>Latest framework build errors</summary>

```text
/home/runner/work/_temp/pio-health-latest/packages/framework-arduinoespressif32/libraries/USB/src/USBMSCFS.h:26:10: fatal error: FS.h: No such file or directory
*** [.pio/build/s3-devkit/libda6/USB/USBHostMSC.cpp.o] Error 1
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
| `ewpa/LibSSH-ESP32` | `^5.6.0` | `5.9.0` | `5.9.0` | `s3-devkit` | ✅ up to date | — | — |
| `fastled/FastLED` | `3.10.3` | `3.10.3` | `3.10.5` | `s3-devkit` | ⚠️ update fails to build | — | — |
| `gilman88/XModem` | `^1.0.3` | `1.0.3` | `1.0.3` | `s3-devkit` | ✅ up to date | — | — |
| `hideakitai/ESP32SPISlave` | `^0.6.8` | `0.6.9` | `0.8.0` | `s3-devkit` | ✅ update builds | +0 B | +0 B |
| `m5stack/M5Unified` | `^0.2.7` | `0.2.23` | `0.2.23` | `m5stack-stamps3` | ✅ up to date | — | — |
| `mathertel/RotaryEncoder` | `1.5.3` | `1.5.3` | `1.6.0` | `t-embed-s3` | ✅ update builds | +0 B | +64 B |
| `miq19/eModbus` | `^1.7.4` | `1.7.4` | `1.7.4` | `s3-devkit` | ✅ up to date | — | — |
| `paulstoffregen/OneWire` | `^2.3.8` | `2.3.8` | `2.3.8` | `s3-devkit` | ✅ up to date | — | — |
| `pstolarz/OneWireNg` | `^0.14.0` | `0.14.1` | `0.14.1` | `s3-devkit` | ✅ up to date | — | — |
| `sparkfun/SparkFun External EEPROM Arduino Library` | `^3.2.10` | `3.2.13` | `3.2.13` | `s3-devkit` | ✅ up to date | — | — |
| `throwtheswitch/Unity` | `^2.6.1` | `2.6.1` | `2.6.1` | `native-tests` | ✅ up to date | — | — |

### Dependency compatibility regressions

#### `fastled/FastLED`

`3.10.3` → `3.10.5`

<details>
<summary>Build errors</summary>

```text
src/Services/LedService.cpp:31:11: error: call of overloaded 'memset(CRGB*&, int, unsigned int)' is ambiguous
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
FastLED        3.10.3     3.10.3    3.10.5    Library  custom
FastLED        3.10.3     3.10.3    3.10.5    Library  cardputer
FastLED        3.10.3     3.10.3    3.10.5    Library  cardputer-adv
FastLED        3.10.3     3.10.3    3.10.5    Library  m5stack-sticks3
FastLED        3.10.3     3.10.3    3.10.5    Library  s3-devkit
FastLED        3.10.3     3.10.3    3.10.5    Library  s3-devkit-n16-r8
FastLED        3.10.3     3.10.3    3.10.5    Library  m5stack-stamps3
FastLED        3.10.3     3.10.3    3.10.5    Library  atom-lite-s3
FastLED        3.10.3     3.10.3    3.10.5    Library  t-display-s3
FastLED        3.10.3     3.10.3    3.10.5    Library  waveshare-s3-geek
FastLED        3.10.3     3.10.3    3.10.5    Library  t-embed-s3
FastLED        3.10.3     3.10.3    3.10.5    Library  t-embed-s3-cc1101
FastLED        3.10.3     3.10.3    3.10.5    Library  t-embed-s3-cc1101plus
FastLED        3.10.3     3.10.3    3.10.5    Library  xiao-esp32s3
FastLED        3.10.3     3.10.3    3.10.5    Library  vision-master-t190
FastLED        3.10.3     3.10.3    3.10.5    Library  heltec_wifi_lora_32_V4
FastLED        3.10.3     3.10.3    3.10.5    Library  heltec_wifi_lora_32_V3
FastLED        3.10.3     3.10.3    3.10.5    Library  um_pros3
RotaryEncoder  1.5.3      1.5.3     1.6.0     Library  t-embed-s3
RotaryEncoder  1.5.3      1.5.3     1.6.0     Library  t-embed-s3-cc1101
RotaryEncoder  1.5.3      1.5.3     1.6.0     Library  t-embed-s3-cc1101plus
```

</details>

## Native tests

✅ Native test suite passed.

## Development activity — last 7 days

- [`d972aa83`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/d972aa830e3bfb11021b6dc751063a8d8c859236) change pioarduino framework workflow to a weekly firmware health workflow — Geo
- [`ba092426`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/ba092426b71be3e60809904d22fa15b54462bbea) Rename firmware-resource-delta to firmware-resource-delta.yml — Geo
- [`3e0cf717`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/3e0cf7172e262cf6d5fee0d9ab79fccfd799bfbc) add firmware resource delta comparison workflow — Geo
- [`1257c26d`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/1257c26d00c133627df5e9ef9b0d18533cf22003) add pioarduino ram comparison workflow — Geo
- [`5b9cb68d`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/5b9cb68decb5dcdafbf7f841d774ccbc05e5fc4e) rename upstream firmware delivery workflow — Geo
- [`08fd1f64`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/08fd1f64fbad17454cf17d78a64428c3d29d5e7a) Rename ci.yml to native-tests.yml — Geo
- [`d65a5bb4`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/d65a5bb4d520705d34f2efc0e5eddfec00a2fa20) define fastLED version related to #180 — geo-tp
- [`8ddef434`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/8ddef4343ee286d7f0c10a45edf7d7d44ec48e63) Merge pull request #181 from Hecatron-Forks/pioarduino — Geo
- [`dc345040`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/dc3450402a5280298d95f42d668ad2ba0f263cfb) fix jtag scanner and improve swd scanner related to #179 — geo-tp
- [`c37dd33a`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/c37dd33a9dd9a1e01ee07fbbaa913a63f1fbf10a) Added note about onboard led — grbd
- [`81cfd69b`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/81cfd69b3f61236049ca760a6c2c83fbf09cef53) removed todo statements — grbd
- [`a740ccef`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/a740ccefe76cc4828973b406dc6cc275dd3f19d8) Initial ProS3 support — grbd

## Recent resource history

| Date | Commit | RAM | Flash | Boards | Tests |
|---|---|---:|---:|---:|---|
| 2026-09-26 | `d972aa8` | 97,328 B | 3,772,136 B | 18/18 | ✅ |
| 2026-09-27 | `d972aa8` | 97,328 B | 3,772,136 B | 18/18 | ✅ |

---

This branch is generated automatically. Do not edit it manually.

Static RAM is PlatformIO's compile-time `.data + .bss` measurement; it is not runtime heap consumption after boot.
