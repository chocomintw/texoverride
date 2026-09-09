#pragma once

#include <string>

// One settings file. tex_overrides\_settings.txt is written on first run with every option
// listed, explained in plain English and switched off, so a user changes a word rather than
// guessing at a filename Explorer will not let them type.
//
// The old marker files (_off, _debug, _verbose, _budget, _auto_update, _no_update_check, with or
// without .txt) are still READ, but only once: the first launch that finds one folds its value
// into _settings.txt and deletes it. So an install converges on exactly one settings file and
// nothing else, and nobody ever has to be told to rename anything.
//
// Gone since 0.8.26, and stripped out of existing files: debug (the log always carries DEBUG
// detail), no_update_check (the check always runs and always says what it found) and
// force_reload (it dropped resident objects the game was still drawing, which crashed the game).
struct Settings
{
    bool off            = false;
    bool autoUpdate     = false;
    std::string budget;   // raw value; resolved in readBudgetFile, which can actually log
    std::string hideOverlay;   // comma separated screenshot keys, or "always"; empty = leave FiveM's overlays alone
};

extern Settings g_set;

int  vkFromName(const std::string& raw);   // "f11" -> VK_F11; 0 = off, -1 = not a key

void loadSettings();       // Setup(), under the loader lock: reads only, never writes, never logs
void migrateSettings();    // beat thread: create the file if missing, absorb markers, delete them
