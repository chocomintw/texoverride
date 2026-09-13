// Proves the vendored MinHook still installs, removes and re-installs a patch after the
// thread-freeze machinery was deleted from minhook/src/hook.c. That deletion rewrote the four
// places that used to wrap EnableHookLL in Freeze/Unfreeze, which is exactly the control flow
// every one of these calls walks:
//
//   MH_EnableHook(target)   -> EnableHook        (the site inside FindHookEntry)
//   MH_EnableHook(ALL)      -> EnableAllHooksLL  (the ALL_HOOKS site)
//   MH_ApplyQueued()        -> the queued site
//   MH_RemoveHook(target)   -> the disable-then-delete site
//
// Run from tools\run_tests.bat. Nothing here touches the game, so it runs anywhere.

#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include "MinHook.h"

static volatile int g_sink = 0;

// Long enough that MinHook has a real prologue to patch rather than a 4-byte leaf, and
// noinline so the calls below actually go through the patched entry point.
__declspec(noinline) static int target(int x)
{
    int acc = 0;
    for (int i = 0; i < 2; ++i) {
        g_sink += i;
        acc += x;
    }
    return acc;
}

typedef int (*targetFn)(int);
static targetFn o_target = nullptr;

__declspec(noinline) static int detour(int x)
{
    return o_target(x) + 1000;
}

static int checks = 0;
static void eq(int got, int want, const char* what)
{
    ++checks;
    if (got != want) {
        printf("  FAIL %-34s got %d, want %d\n", what, got, want);
        exit(1);
    }
    printf("  ok   %-34s %d\n", what, got);
}

static void mh(MH_STATUS s, const char* what)
{
    ++checks;
    if (s != MH_OK) {
        printf("  FAIL %-34s %s\n", what, MH_StatusToString(s));
        exit(1);
    }
    printf("  ok   %-34s %s\n", what, MH_StatusToString(s));
}

int main()
{
    printf("test_minhook\n");

    eq(target(21), 42, "unhooked");

    mh(MH_Initialize(), "MH_Initialize");
    mh(MH_CreateHook((LPVOID)&target, (LPVOID)&detour, (LPVOID*)&o_target), "MH_CreateHook");
    eq(target(21), 42, "created but not enabled");

    // EnableHook, the single-target site
    mh(MH_EnableHook((LPVOID)&target), "MH_EnableHook");
    eq(target(21), 1042, "detour runs");
    eq(o_target(21), 42, "trampoline reaches the original");

    mh(MH_DisableHook((LPVOID)&target), "MH_DisableHook");
    eq(target(21), 42, "patch backed out");

    // EnableAllHooksLL, the ALL_HOOKS site
    mh(MH_EnableHook(MH_ALL_HOOKS), "MH_EnableHook(ALL)");
    eq(target(21), 1042, "detour runs after enable-all");
    mh(MH_DisableHook(MH_ALL_HOOKS), "MH_DisableHook(ALL)");
    eq(target(21), 42, "backed out after disable-all");

    // the queued site
    mh(MH_QueueEnableHook((LPVOID)&target), "MH_QueueEnableHook");
    eq(target(21), 42, "queue alone changes nothing");
    mh(MH_ApplyQueued(), "MH_ApplyQueued");
    eq(target(21), 1042, "detour runs after apply");

    // the remove site: removes while still enabled, so it disables on the way out
    mh(MH_RemoveHook((LPVOID)&target), "MH_RemoveHook while enabled");
    eq(target(21), 42, "original restored after remove");

    mh(MH_Uninitialize(), "MH_Uninitialize");
    eq(target(21), 42, "still original after uninit");

    printf("test_minhook: %d checks passed\n", checks);
    return 0;
}
