#pragma once

#include <functional>

namespace FarHorizon {

// Boolean binary function interface (from Minecraft's BooleanBiFunction.java)
// Used for voxel shape comparisons
class BooleanBiFunction {
public:
    using FunctionType = std::function<bool(bool, bool)>;

    // Predefined functions (BooleanBiFunction.java lines 4-19)
    static const FunctionType FALSE;
    static const FunctionType NOT_OR;
    static const FunctionType ONLY_SECOND;
    static const FunctionType NOT_FIRST;
    static const FunctionType ONLY_FIRST;  // a && !b - used for face culling
    static const FunctionType NOT_SECOND;
    static const FunctionType NOT_SAME;    // a != b - used for equality checks
    static const FunctionType NOT_AND;
    static const FunctionType AND;
    static const FunctionType SAME;
    static const FunctionType SECOND;
    static const FunctionType CAUSES;
    static const FunctionType FIRST;
    static const FunctionType CAUSED_BY;
    static const FunctionType OR;
    static const FunctionType TRUE;
};

// Static function definitions
inline const BooleanBiFunction::FunctionType BooleanBiFunction::FALSE = [](bool a, bool b) { return false; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::NOT_OR = [](bool a, bool b) { return !a && !b; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::ONLY_SECOND = [](bool a, bool b) { return b && !a; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::NOT_FIRST = [](bool a, bool b) { return !a; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::ONLY_FIRST = [](bool a, bool b) { return a && !b; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::NOT_SECOND = [](bool a, bool b) { return !b; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::NOT_SAME = [](bool a, bool b) { return a != b; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::NOT_AND = [](bool a, bool b) { return !a || !b; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::AND = [](bool a, bool b) { return a && b; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::SAME = [](bool a, bool b) { return a == b; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::SECOND = [](bool a, bool b) { return b; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::CAUSES = [](bool a, bool b) { return !a || b; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::FIRST = [](bool a, bool b) { return a; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::CAUSED_BY = [](bool a, bool b) { return a || !b; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::OR = [](bool a, bool b) { return a || b; };
inline const BooleanBiFunction::FunctionType BooleanBiFunction::TRUE = [](bool a, bool b) { return true; };

} // namespace FarHorizon