// inSweepSlice: the round robin the beat uses to revalidate slot names a slice at a time.
// A slot nobody else writes is never name-checked by the mismatch path, so this sweep is the
// only thing that ever notices the store moved a name off an index we still hold. If it skips
// an override, that override is never checked again for the rest of the session.
// Build with tools\run_tests.bat.
#include "core/utils.h"
#include <cassert>
#include <cstdio>
#include <set>
#include <vector>

// Walk the cursor exactly as BeatLoop does and return which indices each beat visited.
static std::vector<std::vector<size_t>> sweep(size_t count, size_t budget, int beats)
{
    std::vector<std::vector<size_t>> out;
    size_t cursor = 0;
    for (int b = 0; b < beats; ++b) {
        size_t from = count ? cursor % count : 0;
        if (count) cursor = (from + budget) % count;
        std::vector<size_t> hit;
        for (size_t n = 0; n < count; ++n)
            if (inSweepSlice(n, count, from, budget)) hit.push_back(n);
        out.push_back(hit);
    }
    return out;
}

int main()
{
    // 1. a beat never checks more than its budget, so the cost per beat stays bounded
    for (size_t count : { (size_t)1, (size_t)5, (size_t)32, (size_t)335, (size_t)6000 })
        for (auto& hit : sweep(count, 32, 40))
            assert(hit.size() <= 32 && hit.size() <= count);

    // 2. every override is visited within one full sweep. This is the property that matters:
    //    miss one and its name is never revalidated again.
    for (size_t count : { (size_t)1, (size_t)7, (size_t)32, (size_t)33, (size_t)335, (size_t)6000 }) {
        size_t budget = 32;
        int beatsForFullSweep = (int)((count + budget - 1) / budget);
        std::set<size_t> seen;
        for (auto& hit : sweep(count, budget, beatsForFullSweep))
            for (size_t n : hit) seen.insert(n);
        assert(seen.size() == count);
    }

    // 3. the window wraps rather than clipping at the end of the list
    assert(inSweepSlice(0, 10, 8, 4));      // 8,9,0,1
    assert(inSweepSlice(1, 10, 8, 4));
    assert(inSweepSlice(8, 10, 8, 4));
    assert(inSweepSlice(9, 10, 8, 4));
    assert(!inSweepSlice(2, 10, 8, 4));
    assert(!inSweepSlice(7, 10, 8, 4));

    // 4. a budget at or over the list size checks everything every beat
    for (size_t n = 0; n < 5; ++n) assert(inSweepSlice(n, 5, 3, 32));

    // 5. an empty list is not a divide by zero
    assert(!inSweepSlice(0, 0, 0, 32));

    // 6. a cursor past the end of the list still lands in range (the beat mods it, but the
    //    helper must not depend on that)
    assert(inSweepSlice(0, 10, 40, 4));     // from 40 % 10 == 0
    assert(!inSweepSlice(5, 10, 40, 4));

    puts("reval: 6 groups passed");
    return 0;
}
