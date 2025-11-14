#pragma once

#include <functional>

namespace FarHorizon {

// Math utility functions (from Minecraft's Mth.java)
class Mth {
public:
    // Binary search (Mth.java line 514)
    // Searches for the first index where the condition returns false
    // Returns an index in range [from, to]
    template<typename Predicate>
        static int binarySearch(int from, int to, Predicate condition) {
        int var3 = to - from;

        while (var3 > 0) {
            int var4 = var3 / 2;
            int var5 = from + var4;
            if (condition(var5)) {
                var3 = var4;
            } else {
                from = var5 + 1;
                var3 -= var4 + 1;
            }
        }

        return from;
    }
};

} // namespace FarHorizon