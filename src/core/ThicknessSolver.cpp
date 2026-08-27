#include "core/ThicknessSolver.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace BPR::Core
{
    namespace
    {
        struct Candidate
        {
            BoundaryHit hit;
            float distance{ 0.0F };
            std::uint8_t identity{ 0 };
        };

        struct CandidateBuffer
        {
            std::array<Candidate, kMaximumBoundaryHits> data{};
            std::size_t size{ 0 };
        };

        std::uint8_t Identity(const SurfaceEntry& entry, const BoundaryHit& hit) noexcept
        {
            if (entry.body != 0 && entry.body == hit.body) {
                return 2;
            }
            if (entry.owner != 0 && entry.owner == hit.owner) {
                return 1;
            }
            return 0;
        }

        CandidateBuffer Collect(
            const SurfaceEntry& entry,
            float maximumDepth,
            std::span<const BoundaryHit> hits,
            const ThicknessSettings& settings) noexcept
        {
            CandidateBuffer output;
            const auto direction = Normalize(entry.incoming);
            if (!direction || !std::isfinite(maximumDepth) || maximumDepth <= settings.entrySeparation) {
                return output;
            }

            for (const BoundaryHit& hit : hits) {
                if (output.size == output.data.size() || !IsFinite(hit.point) || !IsFinite(hit.normal)) {
                    continue;
                }
                const float distance = Dot(Subtract(hit.point, entry.point), *direction);
                const auto normal = Normalize(hit.normal);
                if (!normal || !std::isfinite(distance) || distance < settings.entrySeparation ||
                    distance > maximumDepth || Dot(*normal, *direction) < settings.outwardAlignment) {
                    continue;
                }
                const std::uint8_t identity = Identity(entry, hit);
                if (identity == 0) {
                    continue;
                }
                output.data[output.size++] = Candidate{ hit, distance, identity };
            }

            std::sort(output.data.begin(), output.data.begin() + output.size,
                [](const Candidate& left, const Candidate& right) {
                    if (left.distance != right.distance) {
                        return left.distance < right.distance;
                    }
                    if (left.identity != right.identity) {
                        return left.identity > right.identity;
                    }
                    if (left.hit.body != right.hit.body) {
                        return left.hit.body < right.hit.body;
                    }
                    return left.hit.shapeKey < right.hit.shapeKey;
                });

            std::size_t write = 0;
            for (std::size_t read = 0; read < output.size; ++read) {
                if (write != 0 &&
                    std::fabs(output.data[read].distance - output.data[write - 1].distance) <=
                        settings.duplicateSeparation) {
                    if (output.data[read].identity > output.data[write - 1].identity) {
                        output.data[write - 1] = output.data[read];
                    }
                    continue;
                }
                output.data[write++] = output.data[read];
            }
            output.size = write;
            return output;
        }
    }

    ThicknessSolution SolveThickness(
        const SurfaceEntry& entry,
        float maximumDepth,
        std::span<const BoundaryHit> forwardHits,
        std::span<const BoundaryHit> reverseHits,
        const ThicknessSettings& settings) noexcept
    {
        ThicknessSolution result;
        const CandidateBuffer forward = Collect(entry, maximumDepth, forwardHits, settings);
        const CandidateBuffer reverse = Collect(entry, maximumDepth, reverseHits, settings);
        result.forwardCandidates = forward.size;
        result.reverseCandidates = reverse.size;
        if (forward.size == 0 && reverse.size == 0) {
            return result;
        }

        const Candidate* selected = forward.size != 0 ? &forward.data[0] : &reverse.data[0];
        result.confidence = selected->identity == 2 ?
            ThicknessConfidence::kBodyMatch : ThicknessConfidence::kOwnerMatch;

        for (std::size_t index = 0; index < reverse.size; ++index) {
            if (std::fabs(reverse.data[index].distance - selected->distance) <= settings.reverseAgreement &&
                (reverse.data[index].hit.body == selected->hit.body ||
                    reverse.data[index].hit.owner == selected->hit.owner)) {
                result.confidence = ThicknessConfidence::kReverseCorroborated;
                break;
            }
        }

        result.found = true;
        result.thickness = selected->distance;
        result.exitPoint = selected->hit.point;
        result.exitNormal = selected->hit.normal;
        return result;
    }
}
