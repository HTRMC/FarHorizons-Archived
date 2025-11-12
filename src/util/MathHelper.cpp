#include "MathHelper.hpp"
#include <cmath>

namespace FarHorizon {

// Minecraft's approximatelyEquals for float (MathHelper.java line 148-150)
bool MathHelper::approximatelyEquals(float a, float b) {
    return std::abs(b - a) < EPSILON;
}

// Minecraft's approximatelyEquals for double (MathHelper.java line 152-154)
bool MathHelper::approximatelyEquals(double a, double b) {
    return std::abs(b - a) < EPSILON;
}

// Floor modulo (Minecraft's MathHelper.java line 156-158)
int MathHelper::floorMod(int dividend, int divisor) {
    int result = dividend % divisor;
    if ((result ^ divisor) < 0 && result != 0) {
        result += divisor;
    }
    return result;
}

} // namespace FarHorizon