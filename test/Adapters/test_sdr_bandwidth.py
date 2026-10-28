# SPDX-License-Identifier: MIT
"""Compile the real S3 capture service with a simulated PHY/dump engine."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SDK = r'''
#pragma once
#include <cstdint>
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_ARG = 0x102,
    ESP_ERR_INVALID_STATE = 0x103, ESP_ERR_INVALID_SIZE = 0x104,
    ESP_ERR_NOT_SUPPORTED = 0x106, ESP_ERR_WIFI_NOT_INIT = 0x3001,
    ESP_ERR_NVS_NOT_FOUND = 0x1102;
#define CONFIG_IDF_TARGET_ESP32S3 1
#define SOC_ROM_STACK_START 0x3fceb710
#define SOC_ROM_STACK_SIZE 0x2000
#define SOC_RESERVE_MEMORY_REGION(a,b,c)
#define REG_READ(reg) test_reg_read(reg)
#define REG_WRITE(reg,value) test_reg_write(reg,value)
uint32_t test_reg_read(uintptr_t);
void test_reg_write(uintptr_t, uint32_t);
using wifi_mode_t = int;
struct wifi_init_config_t {};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
constexpr int ESP_BT_CONTROLLER_STATUS_IDLE = 0, WIFI_STORAGE_RAM = 0,
    WIFI_MODE_NULL = 0, WIFI_PS_NONE = 0, WIFI_SECOND_CHAN_NONE = 0;
esp_err_t heap_caps_add_region(intptr_t, intptr_t);
using nvs_handle_t = unsigned;
constexpr int NVS_READONLY = 0;
esp_err_t nvs_open(const char*, int, nvs_handle_t*);
esp_err_t nvs_get_u8(nvs_handle_t, const char*, uint8_t*);
void nvs_close(nvs_handle_t);
int esp_bt_controller_get_status();
esp_err_t esp_wifi_get_mode(wifi_mode_t*);
esp_err_t nvs_flash_init();
esp_err_t esp_event_loop_create_default();
esp_err_t esp_wifi_init(wifi_init_config_t*);
esp_err_t esp_wifi_set_storage(int);
esp_err_t esp_wifi_set_mode(int);
esp_err_t esp_wifi_start();
esp_err_t esp_wifi_stop();
esp_err_t esp_wifi_deinit();
esp_err_t esp_wifi_set_ps(int);
esp_err_t esp_wifi_set_promiscuous(bool);
esp_err_t esp_wifi_set_channel(unsigned, unsigned);
int64_t esp_timer_get_time();
'''


def write_fake_sdk(path):
    (path / "fake_sdk.h").write_text(SDK)
    for name in ("esp_err.h", "esp_bt.h", "esp_event.h", "esp_heap_caps_init.h",
                 "esp_timer.h", "esp_wifi.h", "heap_memory_layout.h", "nvs_flash.h", "nvs.h", "soc/soc.h"):
        header = path / name
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text('#include "fake_sdk.h"\n')


@unittest.skipUnless(shutil.which("g++") and sys.platform == "linux", "Needs Linux mmap and g++")
class SdrBandwidthTest(unittest.TestCase):
    def test_register_lifecycle_and_curve(self):
        with tempfile.TemporaryDirectory(prefix="sdr-bandwidth-") as directory:
            path = Path(directory)
            write_fake_sdk(path)
            harness = ROOT / "test/Adapters/sdr_bandwidth_harness.inc"
            executable = path / "capture"
            subprocess.run(["g++", "-x", "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(path), "-I" + str(ROOT / "src"), "-I" + str(ROOT / "src/Services"),
                            str(harness), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], cwd=path, check=True)


if __name__ == "__main__":
    unittest.main()
