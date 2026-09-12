// Checks ctlPath, the _settings.txt parser, and marker-file migration: defaults, toggles,
// comments, junk lines, legacy markers, that this launch still behaves as the last one did,
// that migration deletes the markers, and that a deleted key is appended rather than lost.
#include "core/utils.h"
#include "core/settings.h"
#include "core/state.h"
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <string>

static std::string dir;

static void touch(const char* n) { FILE* f=nullptr; fopen_s(&f,(dir+n).c_str(),"wb"); if(f) fclose(f); }
static void put(const char* n, const char* body)
{ FILE* f=nullptr; fopen_s(&f,(dir+n).c_str(),"wb"); if(f){fputs(body,f);fclose(f);} }
static bool there(const char* n)
{ return GetFileAttributesA((dir+n).c_str()) != INVALID_FILE_ATTRIBUTES; }
static std::string slurp(const char* n)
{ FILE* f=nullptr; std::string o; if(!fopen_s(&f,(dir+n).c_str(),"rb")&&f){char b[256];
  while(fgets(b,sizeof b,f)) o+=b; fclose(f);} return o; }
static bool has(const std::string& hay, const char* needle)
{ return hay.find(needle) != std::string::npos; }
static void wipe()
{ WIN32_FIND_DATAA fd; HANDLE h=FindFirstFileA((dir+"*").c_str(),&fd);
  if(h==INVALID_HANDLE_VALUE) return;
  do { if(!(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) DeleteFileA((dir+fd.cFileName).c_str()); }
  while(FindNextFileA(h,&fd)); FindClose(h); }

// one launch: Setup() reads, then the beat thread migrates
static void launch() { g_set = Settings(); loadSettings(); migrateSettings(); }
static void readOnly() { g_set = Settings(); loadSettings(); }

int main()
{
    char tmp[MAX_PATH]; GetTempPathA(MAX_PATH,tmp);
    dir = std::string(tmp) + "settest\\";
    CreateDirectoryA(dir.c_str(), nullptr);
    _snprintf_s(g_overrideDir, MAX_PATH, _TRUNCATE, "%s", dir.c_str());

    // 1. nothing there: everything off, and the first launch leaves a file behind
    wipe(); launch();
    assert(!g_set.off && !g_set.autoUpdate);
    assert(there("_settings.txt"));
    readOnly();
    assert(g_set.budget == "auto");

    // 2. an existing file is never rewritten, so notes in it survive
    wipe(); put("_settings.txt", "# my own note\nauto_update = yes\n"); launch();
    assert(g_set.autoUpdate && has(slurp("_settings.txt"), "# my own note"));

    // 3. comments, blank lines, junk, spacing, capitals, every truthy spelling
    wipe();
    put("_settings.txt",
        "# off = yes   <- a note, must be ignored\r\n"
        "\r\n"
        "   OFF   =   On   \r\n"
        "no equals sign here\r\n"
        "AUTO_UPDATE=TRUE\r\n"
        "texture_budget =  12 \r\n"
        "unknown_key = yes\r\n");
    readOnly();
    assert(g_set.off && g_set.autoUpdate);
    assert(g_set.budget == "12");

    // 4. anything not truthy is off
    wipe(); put("_settings.txt", "off = no\nauto_update = maybe\n"); readOnly();
    assert(!g_set.off && !g_set.autoUpdate);

    // 5. markers still apply on the launch that migrates them, bare and .txt; a marker for an
    //    option that no longer exists (_verbose) is deleted and writes nothing
    wipe(); touch("_verbose.txt"); touch("_auto_update.txt"); launch();
    assert(g_set.autoUpdate);
    // ...and they are gone afterwards, with the live value now in the file
    assert(!there("_verbose.txt") && !there("_auto_update.txt"));
    assert(there("_settings.txt"));
    assert(!has(slurp("_settings.txt"), "debug"));
    readOnly();
    assert(g_set.autoUpdate);   // second launch, no markers, same behaviour

    // 6. _budget carries its number across into texture_budget
    wipe(); put("_budget.txt", "8"); launch();
    assert(!there("_budget.txt"));
    readOnly();
    assert(g_set.budget == "8");

    // 7. migration overwrites a contradicting line rather than leaving two answers
    wipe(); put("_settings.txt", "auto_update = no\n"); touch("_auto_update"); launch();
    assert(!there("_auto_update"));
    readOnly();
    assert(g_set.autoUpdate);

    // 8. a key the user deleted out of the file is appended, not lost
    wipe(); put("_settings.txt", "# nothing but a note\n"); touch("_auto_update"); launch();
    assert(has(slurp("_settings.txt"), "auto_update = yes"));
    assert(has(slurp("_settings.txt"), "# nothing but a note"));
    readOnly();
    assert(g_set.autoUpdate);

    // 9. _off migrates like the rest when the plugin is actually running
    wipe(); touch("_off"); launch();
    assert(!there("_off"));
    readOnly();
    assert(g_set.off);

    // 10. migration is idempotent: a second run changes nothing
    std::string before = slurp("_settings.txt");
    launch();
    assert(slurp("_settings.txt") == before);

    // 11. every spelling vkFromName takes (the hide_overlay keys)
    assert(vkFromName("f11") == VK_F11);
    assert(vkFromName("F1")  == VK_F1);
    assert(vkFromName("f12") == VK_F12);
    assert(vkFromName("k")   == 'K');
    assert(vkFromName("5")   == '5');
    assert(vkFromName("off") == 0);
    assert(vkFromName("")    == 0);
    assert(vkFromName("f13") == -1);                   // there is no VK_F13 worth guessing at
    assert(vkFromName("ctrl+r") == -1);
    assert(vkFromName("printscreen") == VK_SNAPSHOT);
    assert(vkFromName("prntscrn")    == VK_SNAPSHOT);   // however they spell it
    assert(vkFromName("PrtSc")       == VK_SNAPSHOT);

    // 12. hide_overlay: absent means FiveM's overlays are left alone, and the value is kept raw
    wipe(); launch();
    assert(g_set.hideOverlay.empty());
    wipe(); put("_settings.txt", "hide_overlay = PrintScreen, F9\n"); readOnly();
    assert(g_set.hideOverlay == "printscreen, f9");

    // 13. a dropped option is taken out of an existing file, note and all, and nothing else moves
    wipe();
    put("_settings.txt",
        "off = no\n"
        "\n"
        "\n"
        "# Key that rescans tex_overrides straight away.\n"
        "#\n"
        "#   f1 to f12, or a single letter or digit\n"
        "refresh_key = f11\n"
        "\n"
        "\n"
        "# my own note\n"
        "hide_overlay = always\n");
    launch();
    {
        std::string s = slurp("_settings.txt");
        assert(s.find("refresh_key") == std::string::npos);   // the line
        assert(s.find("rescans")     == std::string::npos);   // and the note above it
        assert(s.find("off = no")           != std::string::npos);
        assert(s.find("# my own note")      != std::string::npos);   // the user's own words survive
        assert(s.find("hide_overlay = always") != std::string::npos);
        std::string again = s;
        launch();
        assert(slurp("_settings.txt") == again);              // and it only happens once
    }
    readOnly();
    assert(g_set.hideOverlay == "always");

    // 14. the three options 0.8.26 removed come out of an existing file, notes and all, and the
    //     marker files for two of them are deleted without writing anything back
    wipe();
    put("_settings.txt",
        "off = no\n"
        "\n"
        "\n"
        "# Write extra detail into texoverride.log.\n"
        "debug = no\n"
        "\n"
        "\n"
        "auto_update = yes\n"
        "\n"
        "\n"
        "# Make your files win a slot the game already filled.\n"
        "force_reload = yes\n"
        "\n"
        "\n"
        "# Never check whether a new version is out.\n"
        "no_update_check = yes\n");
    touch("_debug"); touch("_no_update_check.txt");
    launch();
    {
        std::string s = slurp("_settings.txt");
        for (const char* gone : { "debug", "force_reload", "no_update_check", "extra detail", "already filled", "new version is out" })
            assert(s.find(gone) == std::string::npos);
        assert(s.find("off = no") != std::string::npos);
        assert(s.find("auto_update = yes") != std::string::npos);
        assert(!there("_debug") && !there("_no_update_check.txt"));
        std::string again = s;
        launch();
        assert(slurp("_settings.txt") == again);
    }
    readOnly();
    assert(g_set.autoUpdate && !g_set.off);
    // a fresh file never mentions them either
    wipe();
    launch();
    for (const char* gone : { "debug", "force_reload", "no_update_check" })
        assert(slurp("_settings.txt").find(gone) == std::string::npos);

    wipe(); RemoveDirectoryA(dir.c_str());
    puts("settings: 14 groups passed");
    return 0;
}
