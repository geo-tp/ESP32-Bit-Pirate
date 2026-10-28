#include "BootMemoryManager.h"
#include "Interfaces/INvsService.h"

#include <esp_heap_caps_init.h>
#include <heap_memory_layout.h>
#include <nvs.h>
#include <soc/soc.h>

#if !CONFIG_IDF_TARGET_ESP32S3
#error The boot memory pool requires ESP32-S3.
#endif

extern "C" {
extern uint8_t _heap_start[];
extern uint8_t _iram_end[];
}

namespace {
constexpr intptr_t bankStart = BootMemoryManager::SDR_BANK_START;
constexpr intptr_t bankEnd = bankStart + BootMemoryManager::SDR_BANK_BYTES;
constexpr intptr_t instructionDataOffset = 0x403c0000 - bankStart;
// Leave ROM/startup-stack memory above this boundary under IDF's management.
constexpr intptr_t poolEnd = SOC_ROM_STACK_START - SOC_ROM_STACK_SIZE;
static_assert(bankEnd < poolEnd, "SDR bank must fit inside the application heap pool");

enum class PoolState : uint8_t { Reserved, NormalHeap, SdrHeap, Failed };
PoolState poolState = PoolState::Reserved;

PoolState stateForMode(OneShotBootMode mode) {
    return mode == OneShotBootMode::SdrCdc ? PoolState::SdrHeap : PoolState::NormalHeap;
}

bool poolLayoutValid() {
    const intptr_t start = reinterpret_cast<intptr_t>(_heap_start);
    const intptr_t iramDataEnd = reinterpret_cast<intptr_t>(_iram_end) - instructionDataOffset;
    return start > 0 && start < bankStart && iramDataEnd <= start;
}

esp_err_t readPendingBootMode(OneShotBootMode& mode) {
    mode = OneShotBootMode::None;
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(INvsService::NVS_NAMESPACE, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (result != ESP_OK) return result;

    uint8_t rawMode = static_cast<uint8_t>(OneShotBootMode::None);
    result = nvs_get_u8(handle, INvsService::ONE_SHOT_BOOT_KEY, &rawMode);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (result != ESP_OK) return result;

    mode = static_cast<OneShotBootMode>(rawMode);
    return ESP_OK;
}
} // namespace

// Reserve the entire D/IRAM heap tail at IDF startup. Restoring it as one region
// outside SDR avoids permanently splitting the largest allocatable block.
// _heap_start already excludes static data and the DRAM alias occupied by IRAM.
SOC_RESERVE_MEMORY_REGION((intptr_t)_heap_start, poolEnd, bitpirate_sdr_pool);

esp_err_t BootMemoryManager::initialize() {
    if (poolState == PoolState::NormalHeap || poolState == PoolState::SdrHeap) return ESP_OK;
    if (poolState == PoolState::Failed) return ESP_ERR_INVALID_STATE;

    OneShotBootMode mode;
    esp_err_t result = readPendingBootMode(mode);
    if (result != ESP_OK) return result;
    if (!poolLayoutValid()) return ESP_ERR_INVALID_SIZE;

    const PoolState requested = stateForMode(mode);
    const intptr_t start = reinterpret_cast<intptr_t>(_heap_start);
    // If only one SDR side is registered successfully, retries must not add it
    // again or expose the RF bank. The caller aborts this boot on any error.
    poolState = PoolState::Failed;
    if (requested == PoolState::NormalHeap) {
        result = heap_caps_add_region(start, poolEnd);
    } else {
        result = heap_caps_add_region(start, bankStart);
        if (result == ESP_OK) result = heap_caps_add_region(bankEnd, poolEnd);
    }
    if (result == ESP_OK) poolState = requested;
    return result;
}

esp_err_t BootMemoryManager::validateBootMode(OneShotBootMode mode) {
    return poolState == stateForMode(mode) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

bool BootMemoryManager::isSdrBankReserved() {
    return poolState == PoolState::SdrHeap;
}
