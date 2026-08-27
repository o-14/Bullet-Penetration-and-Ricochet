/*
 * Fallout 4 replacement-projectile launching and the fallback shooter repair
 * are informed by Penetration System by jarari and used under its MIT license.
 * BPR's typed launch-time shooter initialization, normal-aware placement,
 * validation, chain ownership, multi-runtime dispatch, and diagnostics are new.
 * Missile-style bodies retain the attributed swept-body clearance only where
 * runtime evidence shows the normal-aware placement immediately re-collides.
 */

#include "engine/ProjectileLauncher.h"

#include "core/ContinuationPlacement.h"
#include "core/Vector3.h"
#include "engine/ProjectileMotion.h"
#include "pch.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace BPR::Engine
{
    namespace
    {
        constexpr float kMinimumFraction = 0.0001F;

        struct LaunchRequest
        {
            explicit LaunchRequest(RE::BGSObjectInstanceT<RE::TESObjectWEAP> weapon) :
                data{ .fromWeapon = std::move(weapon) }
            {}

            RE::ProjectileLaunchData data;
            RE::ObjectRefHandle sourceShooter;
            RE::ObjectRefHandle desiredTarget;
            RE::ActorCause* actorCause{ nullptr };
            RE::EffectSetting* effect{ nullptr };
            Runtime::ChainState chain;
            Core::Vector3 impactOrigin;
            Core::Vector3 requestedDirection;
            Core::ContinuationPlacement placement;
            ContinuationKind kind{ ContinuationKind::kPenetration };
            float damage{ 0.0F };
            std::uint32_t projectileForm{ 0 };
            std::uint32_t ammoForm{ 0 };
            const RE::Projectile* source{ nullptr };
            bool deferShooterAtLaunch{ false };
            bool diagnostics{ false };
        };

        RE::BGSProjectile* BaseProjectile(RE::Projectile& projectile) noexcept
        {
            RE::TESBoundObject* base = projectile.GetObjectReference();
            return base ? base->As<RE::BGSProjectile>() : nullptr;
        }

        RE::ProjectileHandle EngineLaunch(const RE::ProjectileLaunchData& data)
        {
            using Function = RE::ProjectileHandle (*)(const RE::ProjectileLaunchData&);
            static REL::Relocation<Function> function{
                REL::VariantID(1452334, 2236958, 2236958)
            };
            return function(data);
        }

        std::optional<LaunchRequest> Capture(
            RE::Projectile& source,
            Core::Vector3 origin,
            Core::Vector3 direction,
            Core::Vector3 surfaceNormal,
            float relativePowerScale,
            float remainingFraction,
            float surfaceTolerance,
            const Runtime::ChainState& state,
            ContinuationKind kind,
            bool diagnostics) noexcept
        {
            const auto outgoing = Core::Normalize(direction);
            RE::BGSProjectile* base = BaseProjectile(source);
            if (!outgoing || !base || !source.parentCell || !source.ammoSource ||
                !std::isfinite(relativePowerScale) || relativePowerScale <= 0.0F ||
                !std::isfinite(remainingFraction) || remainingFraction <= kMinimumFraction) {
                return std::nullopt;
            }

            RE::TESObjectWEAP* weapon = source.weaponSource.object ?
                source.weaponSource.object->As<RE::TESObjectWEAP>() : nullptr;
            LaunchRequest request{ RE::BGSObjectInstanceT<RE::TESObjectWEAP>(
                weapon,
                source.weaponSource.instanceData.get()) };
            request.source = &source;
            request.sourceShooter = source.shooter;
            request.desiredTarget = source.desiredTarget;
            request.actorCause = source.GetActorCause();
            request.effect = source.avEffect;
            request.chain = state;
            request.impactOrigin = origin;
            request.requestedDirection = *outgoing;
            request.kind = kind;
            request.damage = source.damage;
            request.projectileForm = base->GetFormID();
            request.ammoForm = source.ammoSource->GetFormID();
            request.deferShooterAtLaunch = source.IsMissileProjectile();
            request.diagnostics = diagnostics;

            const float pitch = -std::asin(std::clamp(outgoing->z, -1.0F, 1.0F));
            const float yaw = std::atan2(outgoing->x, outgoing->y);
            request.placement = Core::PlaceContinuation(
                origin, *outgoing, surfaceNormal, base->data.collisionRadius, surfaceTolerance,
                request.deferShooterAtLaunch ?
                    (kind == ContinuationKind::kPenetration ?
                        Core::ContinuationPlacementMode::kMissilePenetration :
                        Core::ContinuationPlacementMode::kMissileRebound) :
                    Core::ContinuationPlacementMode::kSurfaceSeparated);
            request.data.origin = {
                request.placement.origin.x,
                request.placement.origin.y,
                request.placement.origin.z
            };
            if (kind == ContinuationKind::kRebound) {
                if (const auto normal = Core::Normalize(surfaceNormal)) {
                    request.data.contactNormal = { normal->x, normal->y, normal->z };
                }
            }
            request.data.projectileBase = base;
            if (auto* beam = RE::fallout_cast<RE::BeamProjectile*>(&source)) {
                request.data.area = beam->launchData.area;
                request.data.alwaysHit = beam->launchData.alwaysHit;
                request.data.noDamageOutsideCombat = beam->launchData.noDamageOutsideCombat;
                request.data.autoAim = beam->launchData.autoAim;
                request.data.intentionalMiss = beam->launchData.intentionalMiss;
            }
            if (!request.deferShooterAtLaunch) {
                if (auto shooter = request.sourceShooter.get()) {
                    request.data.shooter = shooter.get();
                    if (auto* actor = shooter->As<RE::Actor>()) {
                        request.data.shooterCombatController = actor->combatController;
                    }
                }
            }
            request.data.fromAmmo = source.ammoSource;
            request.data.equipIndex = source.equipIndex;
            request.data.xAngle = pitch;
            request.data.zAngle = yaw;
            request.data.parentCell = source.parentCell;
            request.data.spell = source.spell;
            request.data.castingSource = source.castingSource.get();
            if (auto* arrow = RE::fallout_cast<RE::ArrowProjectile*>(&source)) {
                request.data.poison = arrow->poison;
            }
            request.data.power = source.power * relativePowerScale;
            request.data.scale = std::isfinite(source.scale) && source.scale > 0.0F ? source.scale : 1.0F;
            request.data.targetLimb = source.targetLimb.get();
            request.data.coneOfFireRadiusMult = 0.0F;
            request.data.useOrigin = true;
            request.data.tracer = false;
            request.data.forceConeOfFire = true;
            request.data.allow3D = true;
            request.data.ignoreNearCollisions = true;
            return request;
        }

        bool Execute(LaunchRequest request) noexcept
        {
            if (auto target = request.desiredTarget.get()) {
                request.data.homingTarget = target.get();
            }
            RE::ProjectileHandle handle = EngineLaunch(request.data);
            auto reference = handle ? handle.get() : nullptr;
            RE::Projectile* projectile = reference ? reference->As<RE::Projectile>() : nullptr;
            if (!handle || !projectile) {
                return false;
            }
            const std::uint32_t handleValue = handle.get_handle();
            const bool sourceShooterPresent = static_cast<bool>(request.sourceShooter);
            const bool shooterPreserved = sourceShooterPresent &&
                projectile->shooter == request.sourceShooter;
            const RE::ObjectRefHandle deferredShooter =
                sourceShooterPresent &&
                    (request.deferShooterAtLaunch || !shooterPreserved) ?
                request.sourceShooter : RE::ObjectRefHandle{};
            if (auto shooter = request.sourceShooter.get()) {
                projectile->SetActorCause(shooter->GetActorCause());
            } else {
                projectile->SetActorCause(request.actorCause);
            }
            projectile->avEffect = request.effect;
            projectile->damage = request.damage;
            if (request.chain.remainingRange > 0.0F) {
                projectile->range = projectile->range > 0.0F ?
                    std::min(projectile->range, request.chain.remainingRange) : request.chain.remainingRange;
            }
            if (projectile->IsBeamProjectile()) {
                static_cast<RE::BeamProjectile*>(projectile)->flags &= ~0x20000000ULL;
            }
            if (!request.source) {
                return false;
            }
            const RE::NiPoint3 position = projectile->GetPosition();
            request.chain.launchPoint = {
                request.data.origin.x, request.data.origin.y, request.data.origin.z
            };
            request.chain.requestedDirection = request.requestedDirection;
            request.chain.validateForwardProgress = true;
            Runtime::RegisterContinuation(handleValue, request.chain, deferredShooter);

            if (request.diagnostics) {
                const bool contactNormalSet =
                    std::isfinite(request.data.contactNormal.x) &&
                    std::isfinite(request.data.contactNormal.y) &&
                    std::isfinite(request.data.contactNormal.z) &&
                    (std::abs(request.data.contactNormal.x) > 0.0001F ||
                     std::abs(request.data.contactNormal.y) > 0.0001F ||
                     std::abs(request.data.contactNormal.z) > 0.0001F);
                const Core::Vector3 launchOffset = Core::Subtract(
                    { request.data.origin.x, request.data.origin.y, request.data.origin.z },
                    request.impactOrigin);
                const float launchDistance = std::sqrt(std::max(
                    Core::Dot(launchOffset, launchOffset), 0.0F));
                const Core::Vector3 actualDirection =
                    CurrentTravelDirection(*projectile, *request.data.projectileBase);
                const float directionAlignment =
                    Core::Dot(request.requestedDirection, actualDirection);
                const char* shooterPath = "actor-cause";
                if (request.deferShooterAtLaunch && deferredShooter) {
                    shooterPath = "deferred-type";
                } else if (deferredShooter) {
                    shooterPath = "deferred-mismatch";
                } else if (shooterPreserved) {
                    shooterPath = "native";
                }
                REX::INFO(
                    "[BPR-DIAG] continuation chain={} handle={:08X} kind={} projectile={:08X} ammo={:08X} impact=({:.2f},{:.2f},{:.2f}) launch=({:.2f},{:.2f},{:.2f}) actual=({:.2f},{:.2f},{:.2f}) clearance={:.3f} normalClearance={:.3f} forwardClearance={:.3f} placementMode={} usedNormal={} requested=({:.4f},{:.4f},{:.4f}) actualDirection=({:.4f},{:.4f},{:.4f}) alignment={:.4f} movement=({:.3f},{:.3f},{:.3f}) velocity=({:.3f},{:.3f},{:.3f}) angles=({:.4f},{:.4f}) shooterPath={} sourceShooterPresent={} shooterPreserved={} contactNormalSet={} power={:.5f} damage={:.3f}",
                    request.chain.chainID, handleValue,
                    request.kind == ContinuationKind::kPenetration ? "penetration" : "rebound",
                    request.projectileForm, request.ammoForm,
                    request.impactOrigin.x, request.impactOrigin.y, request.impactOrigin.z,
                    request.data.origin.x, request.data.origin.y, request.data.origin.z,
                    position.x, position.y, position.z,
                    launchDistance, request.placement.normalClearance,
                    request.placement.forwardClearance,
                    request.placement.mode == Core::ContinuationPlacementMode::kMissilePenetration ?
                        "proven-missile-penetration" :
                        (request.placement.mode == Core::ContinuationPlacementMode::kMissileRebound ?
                            "proven-missile-rebound" : "surface-normal"),
                    request.placement.usedSurfaceNormal,
                    request.requestedDirection.x, request.requestedDirection.y,
                    request.requestedDirection.z,
                    actualDirection.x, actualDirection.y, actualDirection.z,
                    directionAlignment,
                    projectile->movementDirection.x, projectile->movementDirection.y,
                    projectile->movementDirection.z,
                    projectile->velocity.x, projectile->velocity.y, projectile->velocity.z,
                    projectile->data.angle.x, projectile->data.angle.z,
                    shooterPath,
                    sourceShooterPresent, shooterPreserved,
                    contactNormalSet,
                    projectile->power, projectile->damage);
            }
            return true;
        }
    }

    bool LaunchContinuation(
        RE::Projectile& source,
        Core::Vector3 origin,
        Core::Vector3 direction,
        Core::Vector3 surfaceNormal,
        float relativePowerScale,
        float remainingFraction,
        float surfaceTolerance,
        const Runtime::ChainState& state,
        ContinuationKind kind,
        bool diagnostics) noexcept
    {
        auto request = Capture(
            source, origin, direction, surfaceNormal, relativePowerScale,
            remainingFraction, surfaceTolerance, state, kind, diagnostics);
        return request && Execute(std::move(*request));
    }
}
