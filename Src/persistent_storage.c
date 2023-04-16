#include "persistent_storage.h"

const uint8_t gPersistentData[PERSISTENT_STORAGE_SIZE] __attribute__((section(".PersistentStorage")));

#include <string.h>
#include <stdbool.h>

#include "stm32h7xx_hal.h"

#define FLASH_WORD_SIZE (32)

typedef struct {
    uint32_t flashAddr; // config location in flash (persistent storage)
    uint32_t size; // size of config
    uint32_t id; // id of this config entry
} ConfigEntry;

#define MAX_CONFIG_ENTRIES (8)
static ConfigEntry sEntries[MAX_CONFIG_ENTRIES] = { 0 };
static uint32_t sEntryCnt = 0;

static ConfigEntry * ps_get_entry_by_id(uint32_t id) {
    uint32_t ei;
    for (ei = 0; ei < sEntryCnt; ei++) {
        if (sEntries[ei].id == id) { // collision found...
            return &(sEntries[ei]);
        }
    }
    return NULL;
}

bool ps_add_entry(uint32_t size, uint32_t id) {
    // check for free space
    if (sEntryCnt == MAX_CONFIG_ENTRIES) {
        return false;
    }

    // check for conflicting id
    if (ps_get_entry_by_id(id) != NULL) {
        return false;
    }

    // new entry can be safely created...

    // get free address
    uint32_t freeAddr;
    if (sEntryCnt == 0) {
        freeAddr = (uint32_t)gPersistentData;
    } else {
        // get first theoretical free address
        freeAddr = sEntries[sEntryCnt].flashAddr + sEntries[sEntryCnt].size;

        // align address to next 32-byte boundary
        freeAddr = ((freeAddr % FLASH_WORD_SIZE) != 0) ? ((freeAddr & 0xFFFFFFFFE0) + FLASH_WORD_SIZE) : freeAddr;
    }

    // fill-in entry fields
    ConfigEntry * entry = sEntries + sEntryCnt;
    entry->id = id;
    entry->size = size;
    entry->flashAddr = freeAddr;

    sEntryCnt++;

    return true;
}

static void ps_write(void * ptr, uint32_t flashAddr, uint32_t size) {
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGSERR);

    uint8_t maskingBuffer[FLASH_WORD_SIZE];
    int notIntegerSection = (size % FLASH_WORD_SIZE) != 0;
    uint32_t blockCnt = size / FLASH_WORD_SIZE + notIntegerSection; // determine block count
    for (uint32_t bi = 0; bi < blockCnt; bi++) { // write by 32-bytes
        uint32_t offset = bi * FLASH_WORD_SIZE; // offset
        uint32_t src = ((uint32_t)ptr) + offset; // source address

        // last iteration, mask out bytes not corresponding to input buffer
        if (notIntegerSection && (bi + 1) >= blockCnt) {
            memset(maskingBuffer, 0, FLASH_WORD_SIZE); // clear buffer
            memcpy(maskingBuffer, (void *)src, size % FLASH_WORD_SIZE);
            src = (uint32_t)maskingBuffer;
        }

        // write!
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, flashAddr + offset, src);
    }

    HAL_FLASH_Lock();
}

void ps_store(uint32_t id, void * ptr) {
    ConfigEntry * entry = ps_get_entry_by_id(id);
    if (entry != NULL) {
        ps_write(ptr, entry->flashAddr, entry->size);
    }
}

const void * ps_load(uint32_t id) {
    ConfigEntry * entry = ps_get_entry_by_id(id);
    return (const void *) entry->flashAddr;
}

void ps_clear() {
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGSERR);
    FLASH_Erase_Sector(FLASH_SECTOR_7, FLASH_BANK_1, VOLTAGE_RANGE_3);
}
