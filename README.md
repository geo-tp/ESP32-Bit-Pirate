# ESP32 Bit Pirate — Weekly Firmware Health

Last update: `2026-10-04T10:06:56Z`

Source: [`8022f80`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/8022f80381bb12bc87c5d4d69bd296db661d6d1c)

Full workflow logs and artifacts: [GitHub Actions run](https://github.com/geo-tp/ESP32-Bit-Pirate/actions/runs/37194305877)

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
| Flash | 3,772,120 B |

## Supported environment builds

| Environment | Status | Static RAM | Flash |
|---|---|---:|---:|
| `custom` | ✅ | 97,540 B | 3,796,983 B |
| `cardputer` | ✅ | 100,188 B | 4,028,487 B |
| `cardputer-adv` | ✅ | 100,188 B | 4,028,451 B |
| `m5stack-sticks3` | ✅ | 101,800 B | 4,013,499 B |
| `s3-devkit` | ✅ | 97,328 B | 3,772,120 B |
| `s3-devkit-n16-r8` | ✅ | 97,780 B | 3,777,346 B |
| `m5stack-stamps3` | ✅ | 100,092 B | 3,915,663 B |
| `atom-lite-s3` | ✅ | 100,092 B | 3,918,239 B |
| `t-display-s3` | ✅ | 98,224 B | 3,942,255 B |
| `waveshare-s3-geek` | ✅ | 97,896 B | 3,933,143 B |
| `t-embed-s3` | ✅ | 97,860 B | 3,930,999 B |
| `t-embed-s3-cc1101` | ✅ | 97,860 B | 3,931,451 B |
| `t-embed-s3-cc1101plus` | ✅ | 97,860 B | 3,931,815 B |
| `xiao-esp32s3` | ✅ | 97,248 B | 3,771,272 B |
| `vision-master-t190` | ✅ | 98,152 B | 3,923,627 B |
| `heltec_wifi_lora_32_V4` | ✅ | 97,332 B | 3,774,998 B |
| `heltec_wifi_lora_32_V3` | ✅ | 97,192 B | 3,765,748 B |
| `um_pros3` | ✅ | 97,412 B | 3,775,534 B |

## Pioarduino framework compatibility

| Build | Arduino | Platform | RAM | Flash |
|---|---|---|---:|---:|
| Current pinned | `3.3.7` | `Espressif 32 (55.3.37) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)` | 97,328 B | 3,772,120 B |
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
| `m5stack/M5Unified` | `^0.2.7` | `0.2.25` | `0.2.25` | `m5stack-stamps3` | ✅ up to date | — | — |
| `mathertel/RotaryEncoder` | `1.5.3` | `1.5.3` | `1.6.0` | `t-embed-s3` | ✅ update builds | +0 B | +40 B |
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

- [`8022f803`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/8022f80381bb12bc87c5d4d69bd296db661d6d1c) group gpio definitions — geo-tp
- [`3fcb280a`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/3fcb280ad325e2f7bd8afae9cea8e633327d10de) Create FUNDING.yml for the support button — Geo
- [`71504db6`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/71504db65a49b6007c6ccc7a50bb4feed8499be0) update contributing guide — geo-tp
- [`b8e52754`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/b8e52754a37136af7c15209db43281e5eb9c171c) add comment for new screen driver — geo-tp
- [`8fd74792`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/8fd747927730b3aee66053452c7a365211383deb) update ili9341 ifdef guard — geo-tp
- [`73505900`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/73505900c60a300d0100614cdd4859fc2ad6a827) Merge pull request #184 from hexanet67/pioarduino — Geo
- [`c7bbeaa6`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/c7bbeaa609ab59dca54a282271f132f9aa2cf43d) removed personal configuration from platformio.ini in order to merge it easily with main branch — Jan Vachun
- [`35b0e2af`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/35b0e2afd783c208869efcd431eb7d3a72acea22) Merge pull request #185 from Moulder-B/fix/openocd-swd-read-timing — Geo
- [`6ff737f0`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/6ff737f05e45e3256ffa51b67c2007794c876a6b) OpenOCD adapter: fix SWD read timing so SWD writes work — Bryce Moulder

## Recent resource history

| Date | Commit | RAM | Flash | Boards | Tests |
|---|---|---:|---:|---:|---|
| 2026-09-26 | `d972aa8` | 97,328 B | 3,772,136 B | 18/18 | ✅ |
| 2026-09-27 | `d972aa8` | 97,328 B | 3,772,136 B | 18/18 | ✅ |
| 2026-10-04 | `8022f80` | 97,328 B | 3,772,120 B | 18/18 | ✅ |

---

This branch is generated automatically. Do not edit it manually.

Static RAM is PlatformIO's compile-time `.data + .bss` measurement; it is not runtime heap consumption after boot.
