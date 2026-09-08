#include "streaming/streaming.h"
#include "core/state.h"
#include "core/logger.h"
#include "core/utils.h"
#include "core/pattern.h"
#include "core/cfx.h"
#include <windows.h>
#include <cstring>
#include <string>
#include <set>

// rage::strStreamingInfo packs three things into that second dword (Rockstar streamingdefs.h,
// cross-checked against Cfx's ReleaseObject(idx, 0xF1)):
//   bits 0-1   status: 0 NOTLOADED, 1 LOADED, 2 LOADREQUESTED, 3 LOADING
//   bits 2-15  dependent count
//   bits 16-31 STRFLAGs: DONTDELETE 1<<0, FORCE_LOAD 1<<1, PRIORITY_LOAD 1<<2, LOADSCENE 1<<3,
//              MISSION 1<<4, CUTSCENE 1<<5, INTERIOR 1<<6, ZONEDASSET 1<<7
// Read only. Never write a STRFLAG by hand: the setters also move the entry between the loaded
// and persistent lists and maintain m_numPriorityRequests, and a raw write desyncs both.
const char* strStatusText(uint32_t flags)
{
    switch (flags & 3) {
    case 0:  return "not loaded";
    case 1:  return "ALREADY LOADED";
    case 2:  return "load requested";
    default: return "loading";
    }
}

void resolveOccupiedSlotExports()
{
    if (g_getStreamingManagerFn && g_getStreamingModuleFn && g_getRawEntriesFn) return;
    HMODULE streamingDll = GetModuleHandleA("gta-streaming-five.dll");
    if (!streamingDll) return;
    if (!g_getStreamingManagerFn)
        g_getStreamingManagerFn = (GetStreamingManager_t)GetProcAddress(streamingDll,
            "?GetInstance@Manager@streaming@@SAPEAV12@XZ");
    if (!g_getStreamingModuleFn)
        g_getStreamingModuleFn = (GetStreamingModule_t)GetProcAddress(streamingDll,
            "?GetStreamingModule@strStreamingModuleMgr@streaming@@QEAAPEAVstrStreamingModule@2@PEBD@Z");
    if (!g_getRawEntriesFn)
        g_getRawEntriesFn = (GetRawEntries_t)GetProcAddress(streamingDll,
            "?GetPgRawStreamerEntries@rage@@YAAEBU?$chunkyArray@URawEntry@fiCollection@rage@@$0EAA@$0EA@@1@XZ");
}

// What a loaded slot really costs. The cost audit at scan time can only read the RSC7 page flags
// out of the file on disk, which is the file's own size and nothing more; a clothing .ydd that
// drags three shared texture dictionaries in with it is charged for none of them there. FiveM's
// devtools exports a walk of the dependency tree, so once a slot is resident a better figure is
// one call away.
//
// It only walks HALF of it, and that is not obvious from the name. Disassembled at
// devtools-five+0x332A0: CountDependencyMemory seeds its accumulator from
// gta-streaming-five!StreamingDataEntry::ComputeVirtualSize and then recurses over
// GetDependencies adding more of the same. Virtual is system memory. Texture pixels live in
// PHYSICAL memory, so every texture dictionary came back as ~0 and the HEAVY IN MEMORY warning
// below could never once fire for a .ytd, which is the file type every texture-loss report is
// actually about: mp_fm_skin_f_up_whi.ytd audits at 16.0 MB from its page flags and reported
// 0.0 MB loaded. ComputePhysicalSize is exported beside it, same this-is-the-entry calling
// shape, so ask for both and report them apart the way the scan audit already does.
//
// ponytail: physical is the entry's OWN pages; the dependency recursion is virtual-only because
// GetDependencies sits behind a build-dependent vtable offset we do not have a pattern for. A
// .ytd is exact (it has no dependencies), a drawable is charged for its own vertex buffers but
// not for the pixels of texture dictionaries it shares. Walk it properly if a shared-texture
// report ever needs the difference.
//
// Read-only: it looks up the streaming module (moduleMgr at +0x1B8, the offset Cfx's Streaming.h
// uses too), asks it for the dependency list, and recurses. SEH because it walks game-owned
// arrays that a mid-eviction slot can leave half valid.
typedef uint64_t (*CountDepMem_t)(void* mgr, uint32_t idx);
typedef uint64_t (*ComputePhysSize_t)(void* entry, uint32_t idx);   // __thiscall: entry is `this`
static CountDepMem_t g_countDepMemFn = nullptr;
static ComputePhysSize_t g_physSizeFn = nullptr;
static bool g_countDepTried = false;

void slotMemoryCost(uint32_t id, uint64_t* virt, uint64_t* phys)
{
    if (virt) *virt = 0;
    if (phys) *phys = 0;
    if (!g_countDepTried) {
        g_countDepTried = true;
        g_countDepMemFn = (CountDepMem_t)cfxSymbol("devtools-five.dll",
            "?CountDependencyMemory@@YA_KPEAVManager@streaming@@I@Z");
        g_physSizeFn = (ComputePhysSize_t)cfxSymbol("gta-streaming-five.dll",
            "?ComputePhysicalSize@StreamingDataEntry@@QEAA_KI@Z");
    }
    if (!validStreamingId(id)) return;
    __try {
        if (virt && g_countDepMemFn && g_getStreamingManagerFn) {
            void* mgr = g_getStreamingManagerFn();
            if (mgr) *virt = g_countDepMemFn(mgr, id);
        }
        if (phys && g_physSizeFn) {
            StrMgr* manager = exportedManagerSafe();
            if (manager && id < (uint32_t)manager->numEntries)
                *phys = g_physSizeFn(&manager->entries[id], id);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (virt) *virt = 0;
        if (phys) *phys = 0;
    }
}

// Contains no C++ objects so the virtual lookup can be isolated behind SEH. XBRVirtual inserts
// six methods at build 2802, making FindSlot vtable entry 8 there and entry 2 on older builds.
uint32_t findModuleSlotSafe(void* module, const char* stem, int build)
{
    __try {
        if (!module || !stem || !build) return 0xFFFFFFFF;
        void** vtable = *(void***)module;
        if (!vtable) return 0xFFFFFFFF;
        FindModuleSlot_t findSlot = (FindModuleSlot_t)vtable[build >= 2802 ? 8 : 2];
        if (!findSlot) return 0xFFFFFFFF;
        uint32_t local = 0xFFFFFFFF;
        findSlot(module, &local, stem);
        if (local == 0xFFFFFFFF) return local;
        uint32_t base = *(uint32_t*)((uint8_t*)module + 8);
        if (base > 0xFFFFFFFFu - local) return 0xFFFFFFFF;
        return base + local;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0xFFFFFFFF; }
}

// Holding a slot only decides what the game reads NEXT time. If it already loaded the texture
// from its own file, writing our handle back changes nothing on screen: the object stays
// resident and every reference keeps pointing at it, which is why the log has always had to say
// "your file shows only after the game drops and reloads it". Body skin is the worst case,
// because the ped's skin blend takes a reference on its source txds and holds it for the life of
// the ped, so "next time" may never come.
//
// So drop it ourselves. This is Cfx's own sequence, verbatim from LoadStreamingFile.cpp (~3455,
// the RemoveSlot path): the dependent count in bits 2..15 makes RemoveObject refuse, so clear
// that first, then ClearRequiredFlag, then RemoveObject. The slot itself survives, only the
// loaded object goes, and the next request for the name loads it through the handle we hold.
// RemoveSlot is deliberately NOT called: that frees the index and hands it to whatever registers
// next, which is exactly the corruption the 0.8.20 name check exists to catch.
//
// No SEH here, on purpose. These two calls mutate streaming state, and the plugin's standing
// rule is that catching a fault inside game code leaves the engine half updated and turns our
// bug into a crash somewhere else minutes later. Must run on the game thread; the caller
// journals the key first, so a fault costs one launch and quarantines the file.
typedef void (*RequestObject_t)(void* mgr, uint32_t id, int flags);
typedef bool (*ReleaseObject_t)(void* mgr, uint32_t id);
typedef bool (*ClearRequired_t)(void* mgr, uint32_t id, int flags);
static ReleaseObject_t g_releaseObjectFn = nullptr;
static ClearRequired_t g_clearRequiredFn = nullptr;
static bool g_dropTried = false;

bool dropExportsReady()
{
    if (!g_dropTried) {
        g_dropTried = true;
        g_releaseObjectFn = (ReleaseObject_t)cfxSymbol("gta-streaming-five.dll",
            "?ReleaseObject@Manager@streaming@@QEAA_NI@Z");            // RemoveObject
        g_clearRequiredFn = (ClearRequired_t)cfxSymbol("gta-streaming-five.dll",
            "?ReleaseObject@Manager@streaming@@QEAA_NIH@Z");           // ClearRequiredFlag
    }
    return g_releaseObjectFn && g_clearRequiredFn && g_getStreamingManagerFn;
}

int forceReloadSlot(uint32_t id, uint32_t ourHandle)
{
    if (!dropExportsReady()) return DROP_NO_EXPORTS;
    StrMgr* manager = exportedManagerSafe();
    if (!manager || !validStreamingId(id) || id >= (uint32_t)manager->numEntries) return DROP_BAD_SLOT;
    void* mgr = g_getStreamingManagerFn();
    if (!mgr) return DROP_BAD_SLOT;
    StrEntry& e = manager->entries[id];
    // Only a slot that is resident AND already carries our handle. Dropping one mid-load would
    // race the loader, and dropping one that does not point at us yet would just make the game
    // read its own file a second time.
    if ((e.flags & 3) != 1) return DROP_NOT_LOADED;
    if (e.handle != ourHandle) return DROP_NOT_OURS;
    if (e.flags & 0xFFFC) e.flags &= ~0xFFFC;   // dependent count, or RemoveObject refuses
    g_clearRequiredFn(mgr, id, 0xF1);
    g_releaseObjectFn(mgr, id);
    return DROP_OK;
}

const char* dropWhyText(int w)
{
    switch (w) {
    case DROP_OK:         return "dropped";
    case DROP_NO_EXPORTS: return "FiveM's streaming exports are missing";
    case DROP_BAD_SLOT:   return "the slot index is out of range";
    case DROP_NOT_LOADED: return "it is no longer in memory";
    default:              return "the slot stopped pointing at your file";
    }
}

const char* slotWhyText(int w)
{
    switch (w) {
    case SLOT_OK:         return "ok";
    case SLOT_NO_EXPORTS: return "FiveM's streaming exports are missing, so no name can be looked up";
    case SLOT_BAD_NAME:   return "the key has no usable extension";
    case SLOT_NO_MANAGER: return "the streaming manager came back null";
    case SLOT_NO_MODULE:  return "the game has no streaming module for this file type";
    default:              return "the module does not know that name";
    }
}

uint32_t targetStreamingId(const char* slot, int* why)
{
    auto fail = [&](int w) { if (why) *why = w; return 0xFFFFFFFFu; };
    if (!slot) return fail(SLOT_BAD_NAME);
    if (!g_getStreamingManagerFn || !g_getStreamingModuleFn) return fail(SLOT_NO_EXPORTS);
    const char* file = strrchr(slot, '/');
    file = file ? file + 1 : slot;
    const char* dot = strrchr(file, '.');
    if (!dot || dot == file || !dot[1]) return fail(SLOT_BAD_NAME);
    try {
        std::string stem(file, (size_t)(dot - file));
        std::string extension(dot + 1);
        void* manager = g_getStreamingManagerFn();
        if (!manager) return fail(SLOT_NO_MANAGER);
        // streaming::Manager::moduleMgr is at 0x1B8 in FiveM's exported Streaming.h layout.
        void* module = g_getStreamingModuleFn((uint8_t*)manager + 0x1B8, extension.c_str());
        if (!module) return fail(SLOT_NO_MODULE);
        uint32_t id = findModuleSlotSafe(module, stem.c_str(), runningGameBuild());
        // Slotted keys (folder/file) are also tried as "folder/stem" and "folder\stem": audio
        // banks are addressed as pack/bank and nobody knows which spelling the module keys on.
        // The log says which one hit, once per extension, so the answer lands in a user log.
        if (id == 0xFFFFFFFF && file != slot) {
            std::string folder(slot, (size_t)(file - slot - 1));
            const char* seps[] = { "/", "\\" };
            for (const char* sep : seps) {
                std::string alt = folder + sep + stem;
                id = findModuleSlotSafe(module, alt.c_str(), runningGameBuild());
                if (id != 0xFFFFFFFF) {
                    static std::set<std::string> said;
                    if (said.insert(extension).second)
                        LOG_INFO(LogCategory::Claim, "Slot lookup for .%s: the module keys on \"%s\" (folder%sname), not the bare name", extension.c_str(), alt.c_str(), sep);
                    break;
                }
            }
        }
        if (id == 0xFFFFFFFF) return fail(SLOT_NO_NAME);
        if (why) *why = SLOT_OK;
        return id;
    }
    catch (...) { return fail(SLOT_NO_MANAGER); }
}

void noteSlotWhy(const char* slot, int why)
{
    const char* dot = strrchr(slot, '.');
    if (!dot || !dot[1]) return;
    std::string ext = lower(dot + 1);
    auto it = g_slotWhyByExt.find(ext);
    if (it != g_slotWhyByExt.end() && it->second == why) return;
    g_slotWhyByExt[ext] = why;
    LOG_INFO(LogCategory::Claim, "Slot lookup for .%s: %s", ext.c_str(), slotWhyText(why));
}

bool localRawHandle(const char* file, uint32_t* handle)
{
    if (!file || !handle || !g_getRawEntriesFn) return false;
    __try {
        const RawEntriesView* entries = g_getRawEntriesFn();
        if (!entries || entries->count > 65535) return false;
        for (uint32_t i = entries->count; i > 0; --i) {
            uint32_t index = i - 1;
            RawEntryView* chunk = entries->memory[index / 1024];
            if (!chunk) continue;
            const char* candidate = chunk[index % 1024].fileName;
            if (!candidate || _stricmp(candidate, file) != 0) continue;
            // Entry zero is the documented pgRawStreamer dummy and also encodes the manager's
            // empty-handle sentinel, so it can never be attached as an asset.
            if (index == 0) return false;
            *handle = index;       // original registerRawStreamingFile uses collection zero
            return true;
        }
        return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool validStreamingId(uint32_t id)
{
    // GetStreamingIndex-style APIs use zero for a miss; never write entry zero.
    return g_mgr && g_mgr->entries && id != 0 && id != 0xFFFFFFFF &&
           id < (uint32_t)g_mgr->numEntries;
}

StrMgr* exportedManagerSafe()
{
    if (!g_getStreamingManagerFn) return nullptr;
    __try {
        StrMgr* manager = (StrMgr*)g_getStreamingManagerFn();
        if (!manager || !manager->entries || manager->numEntries <= 0 || manager->numEntries > 10000000)
            return nullptr;
        return manager;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

int recoverOccupiedSlot(Ov& ov)
{
    resolveOccupiedSlotExports();
    if (!g_mgr) g_mgr = exportedManagerSafe();
    uint32_t rawHandle = 0;
    if (!localRawHandle(ov.gfile ? ov.gfile : ov.file, &rawHandle)) return OCCUPIED_FAILED;
    ov.handle = rawHandle;

    int why = SLOT_OK;
    uint32_t target = targetStreamingId(ov.slot, &why);
    noteSlotWhy(ov.slot, why);   // "no module for this type" vs "does not know that name" is the whole diagnosis
    if (!validStreamingId(target)) {
        ov.id = 0xFFFFFFFF;
        return OCCUPIED_WAITING;
    }
    ov.id = target;
    __try {
        StrEntry& entry = g_mgr->entries[target];
        if (entry.handle != ov.handle && (entry.flags & 3) < 2) entry.handle = ov.handle;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return OCCUPIED_FAILED; }
    return OCCUPIED_ATTACHED;
}
