#pragma once
#include <cstddef>
#include <cstdint>

enum class FlashEraseMode : uint8_t { None, Enter, EnterWriteEnable };

struct FlashChipInfo {
    uint8_t manufacturerId;
    uint8_t deviceIdHigh;
    uint8_t deviceIdLow;
    const char* manufacturerName;
    const char* modelName;
    uint32_t capacityBytes;
    bool supportsRead03;
    bool supportsRead13;
    bool supportsProgram02;
    bool supportsProgram12;
    uint8_t eraseOpcode;
    uint32_t eraseBlockBytes;
    FlashEraseMode eraseMode;
    bool supportsChipEraseC7;
};

static constexpr FlashChipInfo flashDatabase[] = {
    {0x01, 0x02, 0x12, "Spansion", "S25FL004A", 524288UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x01, 0x02, 0x13, "Spansion", "S25FL008A", 1048576UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x01, 0x02, 0x14, "Spansion", "S25FL016A", 2097152UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x01, 0x02, 0x15, "Spansion", "S25FL032A/P", 4194304UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x01, 0x02, 0x16, "Spansion", "S25FL064A/P", 8388608UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x01, 0x02, 0x19, "Spansion", "S25FL256S......0", 33554432UL, true, true, true, true, 0xDC, 65536UL, FlashEraseMode::None, true},
    {0x01, 0x40, 0x13, "Spansion", "S25FL204K", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x01, 0x40, 0x14, "Spansion", "S25FL208K", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x01, 0x40, 0x15, "Spansion", "S25FL116K/S25FL216K", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x01, 0x40, 0x16, "Spansion", "S25FL132K", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x01, 0x40, 0x17, "Spansion", "S25FL164K", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x01, 0x60, 0x18, "Spansion", "S25FL128L", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x01, 0x60, 0x19, "Spansion", "S25FL256L", 33554432UL, true, true, true, true, 0x21, 4096UL, FlashEraseMode::None, true},
    {0x0B, 0x40, 0x12, "XTX Technology Limited", "XT25F02E", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x0E, 0x40, 0x15, "Zbit Semiconductor", "ZB25VQ16", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x20, 0x10, "Eon", "EN25B05 / EN25B05T / EN25P05", 65536UL, true, false, true, false, 0x00, 0UL, FlashEraseMode::None, true},
    {0x1C, 0x20, 0x11, "Eon", "EN25B10 / EN25B10T / EN25P10", 131072UL, true, false, true, false, 0x00, 0UL, FlashEraseMode::None, true},
    {0x1C, 0x20, 0x12, "Eon", "EN25B20 / EN25B20T / EN25P20", 262144UL, true, false, true, false, 0x00, 0UL, FlashEraseMode::None, true},
    {0x1C, 0x20, 0x13, "Eon", "EN25B40 / EN25B40T / EN25P40", 524288UL, true, false, true, false, 0x00, 0UL, FlashEraseMode::None, true},
    {0x1C, 0x20, 0x14, "Eon", "EN25B80 / EN25B80T / EN25P80", 1048576UL, true, false, true, false, 0x00, 0UL, FlashEraseMode::None, true},
    {0x1C, 0x20, 0x15, "Eon", "EN25B16 / EN25B16T / EN25P16", 2097152UL, true, false, true, false, 0x00, 0UL, FlashEraseMode::None, true},
    {0x1C, 0x20, 0x16, "Eon", "EN25B32 / EN25B32T / EN25P32", 4194304UL, true, false, true, false, 0x00, 0UL, FlashEraseMode::None, true},
    {0x1C, 0x20, 0x17, "Eon", "EN25B64 / EN25B64T / EN25P64", 8388608UL, true, false, true, false, 0x00, 0UL, FlashEraseMode::None, true},
    {0x1C, 0x30, 0x13, "Eon", "EN25Q40", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x30, 0x14, "Eon", "EN25Q80(A)", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x30, 0x15, "Eon", "EN25Q16", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x30, 0x16, "Eon", "EN25Q32(A/B)", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x30, 0x17, "Eon", "EN25Q64", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x31, 0x10, "Eon", "EN25F05", 65536UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x31, 0x11, "Eon", "EN25F10", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x31, 0x12, "Eon", "EN25F20", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x31, 0x13, "Eon", "EN25F40", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x31, 0x14, "Eon", "EN25F80", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x31, 0x15, "Eon", "EN25F16", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x31, 0x16, "Eon", "EN25F32", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x31, 0x17, "Eon", "EN25F64", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x70, 0x15, "Eon", "EN25QH16", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x70, 0x16, "Eon", "EN25QH32 / EN25QH32B", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x70, 0x17, "Eon", "EN25QH64 / EN25QH64A", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x70, 0x18, "Eon", "EN25QH128", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1C, 0x71, 0x18, "Eon", "EN25QX128A", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x14, 0x01, "Atmel", "AT25EU0041A", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x16, 0x01, "Atmel", "AT25EU0161A", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x42, 0x00, "Atmel", "AT25DF011", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x43, 0x00, "Atmel", "AT25DF021", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x43, 0x01, "Atmel", "AT25DF021A", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x44, 0x01, "Atmel", "AT25DF041A", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x44, 0x08, "Atmel", "AT25FF041A", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x45, 0x01, "Atmel", "AT25DF081A / AT26DF081A", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x46, 0x00, "Atmel", "AT26DF161", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x46, 0x01, "Atmel", "AT26DF161A", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x46, 0x02, "Atmel", "AT25DF161", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x47, 0x00, "Atmel", "AT25DF321", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x47, 0x01, "Atmel", "AT25DF321A", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x48, 0x00, "Atmel", "AT25DF641(A)", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x65, 0x00, "Atmel", "AT25F512B", 65536UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x66, 0x01, "Atmel", "AT25FS010", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x66, 0x04, "Atmel", "AT25FS040", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x84, 0x01, "Atmel", "AT25SF041", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x85, 0x01, "Atmel", "AT25SF081", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x86, 0x00, "Atmel", "AT25DQ161", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x86, 0x01, "Atmel", "AT25SF161", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x1F, 0x87, 0x01, "Atmel", "AT25SF321", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x20, 0x10, "Micron/Numonyx/ST", "M25P05-A", 65536UL, true, false, true, false, 0xD8, 32768UL, FlashEraseMode::None, true},
    {0x20, 0x20, 0x11, "Micron/Numonyx/ST", "M25P10-A", 131072UL, true, false, true, false, 0xD8, 32768UL, FlashEraseMode::None, true},
    {0x20, 0x20, 0x12, "Micron/Numonyx/ST", "M25P20", 262144UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x20, 0x20, 0x13, "Micron/Numonyx/ST", "M25P40", 524288UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x20, 0x20, 0x14, "Micron/Numonyx/ST", "M25P80", 1048576UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x20, 0x20, 0x15, "Micron/Numonyx/ST", "M25P16", 2097152UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x20, 0x20, 0x16, "Micron/Numonyx/ST", "M25P32", 4194304UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x20, 0x20, 0x17, "Micron/Numonyx/ST", "M25P64", 8388608UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x20, 0x20, 0x18, "Micron/Numonyx/ST", "M25P128", 16777216UL, true, false, true, false, 0xD8, 262144UL, FlashEraseMode::None, true},
    {0x20, 0x40, 0x16, "XMC", "XM25QH32C/XM25QH32D", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x40, 0x17, "XMC", "XM25QH64C/XM25QH64D", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x40, 0x18, "XMC", "XM25QH128C/XM25QH128D", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x40, 0x19, "XMC", "XM25QH256C/XM25QH256D", 33554432UL, true, true, true, true, 0x20, 4096UL, FlashEraseMode::EnterWriteEnable, true},
    {0x20, 0x43, 0x18, "XMC", "XM25RH128C", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x70, 0x17, "XMC", "XM25QH64A", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x70, 0x18, "XMC", "XM25QH128A", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x71, 0x14, "Micron/Numonyx/ST", "M25PX80", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x71, 0x15, "Micron/Numonyx/ST", "M25PX16", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x71, 0x16, "Micron/Numonyx/ST", "M25PX32", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x71, 0x17, "Micron/Numonyx/ST", "M25PX64", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x80, 0x11, "Micron/Numonyx/ST", "M25PE10", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x80, 0x12, "Micron/Numonyx/ST", "M25PE20", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x80, 0x13, "Micron/Numonyx/ST", "M25PE40", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x80, 0x14, "Micron/Numonyx/ST", "M25PE80", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0x80, 0x15, "Micron/Numonyx/ST", "M25PE16", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0xBA, 0x16, "Micron/Numonyx/ST", "N25Q032..3E", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0xBA, 0x17, "Micron/Numonyx/ST", "N25Q064..3E", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0xBA, 0x18, "Micron / Micron/Numonyx/ST", "MT25QL128 / N25Q128..3E", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x20, 0xBA, 0x19, "Micron / Micron/Numonyx/ST", "MT25QL256 / N25Q256..3E", 33554432UL, true, true, true, true, 0x21, 4096UL, FlashEraseMode::None, true},
    {0x37, 0x30, 0x10, "AMIC", "A25L512", 65536UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x37, 0x30, 0x11, "AMIC", "A25L010", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x37, 0x30, 0x12, "AMIC", "A25L020", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x37, 0x30, 0x13, "AMIC", "A25L040", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x37, 0x30, 0x14, "AMIC", "A25L080", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x37, 0x30, 0x15, "AMIC", "A25L016", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x37, 0x30, 0x16, "AMIC", "A25L032", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x37, 0x40, 0x15, "AMIC", "A25LQ16", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x37, 0x40, 0x16, "AMIC", "A25LQ032/A25LQ32A", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x37, 0x40, 0x17, "AMIC", "A25LQ64", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x4A, 0x20, 0x13, "ESI", "ES25P40", 524288UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x4A, 0x20, 0x14, "ESI", "ES25P80", 1048576UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x62, 0x06, 0x12, "Sanyo", "LE25FU206A", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x62, 0x06, 0x13, "Sanyo", "LE25FU406C/LE25U40CMC", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x62, 0x11, 0x00, "Sanyo", "LE25FW403A", 524288UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x62, 0x16, 0x00, "Sanyo", "LE25FW203A", 262144UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x68, 0x40, 0x12, "Boya/BoHong Microelectronics", "BY25D20", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x68, 0x40, 0x13, "Boya/BoHong Microelectronics", "BY25D40", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x68, 0x40, 0x14, "Boya/BoHong Microelectronics", "B.25D80A", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x68, 0x40, 0x15, "Boya/BoHong Microelectronics", "B.25D16A", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x68, 0x40, 0x17, "Boya/BoHong Microelectronics", "B.25Q64AS", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x68, 0x40, 0x18, "Boya/BoHong Microelectronics", "B.25Q128AS", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x85, 0x20, 0x13, "PUYA", "P25Q40HB", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x85, 0x20, 0x15, "PUYA", "P25Q16HB", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x85, 0x40, 0x10, "PUYA", "P25Q06H", 65536UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x85, 0x40, 0x11, "PUYA", "P25Q11H", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x85, 0x40, 0x12, "PUYA", "P25Q21H", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x85, 0x60, 0x13, "PUYA", "P25D40SH", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x85, 0x60, 0x14, "PUYA", "P25D80H", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x85, 0x60, 0x16, "PUYA", "P25D32SH/P25Q32SH", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x89, 0x89, 0x11, "Intel", "25F160S33B8", 2097152UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x89, 0x89, 0x12, "Intel", "25F320S33B8", 4194304UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x89, 0x89, 0x13, "Intel", "25F640S33B8", 8388608UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x89, 0x89, 0x15, "Intel", "25F160S33T8", 2097152UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x89, 0x89, 0x16, "Intel", "25F320S33T8", 4194304UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x89, 0x89, 0x17, "Intel", "25F640S33T8", 8388608UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0x8C, 0x20, 0x16, "ESMT", "F25L32PA", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x9D, 0x14, 0x45, "ISSI", "IS25LQ016", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x9D, 0x40, 0x16, "ISSI", "IS25LQ032", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x9D, 0x60, 0x15, "ISSI", "IS25LP016", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x9D, 0x60, 0x16, "ISSI", "IS25LP032", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x9D, 0x60, 0x17, "ISSI", "IS25LP064", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x9D, 0x60, 0x18, "ISSI", "IS25LP128", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0x9D, 0x60, 0x19, "ISSI", "IS25LP256", 33554432UL, true, true, true, true, 0x21, 4096UL, FlashEraseMode::None, true},
    {0x9D, 0x60, 0x20, "ISSI", "IS25LP512MG", 67108864UL, true, true, true, true, 0x21, 4096UL, FlashEraseMode::None, true},
    {0xA1, 0x28, 0x18, "Fudan", "FM25W128", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xA1, 0x31, 0x10, "Fudan", "FM25F005", 65536UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xA1, 0x31, 0x11, "Fudan", "FM25F01", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xA1, 0x31, 0x12, "Fudan", "FM25F02(A)", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xA1, 0x31, 0x13, "Fudan", "FM25F04(A)", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xA1, 0x40, 0x13, "Fudan", "FM25Q04", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xA1, 0x40, 0x14, "Fudan", "FM25Q08", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xA1, 0x40, 0x15, "Fudan", "FM25Q16", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xA1, 0x40, 0x16, "Fudan", "FM25Q32", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xA1, 0x40, 0x17, "Fudan", "FM25Q64", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xA1, 0x40, 0x18, "Fudan", "FM25Q128", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xBA, 0x20, 0x12, "Zetta Device", "ZD25D20", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xBA, 0x20, 0x13, "Zetta Device", "ZD25D40", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xBF, 0x25, 0x4B, "SST", "SST25VF064C", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xBF, 0x26, 0x41, "SST", "SST26VF016B(A)", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xBF, 0x26, 0x42, "SST", "SST26VF032B(A)", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xBF, 0x26, 0x43, "SST", "SST26VF064B(A)", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x20, 0x10, "Macronix", "MX25L512(E)/MX25V512(C)", 65536UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x20, 0x11, "Macronix", "MX25L1005(C)/MX25L1006E", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x20, 0x12, "Macronix", "MX25L2005(C)/MX25L2006E", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x20, 0x13, "Macronix", "MX25L4005(A/C)/MX25L4006E", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x20, 0x14, "Macronix", "MX25L8005/MX25L8006E/MX25L8008E/MX25V8005", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x20, 0x15, "Macronix", "MX25L1605 / MX25V16066 / MX25L1605A/MX25L1606E/MX25L1608E / MX25L1605D/MX25L1608D/MX25L1673E", 2097152UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0xC2, 0x20, 0x16, "Macronix", "MX25L3205(A) / MX25L3205D/MX25L3208D / MX25L3206E/MX25L3208E / MX25L3233F/MX25L3273E", 4194304UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0xC2, 0x20, 0x17, "Macronix", "MX25L6405 / MX25L6405D / MX25L6406E/MX25L6408E / MX25L6436E/MX25L6445E/MX25L6465E / MX25L6473E / MX25L6473F", 8388608UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0xC2, 0x20, 0x18, "Macronix", "MX25L12805D / MX25L12833F / MX25L12845E/MX25L12865E / MX25L12850F", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x20, 0x19, "Macronix", "MX25L25635F/MX25L25645G", 33554432UL, true, true, true, true, 0x21, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x20, 0x1A, "Macronix", "MX66L51235F/MX25L51245G", 67108864UL, true, true, true, true, 0x21, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x23, 0x13, "Macronix", "MX25V4035F", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x23, 0x14, "Macronix", "MX25V8035F", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x24, 0x15, "Macronix", "MX25L1635D / MX25L1633E", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x25, 0x15, "Macronix", "MX25L1635E/MX25L1636E", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x28, 0x10, "Macronix", "MX25R512F", 65536UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x28, 0x11, "Macronix", "MX25R1035F", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x28, 0x12, "Macronix", "MX25R2035F", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x28, 0x13, "Macronix", "MX25R4035F", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x28, 0x14, "Macronix", "MX25R8035F", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x28, 0x15, "Macronix", "MX25R1635F", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x28, 0x16, "Macronix", "MX25R3235F", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x28, 0x17, "Macronix", "MX25R6435F", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x5E, 0x16, "Macronix", "MX25L3235D", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x75, 0x19, "Macronix", "MX77L25650F", 33554432UL, true, true, true, true, 0x20, 4096UL, FlashEraseMode::Enter, true},
    {0xC2, 0x95, 0x17, "Macronix", "MX25L6495F", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC2, 0x9E, 0x16, "Macronix", "MX25L3255E", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x31, 0x14, "GigaDevice", "GD25T80", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x40, 0x10, "GigaDevice", "GD25Q512", 65536UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x40, 0x11, "GigaDevice", "GD25Q10", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x40, 0x12, "GigaDevice", "GD25Q20(B)", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x40, 0x13, "GigaDevice", "GD25Q40(B)", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x40, 0x14, "GigaDevice", "GD25Q80(B)", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x40, 0x15, "GigaDevice", "GD25Q16(B)", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x40, 0x16, "GigaDevice", "GD25Q32(B)", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x40, 0x17, "GigaDevice", "GD25Q64(B)", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x40, 0x18, "GigaDevice", "GD25B128B/GD25Q128B / GD25Q128E/GD25B128E/GD25R128E/GD25Q127C / GD25Q128C", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x40, 0x19, "GigaDevice", "GD25Q256E/GD25B256E/GD25R256E/GD25Q256D", 33554432UL, true, true, true, true, 0x21, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x42, 0x12, "GigaDevice", "GD25VQ21B", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x42, 0x13, "GigaDevice", "GD25VQ40C / GD25VQ41B", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x42, 0x14, "GigaDevice", "GD25VQ80C", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x43, 0x19, "GigaDevice", "GD25F256F", 33554432UL, true, true, true, true, 0x21, 4096UL, FlashEraseMode::None, true},
    {0xC8, 0x65, 0x14, "GigaDevice", "GD25WQ80E", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xD5, 0x30, 0x11, "Nantronics", "N25S10", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xD5, 0x30, 0x12, "Nantronics", "N25S20", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xD5, 0x30, 0x13, "Nantronics", "N25S40", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xD5, 0x30, 0x14, "Nantronics", "N25S80", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xD5, 0x30, 0x15, "Nantronics", "N25S16", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x20, 0x14, "Winbond", "W25P80", 1048576UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0xEF, 0x20, 0x15, "Winbond", "W25P16", 2097152UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0xEF, 0x20, 0x16, "Winbond", "W25P32", 4194304UL, true, false, true, false, 0xD8, 65536UL, FlashEraseMode::None, true},
    {0xEF, 0x30, 0x11, "Winbond", "W25X10", 131072UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x30, 0x12, "Winbond", "W25X20", 262144UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x30, 0x13, "Winbond", "W25X40", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x30, 0x14, "Winbond", "W25X80", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x30, 0x15, "Winbond", "W25X16", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x30, 0x16, "Winbond", "W25X32", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x30, 0x17, "Winbond", "W25X64", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x40, 0x13, "Winbond", "W25Q40.V", 524288UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x40, 0x14, "Winbond", "W25Q80BV/W25Q80DV / W25Q80RV", 1048576UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x40, 0x15, "Winbond", "W25Q16.V", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x40, 0x16, "Winbond", "W25Q32BV/W25Q32CV/W25Q32DV / W25Q32FV / W25Q32JV", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x40, 0x17, "Winbond", "W25Q64BV/W25Q64CV/W25Q64FV / W25Q64JV-.Q", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x40, 0x18, "Winbond", "W25Q128.V", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x4A, 0x17, "Winbond", "W77Q64JV", 8388608UL, true, true, true, true, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x4A, 0x18, "Winbond", "W77Q128JV", 16777216UL, true, true, true, true, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x70, 0x15, "Winbond", "W25Q16JV_M", 2097152UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x70, 0x16, "Winbond", "W25Q32JV_M", 4194304UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x70, 0x17, "Winbond", "W25Q64JV-.M", 8388608UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x70, 0x18, "Winbond", "W25Q128.V..M", 16777216UL, true, false, true, false, 0x20, 4096UL, FlashEraseMode::None, true},
    {0xEF, 0x70, 0x19, "Winbond", "W25Q256JV_M", 33554432UL, true, true, true, true, 0x21, 4096UL, FlashEraseMode::None, true},
};

static constexpr std::size_t flashDatabaseSize = sizeof(flashDatabase) / sizeof(flashDatabase[0]);

inline const FlashChipInfo* findFlashInfo(uint8_t manufacturerId, uint8_t deviceIdHigh, uint8_t deviceIdLow) {
    for (const auto& chip : flashDatabase) {
        if (chip.manufacturerId == manufacturerId && chip.deviceIdHigh == deviceIdHigh && chip.deviceIdLow == deviceIdLow)
            return &chip;
    }
    return nullptr;
}

inline const char* findManufacturerName(uint8_t manufacturerId) {
    for (const auto& chip : flashDatabase)
        if (chip.manufacturerId == manufacturerId) return chip.manufacturerName;
    return "Unknown";
}

// Flash compatibility and memory limits.
static constexpr uint32_t flashAddressLimit = 16UL * 1024 * 1024;
// Keep room for the shell and other tasks on devices with about 150 KiB free heap.
static constexpr size_t flashPatchBufferLimit = 64UL * 1024;
static constexpr size_t flashHeapReserve = 32UL * 1024;

inline size_t flashPatchAvailableMemory(size_t freeHeap, size_t largestBlock) {
    if (freeHeap <= flashHeapReserve) return 0;
    const size_t available = freeHeap - flashHeapReserve;
    return available < largestBlock ? available : largestBlock;
}
enum class FlashOperation { Read, Program, BlockErase, Patch, ChipErase };

inline bool flashEraseProfileSupported(const FlashChipInfo& chip) {
    const bool legacy = (chip.eraseOpcode == 0x20 && chip.eraseBlockBytes == 4096) ||
                        (chip.eraseOpcode == 0x52 && chip.eraseBlockBytes == 32768) ||
                        (chip.eraseOpcode == 0xD8 && (chip.eraseBlockBytes == 32768 || chip.eraseBlockBytes == 65536 || chip.eraseBlockBytes == 262144));
    if (chip.capacityBytes <= flashAddressLimit)
        return legacy && chip.eraseMode == FlashEraseMode::None;
    if (chip.eraseMode == FlashEraseMode::Enter || chip.eraseMode == FlashEraseMode::EnterWriteEnable)
        return legacy;
    if (chip.eraseMode != FlashEraseMode::None) return false;
    return (chip.eraseOpcode == 0x21 && chip.eraseBlockBytes == 4096) ||
           ((chip.eraseOpcode == 0x53 || chip.eraseOpcode == 0x5C) && chip.eraseBlockBytes == 32768) ||
           (chip.eraseOpcode == 0xDC && chip.eraseBlockBytes == 65536);
}

inline const char* flashCompatibilityError(const FlashChipInfo* chip,
                                           FlashOperation operation = FlashOperation::Read) {
    if (!chip) return "Unknown chip: capacity and command profile are not verified. Probe only.";
    if (!chip->capacityBytes) return "Unknown capacity. Probe only.";
    const bool large = chip->capacityBytes > flashAddressLimit;
    if (large ? !chip->supportsRead13 : !chip->supportsRead03)
        return "Read command is not verified. Probe only.";
    if ((operation == FlashOperation::Program || operation == FlashOperation::Patch) &&
        (large ? !chip->supportsProgram12 : !chip->supportsProgram02))
        return "Page program command is not verified.";
    if (operation == FlashOperation::BlockErase || operation == FlashOperation::Patch) {
        if (!flashEraseProfileSupported(*chip) || !chip->eraseBlockBytes ||
            chip->capacityBytes % chip->eraseBlockBytes)
            return "No common block erase geometry verified for this JEDEC ID.";
    }
    if (operation == FlashOperation::ChipErase && !chip->supportsChipEraseC7)
        return "Full-chip erase command 0xC7 is not verified.";
    if (operation == FlashOperation::Patch && chip->eraseBlockBytes > flashPatchBufferLimit)
        return "Automatic erase requires a block buffer larger than the 64 KiB RAM limit.";
    return nullptr;
}

inline bool flashRangeSupported(const FlashChipInfo* chip, uint32_t address, size_t length) {
    return !flashCompatibilityError(chip) && address < chip->capacityBytes &&
           length > 0 && length <= chip->capacityBytes - address;
}
