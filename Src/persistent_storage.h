#ifndef PERSISTENT_STORAGE_H_
#define PERSISTENT_STORAGE_H_

#include <stdint.h>
#include <stdbool.h>

#define PERSISTENT_STORAGE_SIZE (128 * 1024) // size of persistent storage
extern const uint8_t gPersistentData[PERSISTENT_STORAGE_SIZE];

bool ps_add_entry(uint32_t size, uint32_t id); // add entry to persistent storage
void ps_store(uint32_t id, void * ptr); // store to persistent storage
const void * ps_load(uint32_t id); // load by id
void ps_clear(); // clear storage area

#endif /* PERSISTENT_STORAGE_H_ */
