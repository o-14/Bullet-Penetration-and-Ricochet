#include "core/Vector3.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace BPR::Core
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846F;
        constexpr float kLengthEpsilon = 1.0e-6F;

        Vector3 Cross(Vector3 left, Vector3 right) noexcept
        {
            return {
                left.y * right.z - left.z * right.y,
                left.z * right.x - left.x * right.z,
                left.x * right.y - left.y * right.x
            };
        }

        std::uint64_t Scramble(std::uint64_t value) noexcept
        {
            value ^= value >> 30U;
            value *= 0xBF58476D1CE4E5B9ULL;
            value ^= value >> 27U;
            value *= 0x94D049BB133111EBULL;
            return value ^ (value >> 31U);
        }

        float SignedUnit(std::uint64_t value) noexcept
        {
            const auto bits = static_cast<std::uint32_t>(Scramble(value) >> 40U);
            return static_cast<float>(bits) / 8388607.5F - 1.0F;
        }
    }

    bool IsFinite(Vector3 value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    float Dot(Vector3 left, Vector3 right) noexcept
    {
        return left.x * right.x + left.y * right.y + left.z * right.z;
    }

    Vector3 Add(Vector3 left, Vector3 right) noexcept
    {
        return { left.x + right.x, left.y + right.y, left.z + right.z };
    }

    Vector3 Subtract(Vector3 left, Vector3 right) noexcept
    {
        return { left.x - right.x, left.y - right.y, left.z - right.z };
    }

    Vector3 Scale(Vector3 value, float factor) noexcept
    {
        return { value.x * factor, value.y * factor, value.z * factor };
    }

    std::optional<Vector3> Normalize(Vector3 value) noexcept
    {
        if (!IsFinite(value)) {
            return std::nullopt;
        }
        const float lengthSquared = Dot(value, value);
        if (!std::isfinite(lengthSquared) || lengthSquared <= kLengthEpsilon * kLengthEpsilon) {
            return std::nullopt;
        }
        return Scale(value, 1.0F / std::sqrt(lengthSquared));
    }

    std::optional<float> PlaneAngleDegrees(Vector3 incoming, Vector3 surfaceNormal) noexcept
    {
        const auto direction = Normalize(incoming);
        auto normal = Normalize(surfaceNormal);
        if (!direction || !normal) {
            return std::nullopt;
        }
        if (Dot(*direction, *normal) > 0.0F) {
            *normal = Scale(*normal, -1.0F);
        }
        const float normalComponent = std::clamp(-Dot(*direction, *normal), 0.0F, 1.0F);
        return std::asin(normalComponent) * 180.0F / kPi;
    }

    Vector3 ReflectedDirection(Vector3 incoming, Vector3 surfaceNormal) noexcept
    {
        const auto direction = Normalize(incoming);
        auto normal = Normalize(surfaceNormal);
        if (!direction || !normal) {
            return {};
        }
        if (Dot(*direction, *normal) > 0.0F) {
            *normal = Scale(*normal, -1.0F);
        }
        return Normalize(Subtract(*direction, Scale(*normal, 2.0F * Dot(*direction, *normal))))
            .value_or(Vector3{});
    }

    Vector3 DeterministicConeVariation(Vector3 direction, float maximumDegrees, std::uint64_t seed) noexcept
    {
        const auto unit = Normalize(direction);
        if (!unit || !std::isfinite(maximumDegrees) || maximumDegrees <= 0.0F) {
            return unit.value_or(Vector3{});
        }

        const Vector3 reference = std::fabs(unit->z) < 0.8F ?
            Vector3{ 0.0F, 0.0F, 1.0F } : Vector3{ 0.0F, 1.0F, 0.0F };
        const auto tangent = Normalize(Cross(*unit, reference));
        if (!tangent) {
            return *unit;
        }
        const Vector3 bitangent = Cross(*unit, *tangent);
        const float radius = std::tan(std::clamp(maximumDegrees, 0.0F, 12.0F) * kPi / 180.0F);
        const float radial = std::sqrt(std::fabs(SignedUnit(seed ^ 0xA0761D6478BD642FULL))) * radius;
        const float azimuth = (SignedUnit(seed ^ 0xE7037ED1A0B428DBULL) + 1.0F) * kPi;
        const Vector3 offset = Add(
            Scale(*tangent, radial * std::cos(azimuth)),
            Scale(bitangent, radial * std::sin(azimuth)));
        return Normalize(Add(*unit, offset)).value_or(*unit);
    }

    float DeterministicUnitFloat(std::uint64_t seed) noexcept
    {
        const auto bits = static_cast<std::uint32_t>(Scramble(seed) >> 40U);
        return static_cast<float>(bits) / 16777216.0F;
    }
}
