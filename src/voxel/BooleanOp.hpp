#pragma once

namespace FarHorizon {

// BooleanOp enum (from Minecraft)
// Defines boolean operations for combining voxel shapes
enum class BooleanOp {
    FALSE,        // Always false
    NOT_OR,       // !first && !second (NOR)
    ONLY_SECOND,  // second && !first
    NOT_FIRST,    // !first
    ONLY_FIRST,   // first && !second
    NOT_SECOND,   // !second
    NOT_SAME,     // first != second (XOR)
    NOT_AND,      // !first || !second (NAND)
    AND,          // first && second
    SAME,         // first == second (XNOR)
    SECOND,       // second
    CAUSES,       // !first || second (implies)
    FIRST,        // first
    CAUSED_BY,    // first || !second
    OR,           // first || second
    TRUE          // Always true
};

// Apply boolean operation (Minecraft: boolean apply(boolean first, boolean second))
inline bool applyBooleanOp(BooleanOp op, bool first, bool second) {
    switch (op) {
        case BooleanOp::FALSE:
            return false;
        case BooleanOp::NOT_OR:
            return !first && !second;
        case BooleanOp::ONLY_SECOND:
            return second && !first;
        case BooleanOp::NOT_FIRST:
            return !first;
        case BooleanOp::ONLY_FIRST:
            return first && !second;
        case BooleanOp::NOT_SECOND:
            return !second;
        case BooleanOp::NOT_SAME:
            return first != second;
        case BooleanOp::NOT_AND:
            return !first || !second;
        case BooleanOp::AND:
            return first && second;
        case BooleanOp::SAME:
            return first == second;
        case BooleanOp::SECOND:
            return second;
        case BooleanOp::CAUSES:
            return !first || second;
        case BooleanOp::FIRST:
            return first;
        case BooleanOp::CAUSED_BY:
            return first || !second;
        case BooleanOp::OR:
            return first || second;
        case BooleanOp::TRUE:
            return true;
        default:
            return false;
    }
}

} // namespace FarHorizon