# ESP32 Bit Pirate — Weekly Firmware Health

Last update: `2026-10-11T08:06:16Z`

Source: [`8ebef51`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/8ebef51652aa89950c6d7e7ffdcf508a96748266)

Full workflow logs and artifacts: [GitHub Actions run](https://github.com/geo-tp/ESP32-Bit-Pirate/actions/runs/38123624917)

## Overall status

✅ **Current Bit Pirate firmware is healthy.**

- Boards: **19/19** build successfully
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
| Static RAM | 88,540 B |
| Flash | 3,800,651 B |

## Supported environment builds

| Environment | Status | Static RAM | Flash |
|---|---|---:|---:|
| `custom` | ✅ | 88,772 B | 3,825,503 B |
| `cardputer` | ✅ | 91,420 B | 4,057,207 B |
| `cardputer-adv` | ✅ | 91,420 B | 4,057,763 B |
| `m5stack-sticks3` | ✅ | 93,000 B | 4,038,823 B |
| `s3-devkit` | ✅ | 88,540 B | 3,800,651 B |
| `s3-devkit-n16-r8` | ✅ | 89,012 B | 3,805,877 B |
| `s3-supermini` | ✅ | 88,628 B | 3,804,225 B |
| `m5stack-stamps3` | ✅ | 91,324 B | 3,944,271 B |
| `atom-lite-s3` | ✅ | 91,324 B | 3,946,815 B |
| `t-display-s3` | ✅ | 89,456 B | 3,970,747 B |
| `waveshare-s3-geek` | ✅ | 89,128 B | 3,961,775 B |
| `t-embed-s3` | ✅ | 89,092 B | 3,959,515 B |
| `t-embed-s3-cc1101` | ✅ | 89,092 B | 3,960,131 B |
| `t-embed-s3-cc1101plus` | ✅ | 89,092 B | 3,960,475 B |
| `xiao-esp32s3` | ✅ | 88,460 B | 3,799,775 B |
| `vision-master-t190` | ✅ | 89,384 B | 3,952,639 B |
| `heltec_wifi_lora_32_V4` | ✅ | 88,548 B | 3,803,485 B |
| `heltec_wifi_lora_32_V3` | ✅ | 88,404 B | 3,794,251 B |
| `um_pros3` | ✅ | 88,628 B | 3,804,045 B |

## Pioarduino framework compatibility

| Build | Arduino | Platform | RAM | Flash |
|---|---|---|---:|---:|
| Current pinned | `3.3.7` | `Espressif 32 (55.3.37) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)` | 88,540 B | 3,800,651 B |
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
| `ewpa/LibSSH-ESP32` | `^5.6.0` | `5.10.0` | `5.10.0` | `s3-devkit` | ✅ up to date | — | — |
| `fastled/FastLED` | `3.10.3` | `3.10.3` | `3.10.6` | `s3-devkit` | ⚠️ update fails to build | — | — |
| `gilman88/XModem` | `^1.0.3` | `1.0.3` | `1.0.3` | `s3-devkit` | ✅ up to date | — | — |
| `hideakitai/ESP32SPISlave` | `^0.6.8` | `0.6.9` | `0.8.0` | `s3-devkit` | ✅ update builds | +0 B | +0 B |
| `m5stack/M5Unified` | `^0.2.7` | `0.2.25` | `0.2.25` | `m5stack-stamps3` | ✅ up to date | — | — |
| `mathertel/RotaryEncoder` | `1.5.3` | `1.5.3` | `1.6.0` | `t-embed-s3` | ✅ update builds | +0 B | +56 B |
| `miq19/eModbus` | `^1.7.4` | `1.7.4` | `1.7.4` | `s3-devkit` | ✅ up to date | — | — |
| `paulstoffregen/OneWire` | `^2.3.8` | `2.3.8` | `2.3.8` | `s3-devkit` | ✅ up to date | — | — |
| `pstolarz/OneWireNg` | `^0.14.0` | `0.14.1` | `0.14.1` | `s3-devkit` | ✅ up to date | — | — |
| `sparkfun/SparkFun External EEPROM Arduino Library` | `^3.2.10` | `3.2.13` | `3.2.13` | `s3-devkit` | ✅ up to date | — | — |
| `throwtheswitch/Unity` | `^2.6.1` | `2.6.1` | `2.6.1` | `native-tests` | ✅ up to date | — | — |

### Dependency compatibility regressions

#### `fastled/FastLED`

`3.10.3` → `3.10.6`

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
FastLED        3.10.3     3.10.3    3.10.6    Library  custom
FastLED        3.10.3     3.10.3    3.10.6    Library  cardputer
FastLED        3.10.3     3.10.3    3.10.6    Library  cardputer-adv
FastLED        3.10.3     3.10.3    3.10.6    Library  m5stack-sticks3
FastLED        3.10.3     3.10.3    3.10.6    Library  s3-devkit
FastLED        3.10.3     3.10.3    3.10.6    Library  s3-devkit-n16-r8
FastLED        3.10.3     3.10.3    3.10.6    Library  s3-supermini
FastLED        3.10.3     3.10.3    3.10.6    Library  m5stack-stamps3
FastLED        3.10.3     3.10.3    3.10.6    Library  atom-lite-s3
FastLED        3.10.3     3.10.3    3.10.6    Library  t-display-s3
FastLED        3.10.3     3.10.3    3.10.6    Library  waveshare-s3-geek
FastLED        3.10.3     3.10.3    3.10.6    Library  t-embed-s3
FastLED        3.10.3     3.10.3    3.10.6    Library  t-embed-s3-cc1101
FastLED        3.10.3     3.10.3    3.10.6    Library  t-embed-s3-cc1101plus
FastLED        3.10.3     3.10.3    3.10.6    Library  xiao-esp32s3
FastLED        3.10.3     3.10.3    3.10.6    Library  vision-master-t190
FastLED        3.10.3     3.10.3    3.10.6    Library  heltec_wifi_lora_32_V4
FastLED        3.10.3     3.10.3    3.10.6    Library  heltec_wifi_lora_32_V3
FastLED        3.10.3     3.10.3    3.10.6    Library  um_pros3
RotaryEncoder  1.5.3      1.5.3     1.6.0     Library  t-embed-s3
RotaryEncoder  1.5.3      1.5.3     1.6.0     Library  t-embed-s3-cc1101
RotaryEncoder  1.5.3      1.5.3     1.6.0     Library  t-embed-s3-cc1101plus
```

</details>

## Native tests

✅ Native test suite passed.

## Development activity — last 7 days

- [`8ebef516`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/8ebef51652aa89950c6d7e7ffdcf508a96748266) add info for i2c freq — geo-tp
- [`9e8bb066`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/9e8bb066657a56b71d28693827e44614bb8e4fec) udpate gemini assitant prompt — geo-tp
- [`efa9e053`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/efa9e053a321928d00893779df0a9a401b22aefd) change version to 1.8 — geo-tp
- [`359611ed`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/359611ed0d4f6fbd0a910a0f137f67322ea668c2) fix I2C mode to use the configured frequency for every cmds — geo-tp
- [`02c5db2d`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/02c5db2dcdef491f715a14b9eabe266aec4a69c8) remove arg validation for i2c sniffer — geo-tp
- [`14f82f60`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/14f82f60cebbe5030f10e8c009d524d941342160) update i2c tests — geo-tp
- [`d25321f7`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/d25321f7e104b2b31bb87da6ae82723e34cee38e) add i2c freq cmd — geo-tp
- [`77a460cb`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/77a460cb6b9d88ff9f63b5953e6234e18fb603c7) wire the new i2c sniffer and update tests — geo-tp
- [`240be58d`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/240be58dae00eaf21c3d527405dfc210e22efb26) remove old mcpwm sniffer — geo-tp
- [`36666aab`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/36666aabb868b3f7399cef8174d5f5d2722bd6b8) create rmt i2c service sniffer to replace the mcpwm based one — geo-tp
- [`55bb9c13`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/55bb9c13ce50bdab1bfbaed27c75525ba80db43e) properly set gpios before jtag scan, related to #179 — geo-tp
- [`ca8a31b9`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/ca8a31b981b5beb3674d890eae847bcfff604d61) merge stick S3 ir service into the global ir service as rmt impl — geo-tp
- [`4f2a0373`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/4f2a0373589fdf98b86f29c703500ed248e8c190) restore subghz freq after scan — geo-tp
- [`bc2ecb42`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/bc2ecb421ae4a7e16fdf3b4d01ec7ab3307143b8) Merge pull request #190 from DavDave1/m5sticks3_ir_receive_support — Geo
- [`d2c6c7e6`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/d2c6c7e65ae3285c95170d9c5c5227f59101f807) update i2c sniffer tests — geo-tp
- [`6acce434`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/6acce4348331d306d0cab051d1e502293b58cf1e) rework i2c sniffer to use mcpwm for more reliability, add a addr filter — geo-tp
- [`7685a2f3`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/7685a2f3f8af149228d63f6f453ccd3565f738d0) update subghz fake service for tests — geo-tp
- [`da4eabcb`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/da4eabcbef45fe1c21b6f2d4b716a33f18236035) handle rf path for adv cc1101 cap — geo-tp
- [`7eb2f731`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/7eb2f731b15b0b263fcbe928204674d8a40649b0) change cardputer subghz gpios to match cc1101 adv cap — geo-tp
- [`28a463bb`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/28a463bb6081ab3e318a02d214b3ca559ae2a136) adjust esp32s3 super mini gpios to use the most generic ones — geo-tp
- [`d385e0bc`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/d385e0bcaad4fee1e14574cbc19441d829495b2e) Merge pull request #188 from douardda/supermini — Geo
- [`41201529`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/412015293fe8c3f87294f40be90c3dd2497d94c7) update main to handle heap for boot mode — geo-tp
- [`1ba80d6e`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/1ba80d6ea3895e132295e4a61b2697eb83822e02) add tests for the sdr cdc adapter — geo-tp
- [`c553bb77`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/c553bb77b81584df647e4105ad17c2c4678b2771) add esp sdr to boot mode config — geo-tp
- [`f6f50c44`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/f6f50c44f97259655c566f2529b5ade47611ca67) remove useless state var — geo-tp
- [`050ac9d3`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/050ac9d3e0df5594bd3dcb5ce15b314562564be5) add new mode to usb adapter shell — geo-tp
- [`849433da`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/849433da7406a03d0399d5de059ab64ca6b9e3b3) update nvs service to work with the new oneshotboot enum — geo-tp
- [`584ee68a`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/584ee68aa2271bd7e0138bb96e1c57d83325ea42) create a service for sdr capture — geo-tp
- [`e7302be1`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/e7302be1d8abc2fc57abf834669a5c38e96cbdaf) create manager to handle heap at boot — geo-tp
- [`e45d1eb3`](https://github.com/geo-tp/ESP32-Bit-Pirate/commit/e45d1eb38c077981e9b7bbe91bee1e1948e165be) create sdr usb adapter — geo-tp

## Recent resource history

| Date | Commit | RAM | Flash | Boards | Tests |
|---|---|---:|---:|---:|---|
| 2026-09-26 | `d972aa8` | 97,328 B | 3,772,136 B | 18/18 | ✅ |
| 2026-09-27 | `d972aa8` | 97,328 B | 3,772,136 B | 18/18 | ✅ |
| 2026-10-04 | `8022f80` | 97,328 B | 3,772,120 B | 18/18 | ✅ |
| 2026-10-11 | `8ebef51` | 88,540 B | 3,800,651 B | 19/19 | ✅ |

---

This branch is generated automatically. Do not edit it manually.

Static RAM is PlatformIO's compile-time `.data + .bss` measurement; it is not runtime heap consumption after boot.
