#pragma once

#include <cstddef>
#include <string>

// Round robin over a list: is index `n` inside the `budget`-wide window starting at `from`?
// The window wraps, so one that runs off the end continues from the start and every index is
// still visited exactly once per sweep. Header-only so scratch\test_reval.bat can exercise the
// same expression the beat does rather than a copy of it.
inline bool inSweepSlice(size_t n, size_t count, size_t from, size_t budget)
{
    return count != 0 && ((n + count - (from % count)) % count) < budget;
}

std::string lower(std::string s);
std::string fwd(std::string s);
const char* rel(const char* file);
const char* toUtf8(const char* ansi);
bool hasExt(const std::string& k, const char* e);
std::string ctlPath(const char* name);
