/*
 * Jarari's Penetration System documented the need for an all-hit Fallout 4
 * physics pick and shooter collision-group filtering. BPR uses those findings
 * as attributed documentation, but constructs its mask from runtime collision
 * layer records and converts hit positions from the ray fraction. It does not
 * use Penetration System's collision-table relocations, masks, or loaded-data
 * offsets.
 */

#include "engine/RayQuery.h"

#include "pch.h"

#include <algorithm>

namespace BPR::Engine
{
    namespace
    {
        constexpr std::uint64_t kSafeWorldLayers =
            (1ULL << 1U) | (1ULL << 2U) | (1ULL << 3U) | (1ULL << 4U) |
            (1ULL << 9U) | (1ULL << 10U) | (1ULL << 11U) | (1ULL << 13U) |
            (1ULL << 17U) | (1ULL << 19U) | (1ULL << 20U) | (1ULL << 26U) |
            (1ULL << 27U) | (1ULL << 28U) | (1ULL << 29U) | (1ULL << 30U) |
            (1ULL << 32U) | (1ULL << 33U) | (1ULL << 35U) |
            0xFFFF000000000000ULL;

        std::uint64_t CollisionLayers(const RE::BGSProjectile& projectileBase) noexcept
        {
            std::uint64_t mask = 0;
            if (const RE::BGSCollisionLayer* source = projectileBase.data.collisionLayer) {
                for (const RE::BGSCollisionLayer* layer : source->collidesWith) {
                    if (layer && layer->collisionIdx < 64) {
                        mask |= 1ULL << layer->collisionIdx;
                    }
                }
            }
            return mask != 0 ? mask : kSafeWorldLayers;
        }

        std::uint32_t ShooterGroup(RE::Actor* shooter) noexcept
        {
            return shooter && shooter->loadedData ? shooter->GetCurrentCollisionGroup() : 6U;
        }

        std::uintptr_t BodyID(const RE::hknpBodyId& body) noexcept
        {
            return body.m_value == 0x7FFFFFFFU ? 0 :
                static_cast<std::uintptr_t>(body.m_value) + 1U;
        }

        RE::bhkNPCollisionObject* CollisionObjectForBody(
            RE::bhkWorld* world,
            RE::hknpBodyId& body) noexcept
        {
            if (!world || body.m_value == 0x7FFFFFFFU) {
                return nullptr;
            }
            if (REX::FModule::IsRuntimeOG()) {
                return RE::bhkNPCollisionObject::Getbhk(world, body);
            }

            // Fallout 4 1.11.x changed this function's first parameter from the
            // outer bhkWorld wrapper to its inner hknpBSWorld. The current
            // CommonLib declaration retains the old signature even though its
            // NG/AE relocation resolves to the new function. Adapt the ABI here
            // instead of letting the engine interpret bhkWorld as hknpBSWorld.
            RE::hknpBSWorld* worldNP = world->m_worldNP.get();
            if (!worldNP) {
                return nullptr;
            }
            using function_type = RE::bhkNPCollisionObject* (*)(
                RE::hknpBSWorld*, RE::hknpBodyId&);
            static REL::Relocation<function_type> function{
                RE::ID::bhkNPCollisionObject::Getbhk
            };
            return function(worldNP, body);
        }
    }

    std::uintptr_t CollisionBodyIdentity(
        const RE::bhkNPCollisionObject* collisionObject) noexcept
    {
        if (!collisionObject) {
            return 0;
        }
        const RE::bhkPhysicsSystem* wrapper = collisionObject->system.get();
        const RE::hknpPhysicsSystem* system = wrapper ? wrapper->systemInstance.get() : nullptr;
        if (!system || !system->m_bodyIds.data()) {
            return 0;
        }
        const std::uint32_t index = collisionObject->systemBodyIdx;
        if (index >= static_cast<std::uint32_t>(std::max(system->m_bodyIds.size(), 0))) {
            return 0;
        }
        return BodyID(system->m_bodyIds[index]);
    }

    BoundaryBatch QueryBoundaries(
        RE::Projectile& projectile,
        RE::Actor* shooter,
        RE::BGSProjectile& projectileBase,
        const RE::NiPoint3& start,
        const RE::NiPoint3& end,
        bool diagnostics) noexcept
    {
        BoundaryBatch output;
        RE::TESObjectCELL* cell = projectile.parentCell;
        RE::bhkWorld* world = cell ? cell->GetbhkWorld() : nullptr;
        if (!cell || !world || !world->m_worldNP) {
            return output;
        }

        RE::bhkPickData pick;
        RE::hknpAllHitsCollector collector;
        pick.Reset();
        pick.SetStartEnd(start, end);
        const std::uint64_t layers = CollisionLayers(projectileBase);
        const std::uint32_t group = ShooterGroup(shooter);
        pick.customCollideLayers = layers;
        pick.castQuery.m_filterData.m_collisionFilterInfo.storage = group << 16U;
        pick.collector = &collector;
        pick.collectorType = static_cast<RE::bhkPickData::COLLECTOR_TYPE>(0);
        const bool returnedObject = cell->Pick(pick) != nullptr;

        const std::int32_t hitCount = std::max(collector.m_hits.size(), 0);
        output.completed = hitCount > 0;
        std::array<RE::hknpCollisionResult, Core::kMaximumBoundaryHits> nearest{};
        std::size_t nearestCount = 0;
        for (std::int32_t index = 0; index < hitCount; ++index) {
            const RE::hknpCollisionResult& hit = collector.m_hits[index];
            if (nearestCount < nearest.size()) {
                nearest[nearestCount++] = hit;
                continue;
            }
            ++output.dropped;
            std::size_t farthest = 0;
            for (std::size_t candidate = 1; candidate < nearestCount; ++candidate) {
                if (nearest[candidate].fraction.storage > nearest[farthest].fraction.storage) {
                    farthest = candidate;
                }
            }
            if (hit.fraction.storage < nearest[farthest].fraction.storage) {
                nearest[farthest] = hit;
            }
        }

        for (std::size_t index = 0; index < nearestCount; ++index) {
            const RE::hknpCollisionResult& hit = nearest[index];
            RE::hknpBodyId bodyID = hit.hitBodyInfo.bodyId;
            RE::bhkNPCollisionObject* collision = CollisionObjectForBody(world, bodyID);
            RE::TESObjectREFR* owner = collision && collision->sceneObject ?
                RE::TESObjectREFR::FindReferenceFor3D(collision->sceneObject) : nullptr;
            const float fraction = std::clamp(hit.fraction.storage, 0.0F, 1.0F);
            output.hits[output.count++] = {
                .point = {
                    start.x + (end.x - start.x) * fraction,
                    start.y + (end.y - start.y) * fraction,
                    start.z + (end.z - start.z) * fraction
                },
                .normal = { hit.normal.x, hit.normal.y, hit.normal.z },
                .body = BodyID(bodyID),
                .owner = reinterpret_cast<std::uintptr_t>(owner),
                .shapeKey = hit.hitBodyInfo.shapeKey.storage
            };
        }
        pick.collector = nullptr;
        if (diagnostics) {
            REX::INFO(
                "[BPR-DIAG] ray query returned={} collected={} retained={} dropped={} layers={:016X} group={}",
                returnedObject, hitCount, output.count, output.dropped, layers, group);
        }
        return output;
    }
}
