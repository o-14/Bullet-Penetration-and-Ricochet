#pragma once

#include "core/Vector3.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace BPR::Core
{
    inline constexpr std::size_t kMaximumBoundaryHits = 48;

    struct BoundaryHit
    {
        Vector3 point;
        Vector3 normal;
        std::uintptr_t body{ 0 };
        std::uintptr_t owner{ 0 };
        std::uint32_t shapeKey{ 0 };
    };

    struct SurfaceEntry
    {
        Vector3 point;
        Vector3 incoming;
        std::uintptr_t body{ 0 };
        std::uintptr_t owner{ 0 };
        std::uint32_t shapeKey{ 0 };
    };

    enum class ThicknessConfidence : std::uint8_t
    {
        kNone,
        kOwnerMatch,
        kBodyMatch,
        kReverseCorroborated
    };

    struct ThicknessSettings
    {
        float entrySeparation{ 0.35F };
        float duplicateSeparation{ 0.10F };
        float outwardAlignment{ 0.015F };
        float reverseAgreement{ 0.35F };
    };

    struct ThicknessSolution
    {
        bool found{ false };
        float thickness{ 0.0F };
        Vector3 exitPoint;
        Vector3 exitNormal;
        ThicknessConfidence confidence{ ThicknessConfidence::kNone };
        std::size_t forwardCandidates{ 0 };
        std::size_t reverseCandidates{ 0 };
    };

    [[nodiscard]] ThicknessSolution SolveThickness(
        const SurfaceEntry& entry,
        float maximumDepth,
        std::span<const BoundaryHit> forwardHits,
        std::span<const BoundaryHit> reverseHits,
        const ThicknessSettings& settings = {}) noexcept;
}
