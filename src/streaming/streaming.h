#pragma once

#include "core/types.h"
#include <cstdint>

enum { DROP_OK = 0, DROP_NO_EXPORTS, DROP_BAD_SLOT, DROP_NOT_LOADED, DROP_NOT_OURS };

const char* strStatusText(uint32_t flags);
// Game thread only. Drops the resident object so the next request reads through our handle.
int forceReloadSlot(uint32_t id, uint32_t ourHandle);
const char* dropWhyText(int w);
void resolveOccupiedSlotExports();
uint32_t findModuleSlotSafe(void* module, const char* stem, int build);
const char* slotWhyText(int w);
void slotMemoryCost(uint32_t id, uint64_t* virt, uint64_t* phys);
uint32_t targetStreamingId(const char* slot, int* why = nullptr);
void noteSlotWhy(const char* slot, int why);
bool localRawHandle(const char* file, uint32_t* handle);
bool validStreamingId(uint32_t id);
StrMgr* exportedManagerSafe();
int recoverOccupiedSlot(Ov& ov);
