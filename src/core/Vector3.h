#pragma once

#include <optional>
#include <cstdint>

namespace BPR::Core
{
    struct Vector3
    {
        float x{ 0.0F };
        float y{ 0.0F };
        float z{ 0.0F };
    };

    [[nodiscard]] bool IsFinite(Vector3 value) noexcept;
    [[nodiscard]] float Dot(Vector3 left, Vector3 right) noexcept;
    [[nodiscard]] Vector3 Add(Vector3 left, Vector3 right) noexcept;
    [[nodiscard]] Vector3 Subtract(Vector3 left, Vector3 right) noexcept;
    [[nodiscard]] Vector3 Scale(Vector3 value, float factor) noexcept;
    [[nodiscard]] std::optional<Vector3> Normalize(Vector3 value) noexcept;
    [[nodiscard]] std::optional<float> PlaneAngleDegrees(
        Vector3 incoming,
        Vector3 surfaceNormal) noexcept;
    [[nodiscard]] Vector3 ReflectedDirection(
        Vector3 incoming,
        Vector3 surfaceNormal) noexcept;
    [[nodiscard]] Vector3 DeterministicConeVariation(
        Vector3 direction,
        float maximumDegrees,
        std::uint64_t seed) noexcept;
    [[nodiscard]] float DeterministicUnitFloat(std::uint64_t seed) noexcept;
}
