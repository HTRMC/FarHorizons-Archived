#pragma once

namespace FarHorizon {

// Math utilities matching Minecraft's MathHelper.java
class MathHelper {
public:
    // Minecraft's approximatelyEquals implementation (MathHelper.java line 148-154)
    // Returns true if the difference between a and b is less than 1.0E-5F
    static bool approximatelyEquals(float a, float b);
    static bool approximatelyEquals(double a, double b);

    // Floor modulo (useful for world coordinates)
    static int floorMod(int dividend, int divisor);

private:
    // Epsilon value from Minecraft
    static constexpr float EPSILON = 1.0E-5F;
};

} // namespace FarHorizon