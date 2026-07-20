// ----- AI
/* 主 Flash 末尾双槽 generation+CRC 记录；每槽独占一个 1 KiB 扇区。 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ti_msp_dl_config.h"
#include "autotune_types.h"
#include "control_params.h"
#include "crc16.h"
#include "flash_params.h"

#define FLASH_RECORD_MAGIC       (0x31504941UL)
#define FLASH_RECORD_SCHEMA      (1U)
#define FLASH_RECORD_CRC_OFFSET  (74U)
#define FLASH_PROGRAM_CHUNK_SIZE (8U)

/*
 * location+noinit 让链接器把最后 2 KiB 显式占位但不生成初始化数据，
 * 因此应用代码不可能与参数槽重叠。
 */
const volatile uint8_t gFlashParamsSlotA[FLASH_PARAMS_SLOT_SIZE]
    __attribute__((noinit, used, retain, location(FLASH_PARAMS_SLOT_A_ADDRESS)));
const volatile uint8_t gFlashParamsSlotB[FLASH_PARAMS_SLOT_SIZE]
    __attribute__((noinit, used, retain, location(FLASH_PARAMS_SLOT_B_ADDRESS)));

typedef struct {
    bool valid;
    uint32_t generation;
    uint16_t paramVersion;
    ControlParams params;
    uint32_t address;
} DecodedFlashRecord;

typedef union {
    uint8_t bytes[FLASH_PARAMS_RECORD_SIZE];
    uint32_t words[FLASH_PARAMS_RECORD_SIZE / sizeof(uint32_t)];
} AlignedFlashRecord;

static bool FlashParams_DecodeAt(
    uint32_t address, DecodedFlashRecord *record);
static void FlashParams_BuildRecord(AlignedFlashRecord *record,
    const ControlParams *params, uint16_t paramVersion, uint32_t generation);
static bool FlashParams_ReadBytes(
    uint32_t address, uint8_t *destination, uint16_t length);
static bool FlashParams_ParamsEqual(
    const ControlParams *left, const ControlParams *right);
static uint16_t FlashParams_ReadU16(const uint8_t *source);
static uint32_t FlashParams_ReadU32(const uint8_t *source);
static void FlashParams_WriteU16(uint8_t *destination, uint16_t value);
static void FlashParams_WriteU32(uint8_t *destination, uint32_t value);

bool FlashParams_LoadLatest(
    ControlParams *params, uint16_t *paramVersion, uint32_t *generation)
{
    DecodedFlashRecord slotA;
    DecodedFlashRecord slotB;
    const DecodedFlashRecord *selected = 0;

    if ((params == 0) || (paramVersion == 0) || (generation == 0)) {
        return false;
    }
    (void) FlashParams_DecodeAt(FLASH_PARAMS_SLOT_A_ADDRESS, &slotA);
    (void) FlashParams_DecodeAt(FLASH_PARAMS_SLOT_B_ADDRESS, &slotB);
    if (slotA.valid && slotB.valid) {
        selected = (slotB.generation > slotA.generation) ? &slotB : &slotA;
    } else if (slotA.valid) {
        selected = &slotA;
    } else if (slotB.valid) {
        selected = &slotB;
    }
    if (selected == 0) {
        return false;
    }
    *params = selected->params;
    *paramVersion = selected->paramVersion;
    *generation = selected->generation;
    return true;
}

FlashParamsResult FlashParams_Commit(
    const ControlParams *params, uint16_t paramVersion)
{
    DecodedFlashRecord slotA;
    DecodedFlashRecord slotB;
    const DecodedFlashRecord *latest = 0;
    uint32_t targetAddress;
    uint32_t nextGeneration;
    AlignedFlashRecord newRecord;
    uint16_t offset;
    uint32_t savedPrimask;
    DL_FLASHCTL_COMMAND_STATUS commandStatus;

    if ((params == 0) ||
        (ControlParams_Validate(params) != PARAMS_VALID)) {
        return FLASH_PARAMS_INVALID_ARGUMENT;
    }
    (void) FlashParams_DecodeAt(FLASH_PARAMS_SLOT_A_ADDRESS, &slotA);
    (void) FlashParams_DecodeAt(FLASH_PARAMS_SLOT_B_ADDRESS, &slotB);
    if (slotA.valid && slotB.valid) {
        latest = (slotB.generation > slotA.generation) ? &slotB : &slotA;
    } else if (slotA.valid) {
        latest = &slotA;
    } else if (slotB.valid) {
        latest = &slotB;
    }
    if ((latest != 0) && (latest->paramVersion == paramVersion) &&
        FlashParams_ParamsEqual(&latest->params, params)) {
        return FLASH_PARAMS_ALREADY_CURRENT;
    }
    targetAddress = ((latest != 0) &&
        (latest->address == FLASH_PARAMS_SLOT_A_ADDRESS)) ?
        FLASH_PARAMS_SLOT_B_ADDRESS : FLASH_PARAMS_SLOT_A_ADDRESS;
    nextGeneration = (latest == 0) ? 1U : latest->generation + 1U;
    if (nextGeneration == 0U) {
        nextGeneration = 1U;
    }
    FlashParams_BuildRecord(
        &newRecord, params, paramVersion, nextGeneration);

    savedPrimask = __get_PRIMASK();
    /* Flash 擦写时屏蔽全局中断，避免 ISR 在主 Flash 忙时取指；上层已处于 FLASH_WRITE 且电机停止。 */
    __disable_irq();
    /* 解除目标扇区的动态写保护，仅针对本次双槽中的非当前槽。 */
    DL_FlashCTL_unprotectSector(
        FLASHCTL, targetAddress, DL_FLASHCTL_REGION_SELECT_MAIN);
    /* 使用 DriverLib 的 RAM 执行版本擦除一个 1 KiB 扇区，避免在擦除期间从 Flash 取函数代码。 */
    commandStatus = DL_FlashCTL_eraseMemoryFromRAM(
        FLASHCTL, targetAddress, DL_FLASHCTL_COMMAND_SIZE_SECTOR);
    if (commandStatus != DL_FLASHCTL_COMMAND_STATUS_PASSED) {
        if (savedPrimask == 0U) { __enable_irq(); }
        return FLASH_PARAMS_ERASE_FAILED;
    }

    for (offset = 0U; offset < FLASH_PARAMS_RECORD_SIZE;
         offset += FLASH_PROGRAM_CHUNK_SIZE) {
        /* 每个 64 位 Flash word 编程前重新确认目标扇区写保护已解除。 */
        DL_FlashCTL_unprotectSector(
            FLASHCTL, targetAddress, DL_FLASHCTL_REGION_SELECT_MAIN);
        /* 从 RAM 执行 64 位编程并让硬件生成 ECC，地址始终 8 字节对齐。 */
        commandStatus = DL_FlashCTL_programMemoryFromRAM64WithECCGenerated(
            FLASHCTL, targetAddress + offset,
            &newRecord.words[offset / sizeof(uint32_t)]);
        if (commandStatus != DL_FLASHCTL_COMMAND_STATUS_PASSED) {
            if (savedPrimask == 0U) { __enable_irq(); }
            return FLASH_PARAMS_PROGRAM_FAILED;
        }
    }
    if (savedPrimask == 0U) {
        /* 擦写结束后恢复进入函数前的中断开启状态。 */
        __enable_irq();
    }

    {
        DecodedFlashRecord verified;
        if (!FlashParams_DecodeAt(targetAddress, &verified) ||
            !verified.valid || (verified.generation != nextGeneration) ||
            (verified.paramVersion != paramVersion) ||
            !FlashParams_ParamsEqual(&verified.params, params)) {
            return FLASH_PARAMS_VERIFY_FAILED;
        }
    }
    return FLASH_PARAMS_OK;
}

static bool FlashParams_DecodeAt(
    uint32_t address, DecodedFlashRecord *record)
{
    AlignedFlashRecord raw;
    uint16_t storedCrc;
    uint16_t calculatedCrc;

    (void) memset(record, 0, sizeof(*record));
    record->address = address;
    if (!FlashParams_ReadBytes(address, raw.bytes, sizeof(raw.bytes))) {
        return false;
    }
    if ((FlashParams_ReadU32(&raw.bytes[0]) != FLASH_RECORD_MAGIC) ||
        (FlashParams_ReadU16(&raw.bytes[4]) != FLASH_RECORD_SCHEMA) ||
        (FlashParams_ReadU16(&raw.bytes[6]) != CONTROL_PARAMS_WIRE_SIZE)) {
        return false;
    }
    storedCrc = FlashParams_ReadU16(&raw.bytes[FLASH_RECORD_CRC_OFFSET]);
    calculatedCrc = CRC16_Calculate(raw.bytes, FLASH_RECORD_CRC_OFFSET);
    if (storedCrc != calculatedCrc) {
        return false;
    }
    if (!ControlParams_DecodeWire(&raw.bytes[16],
            CONTROL_PARAMS_WIRE_SIZE, &record->params) ||
        (ControlParams_Validate(&record->params) != PARAMS_VALID)) {
        return false;
    }
    record->generation = FlashParams_ReadU32(&raw.bytes[8]);
    record->paramVersion = FlashParams_ReadU16(&raw.bytes[12]);
    record->valid = true;
    return true;
}

static void FlashParams_BuildRecord(AlignedFlashRecord *record,
    const ControlParams *params, uint16_t paramVersion, uint32_t generation)
{
    (void) memset(record->bytes, 0xFF, sizeof(record->bytes));
    FlashParams_WriteU32(&record->bytes[0], FLASH_RECORD_MAGIC);
    FlashParams_WriteU16(&record->bytes[4], FLASH_RECORD_SCHEMA);
    FlashParams_WriteU16(&record->bytes[6], CONTROL_PARAMS_WIRE_SIZE);
    FlashParams_WriteU32(&record->bytes[8], generation);
    FlashParams_WriteU16(&record->bytes[12], paramVersion);
    FlashParams_WriteU16(&record->bytes[14], 0U);
    (void) ControlParams_EncodeWire(
        params, &record->bytes[16], CONTROL_PARAMS_WIRE_SIZE);
    FlashParams_WriteU16(&record->bytes[FLASH_RECORD_CRC_OFFSET],
        CRC16_Calculate(record->bytes, FLASH_RECORD_CRC_OFFSET));
}

static bool FlashParams_ReadBytes(
    uint32_t address, uint8_t *destination, uint16_t length)
{
    const volatile uint8_t *source = (const volatile uint8_t *) address;
    uint16_t index;
    if (destination == 0) { return false; }
    for (index = 0U; index < length; index++) {
        destination[index] = source[index];
    }
    return true;
}

static bool FlashParams_ParamsEqual(
    const ControlParams *left, const ControlParams *right)
{
    uint8_t leftWire[CONTROL_PARAMS_WIRE_SIZE];
    uint8_t rightWire[CONTROL_PARAMS_WIRE_SIZE];
    if (!ControlParams_EncodeWire(left, leftWire, sizeof(leftWire)) ||
        !ControlParams_EncodeWire(right, rightWire, sizeof(rightWire))) {
        return false;
    }
    return memcmp(leftWire, rightWire, sizeof(leftWire)) == 0;
}

static uint16_t FlashParams_ReadU16(const uint8_t *source)
{
    return (uint16_t) source[0] | ((uint16_t) source[1] << 8);
}

static uint32_t FlashParams_ReadU32(const uint8_t *source)
{
    return (uint32_t) source[0] | ((uint32_t) source[1] << 8) |
        ((uint32_t) source[2] << 16) | ((uint32_t) source[3] << 24);
}

static void FlashParams_WriteU16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t) value;
    destination[1] = (uint8_t) (value >> 8);
}

static void FlashParams_WriteU32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t) value;
    destination[1] = (uint8_t) (value >> 8);
    destination[2] = (uint8_t) (value >> 16);
    destination[3] = (uint8_t) (value >> 24);
}
// ----- AI
