// One check for ctlPath: bare name, .txt name, neither, and "bare wins over .txt".
// Build+run: tools\run_tests.bat (runs every test in this folder).
#include "core/utils.h"
#include "core/state.h"
#include <windows.h>
#include <cassert>
#include <cstdio>

static void touch(const char* p) { FILE* f=nullptr; fopen_s(&f,p,"wb"); if(f) fclose(f); }

int main()
{
    char tmp[MAX_PATH]; GetTempPathA(MAX_PATH, tmp);
    _snprintf_s(g_overrideDir, MAX_PATH, _TRUNCATE, "%sctltest\\", tmp);
    CreateDirectoryA(g_overrideDir, nullptr);

    std::string bare = std::string(g_overrideDir) + "_off";
    std::string txt  = bare + ".txt";
    DeleteFileA(bare.c_str()); DeleteFileA(txt.c_str());

    assert(ctlPath("_off").empty());                 // neither exists

    touch(txt.c_str());
    assert(ctlPath("_off") == txt);                  // .txt form found
    DeleteFileA(txt.c_str());

    touch(bare.c_str());
    assert(ctlPath("_off") == bare);                 // bare form found

    touch(txt.c_str());
    assert(ctlPath("_off") == bare);                 // bare wins when both exist
    DeleteFileA(bare.c_str()); DeleteFileA(txt.c_str());

    assert(ctlPath("_off").empty());                 // back to neither
    RemoveDirectoryA(g_overrideDir);
    puts("ctlPath: 5 checks passed");
    return 0;
}
