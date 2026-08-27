/*
 * Processing the first unprocessed projectile impact before the native
 * ProcessImpacts call is informed by Penetration System by jarari. BPR uses a
 * separately structured fail-closed pipeline, a new response model, a new
 * thickness solver, immutable configuration, and bounded chain ownership.
 */

#include "runtime/ImpactProcessor.h"

#include "core/PenetrationModel.h"
#include "core/ContinuationProgress.h"
#include "core/SurfaceResponse.h"
#include "engine/ProjectileLauncher.h"
#include "engine/ProjectileMotion.h"
#include "engine/RayQuery.h"
#include "runtime/ContinuationState.h"
#include "runtime/RuntimeConfig.h"
#include "pch.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <span>

namespace BPR::Runtime
{
    namespace
    {
        constexpr std::uint32_t kAbsoluteContinuationCeiling = 128;
        constexpr float kRadiansToDegrees = 57.29577951308232F;

        Core::Vector3 ToCore(const RE::NiPoint3& value) noexcept
        {
            return { value.x, value.y, value.z };
        }

        RE::NiPoint3 ToEngine(Core::Vector3 value) noexcept
        {
            return { value.x, value.y, value.z };
        }

        RE::BGSProjectile* BaseProjectile(RE::Projectile& projectile) noexcept
        {
            RE::TESBoundObject* base = projectile.GetObjectReference();
            return base ? base->As<RE::BGSProjectile>() : nullptr;
        }

        std::uint64_t ImpactToken(const RE::Projectile::ImpactData& impact, std::size_t index) noexcept
        {
            std::uint64_t token = (static_cast<std::uint64_t>(index) + 1U) << 57U;
            token ^= static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(impact.location.x)) << 25U;
            token ^= static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(impact.location.y)) << 3U;
            token ^= static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(impact.location.z));
            token ^= static_cast<std::uint64_t>(impact.collisionShapeKey) << 11U;
            token ^= reinterpret_cast<std::uintptr_t>(impact.colObj.get());
            return token != 0 ? token : 1U;
        }

        RE::Actor* ShooterActor(const RE::Projectile& projectile) noexcept
        {
            if (auto shooter = projectile.shooter.get()) {
                return shooter->As<RE::Actor>();
            }
            return nullptr;
        }

        Core::ShooterClass ClassifyShooter(const RE::Projectile& projectile) noexcept
        {
            if (RE::Actor* actor = ShooterActor(projectile)) {
                return actor->IsPlayerRef() ? Core::ShooterClass::kPlayer : Core::ShooterClass::kNPC;
            }
            RE::ActorCause* cause = projectile.GetActorCause();
            RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
            if (!cause) {
                return Core::ShooterClass::kUnknown;
            }
            return player && cause == player->GetActorCause() ?
                Core::ShooterClass::kPlayer : Core::ShooterClass::kNPC;
        }

        RE::TESObjectREFR* ImpactOwner(const RE::Projectile::ImpactData& impact) noexcept
        {
            auto owner = impact.collidee.get();
            return owner ? owner.get() : nullptr;
        }

        Core::ImpactOwnerClass ClassifyOwner(RE::TESObjectREFR* owner) noexcept
        {
            if (!owner) {
                return Core::ImpactOwnerClass::kUnknown;
            }
            if (owner->As<RE::Actor>()) {
                return Core::ImpactOwnerClass::kActor;
            }
            RE::TESBoundObject* base = owner->GetObjectReference();
            if (!base) {
                return Core::ImpactOwnerClass::kUnknown;
            }
            if (base->Is(
                    RE::ENUM_FORM_ID::kACTI, RE::ENUM_FORM_ID::kTACT,
                    RE::ENUM_FORM_ID::kCONT, RE::ENUM_FORM_ID::kDOOR,
                    RE::ENUM_FORM_ID::kLIGH, RE::ENUM_FORM_ID::kMISC,
                    RE::ENUM_FORM_ID::kMSTT, RE::ENUM_FORM_ID::kFURN,
                    RE::ENUM_FORM_ID::kWEAP, RE::ENUM_FORM_ID::kAMMO,
                    RE::ENUM_FORM_ID::kARMO, RE::ENUM_FORM_ID::kBOOK,
                    RE::ENUM_FORM_ID::kKEYM, RE::ENUM_FORM_ID::kALCH,
                    RE::ENUM_FORM_ID::kTERM)) {
                return Core::ImpactOwnerClass::kProp;
            }
            return Core::ImpactOwnerClass::kWorld;
        }

        std::uintptr_t ActorIdentity(RE::TESObjectREFR* owner) noexcept
        {
            return owner && owner->As<RE::Actor>() ?
                reinterpret_cast<std::uintptr_t>(owner) : 0;
        }

        float ReceiverScale(const RE::Projectile& projectile, const Config::RuntimeSettings& settings) noexcept
        {
            RE::TESForm* object = projectile.weaponSource.object;
            RE::TESObjectWEAP* weapon = object ? object->As<RE::TESObjectWEAP>() : nullptr;
            const auto* instance = static_cast<const RE::TESObjectWEAP::InstanceData*>(
                projectile.weaponSource.instanceData.get());
            if (!weapon || !instance) {
                return 1.0F;
            }
            return Core::ReceiverMultiplier(
                static_cast<float>(weapon->weaponData.attackDamage),
                static_cast<float>(instance->attackDamage),
                settings.receiver);
        }

        bool Explosive(const RE::Projectile& projectile, const RE::BGSProjectile& base) noexcept
        {
            return projectile.explosion != nullptr || base.data.explosionType != nullptr;
        }

        std::span<const Core::BoundaryHit> Hits(const Engine::BoundaryBatch& batch) noexcept
        {
            return { batch.hits.data(), batch.count };
        }

        float DirectionSeparationDegrees(Core::Vector3 left, Core::Vector3 right) noexcept
        {
            const auto first = Core::Normalize(left);
            const auto second = Core::Normalize(right);
            if (!first || !second) {
                return 0.0F;
            }
            return std::acos(std::clamp(Core::Dot(*first, *second), -1.0F, 1.0F)) *
                kRadiansToDegrees;
        }

        void ReleaseClaim(std::uint32_t handle, ImpactPhase phase) noexcept
        {
            if (phase == ImpactPhase::kNativeProcessing) {
                EraseProjectile(handle);
            }
        }

        bool TryRebound(
            RE::Projectile& projectile,
            const RE::Projectile::ImpactData& impact,
            std::uint32_t handle,
            std::uint64_t token,
            Core::Vector3 incoming,
            RE::TESObjectREFR* owner,
            Core::ImpactOwnerClass ownerClass,
            const Config::RuntimeSettings& settings,
            const Core::ProjectileProfile& projectileProfile,
            const ResolvedSurface& surface,
            ChainState state,
            Core::ReboundMode mode) noexcept
        {
            if (!Core::AllowsRicochet(
                    state.shooterClass, ownerClass, settings.rebound.enabled, settings.impactPolicy)) {
                return false;
            }
            const Core::ShooterControlSettings& controls =
                Core::ControlsFor(state.shooterClass, settings.impactPolicy);
            if (Core::ReachedLimit(state.reboundCount, controls.maxRicochets)) {
                return false;
            }

            const Core::ReboundResult response = Core::EvaluateRebound({
                .incoming = incoming,
                .surfaceNormal = ToCore(impact.normal),
                .projectile = projectileProfile,
                .surface = surface.profile,
                .settings = settings.rebound,
                .originalDamage = projectile.damage,
                .currentFraction = state.remainingFraction,
                .priorRebounds = state.reboundCount,
                .variationSeed = (static_cast<std::uint64_t>(handle) << 32U) ^ token ^ state.chainID,
                .mode = mode
            });
            if (!response.accepted) {
                if (response.chanceRejected && settings.detailedLogging) {
                    REX::INFO(
                        "[BPR-DIAG] chain={} rebound chance rejected mode={} roll={:.2f} chance={:.2f}",
                        state.chainID,
                        mode == Core::ReboundMode::kGlancingPriority ? "glancing" : "residual",
                        response.chanceRollPercent, settings.rebound.chancePercent);
                }
                return false;
            }

            const float relativePowerScale = response.remainingFraction /
                std::max(state.remainingFraction, 0.0001F);
            ++state.reboundCount;
            state.remainingFraction = response.remainingFraction;
            state.lastActor = ActorIdentity(owner);
            if (!Engine::LaunchContinuation(
                    projectile, ToCore(impact.location), response.direction, ToCore(impact.normal),
                    relativePowerScale,
                    state.remainingFraction, settings.thickness.entrySeparation,
                    state, Engine::ContinuationKind::kRebound,
                    settings.detailedLogging)) {
                return false;
            }
            if (settings.detailedLogging) {
                REX::INFO(
                    "[BPR] chain={} rebound planeAngle={:.2f} chance={:.2f}/{:.2f} costPercent={:.2f} remaining={:.4f} count={}/{} material={}->{}",
                    state.chainID, response.planeAngleDegrees,
                    response.chanceRollPercent, settings.rebound.chancePercent,
                    response.energyCostPercent,
                    state.remainingFraction, state.reboundCount, controls.maxRicochets,
                    surface.runtimeName.empty() ? "<unknown>" : surface.runtimeName,
                    surface.familyName.empty() ? "<fallback>" : surface.familyName);
            }
            return true;
        }

        bool TryPenetration(
            RE::Projectile& projectile,
            const RE::Projectile::ImpactData& impact,
            std::uint32_t handle,
            std::uint64_t token,
            Core::Vector3 incoming,
            RE::BGSProjectile& projectileBase,
            RE::TESObjectREFR* owner,
            const ConfigurationSnapshot& configuration,
            const Config::RuntimeSettings& settings,
            const Core::ProjectileProfile& projectileProfile,
            const ResolvedSurface& surface,
            ChainState state) noexcept
        {
            if (!Core::AllowsPenetration(state.shooterClass, settings.impactPolicy)) {
                return false;
            }
            const Core::ShooterControlSettings& controls =
                Core::ControlsFor(state.shooterClass, settings.impactPolicy);
            if (Core::ReachedLimit(state.penetrationCount, controls.maxPenetrations)) {
                return false;
            }

            const float ammoDepth = Runtime::AmmoDepth(configuration, projectile.ammoSource);
            const float receiver = ReceiverScale(projectile, settings);
            const float maximumDepth = Core::MaximumDepth(
                ammoDepth, projectileProfile.penetrationScale, receiver, surface.profile.penetrationScale);
            if (maximumDepth <= settings.thickness.entrySeparation) {
                return false;
            }

            const Core::Vector3 entryPoint = ToCore(impact.location);
            const Core::Vector3 forwardStart = Core::Add(
                entryPoint, Core::Scale(incoming, settings.thickness.entrySeparation * 0.5F));
            const Core::Vector3 forwardEnd = Core::Add(
                entryPoint, Core::Scale(incoming, maximumDepth + settings.thickness.duplicateSeparation));
            Engine::BoundaryBatch forward = Engine::QueryBoundaries(
                projectile, ShooterActor(projectile), projectileBase,
                ToEngine(forwardStart), ToEngine(forwardEnd), settings.detailedLogging);

            const Core::SurfaceEntry entry{
                .point = entryPoint,
                .incoming = incoming,
                .body = Engine::CollisionBodyIdentity(impact.colObj.get()),
                .owner = reinterpret_cast<std::uintptr_t>(owner),
                .shapeKey = impact.collisionShapeKey
            };
            Core::ThicknessSolution solution;
            if (forward.completed) {
                solution = Core::SolveThickness(entry, maximumDepth, Hits(forward), {}, settings.thickness);
            }

            Engine::BoundaryBatch reverse;
            if (!solution.found || solution.confidence == Core::ThicknessConfidence::kOwnerMatch) {
                const Core::Vector3 reverseStart = Core::Add(
                    entryPoint, Core::Scale(incoming, maximumDepth + settings.thickness.duplicateSeparation));
                const Core::Vector3 reverseEnd = Core::Subtract(
                    entryPoint, Core::Scale(incoming, settings.thickness.entrySeparation));
                reverse = Engine::QueryBoundaries(
                    projectile, ShooterActor(projectile), projectileBase,
                    ToEngine(reverseStart), ToEngine(reverseEnd), settings.detailedLogging);
                if (reverse.completed) {
                    solution = Core::SolveThickness(
                        entry, maximumDepth, Hits(forward), Hits(reverse), settings.thickness);
                }
            }
            if (!solution.found) {
                return false;
            }

            const float localFraction = Core::RemainingFraction(
                solution.thickness, maximumDepth, settings.damageFalloffExponent);
            const float remaining = state.remainingFraction * localFraction;
            if (!std::isfinite(remaining) || remaining <= 0.0001F) {
                return false;
            }

            ++state.penetrationCount;
            state.remainingFraction = remaining;
            state.lastActor = ActorIdentity(owner);
            const Core::Vector3 continuationDirection = Core::PenetrationExitDirection(
                incoming, solution.exitNormal, settings.penetrationVariationDegrees,
                settings.thickness.outwardAlignment,
                (static_cast<std::uint64_t>(handle) << 32U) ^ token ^ state.chainID ^
                    static_cast<std::uint64_t>(state.penetrationCount));
            const float deflection = DirectionSeparationDegrees(incoming, continuationDirection);
            if (!Engine::LaunchContinuation(
                    projectile, solution.exitPoint, continuationDirection, solution.exitNormal,
                    localFraction,
                    state.remainingFraction, settings.thickness.entrySeparation,
                    state, Engine::ContinuationKind::kPenetration,
                    settings.detailedLogging)) {
                return false;
            }
            if (settings.detailedLogging) {
                REX::INFO(
                    "[BPR] chain={} penetration thickness={:.3f}/{:.3f} local={:.4f} cumulative={:.4f} deflection={:.2f}/{:.2f} count={}/{} confidence={} rays={} hits={}/{} material={}->{} ammoDepth={:.2f} profile={:.2f} receiver={:.2f} surface={:.2f}",
                    state.chainID, solution.thickness, maximumDepth, localFraction,
                    state.remainingFraction, deflection, settings.penetrationVariationDegrees,
                    state.penetrationCount, controls.maxPenetrations,
                    static_cast<unsigned>(solution.confidence), reverse.completed ? 2 : 1,
                    forward.count, reverse.count,
                    surface.runtimeName.empty() ? "<unknown>" : surface.runtimeName,
                    surface.familyName.empty() ? "<fallback>" : surface.familyName,
                    ammoDepth, projectileProfile.penetrationScale, receiver,
                    surface.profile.penetrationScale);
            }
            return true;
        }
    }

    void ProcessProjectileImpact(RE::Projectile& projectile, ImpactPhase phase) noexcept
    {
        try {
            const std::uint32_t handle = ProjectileHandleValue(projectile);
            if (handle == 0) {
                return;
            }
            RE::BGSProjectile* base = BaseProjectile(projectile);
            if (!base || Explosive(projectile, *base)) {
                EraseProjectile(handle);
                return;
            }

            RE::Projectile::ImpactData* impact = nullptr;
            std::uint32_t impactIndex = 0;
            for (std::uint32_t index = 0; index < projectile.impacts.size(); ++index) {
                if (!projectile.impacts[index].processed) {
                    impact = &projectile.impacts[index];
                    impactIndex = index;
                    break;
                }
            }
            if (!impact) {
                if (!projectile.impacts.empty()) {
                    EraseProjectile(handle);
                }
                return;
            }

            ChainState state;
            const std::uint64_t token = ImpactToken(*impact, impactIndex);
            if (!ClaimImpact(handle, token, state)) {
                ReleaseClaim(handle, phase);
                return;
            }
            const bool deferredShooterApplied = ApplyDeferredShooter(handle, projectile);
            const Configuration configuration = AcquireConfiguration();
            const Config::RuntimeSettings& settings = GlobalSettings(*configuration);
            if (state.shooterClass == Core::ShooterClass::kUnknown) {
                state.shooterClass = ClassifyShooter(projectile);
            }
            if (state.validateForwardProgress) {
                const Core::Vector3 impactPoint = ToCore(impact->location);
                const auto progress = Core::ForwardProgress(
                    state.launchPoint, state.requestedDirection, impactPoint);
                if (!progress || *progress < -Core::kContinuationBacktrackTolerance) {
                    if (settings.detailedLogging) {
                        if (progress) {
                            const Core::Vector3 motionDirection =
                                Engine::CurrentTravelDirection(projectile, *base);
                            const float directionAlignment =
                                Core::Dot(state.requestedDirection, motionDirection);
                            const RE::NiPoint3 position = projectile.GetPosition();
                            REX::INFO(
                                "[BPR-DIAG] continuation chain={} handle={:08X} stopped: backward progress {:.3f} impact=({:.2f},{:.2f},{:.2f}) position=({:.2f},{:.2f},{:.2f}) launch=({:.2f},{:.2f},{:.2f}) requested=({:.4f},{:.4f},{:.4f}) actualDirection=({:.4f},{:.4f},{:.4f}) alignment={:.4f} movement=({:.3f},{:.3f},{:.3f}) velocity=({:.3f},{:.3f},{:.3f}) angles=({:.4f},{:.4f}) distanceMoved={:.3f} range={:.3f}",
                                state.chainID, handle, *progress,
                                impactPoint.x, impactPoint.y, impactPoint.z,
                                position.x, position.y, position.z,
                                state.launchPoint.x, state.launchPoint.y, state.launchPoint.z,
                                state.requestedDirection.x, state.requestedDirection.y,
                                state.requestedDirection.z,
                                motionDirection.x, motionDirection.y, motionDirection.z,
                                directionAlignment,
                                projectile.movementDirection.x, projectile.movementDirection.y,
                                projectile.movementDirection.z,
                                projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
                                projectile.data.angle.x, projectile.data.angle.z,
                                projectile.distanceMoved, projectile.range);
                        } else {
                            REX::INFO(
                                "[BPR-DIAG] continuation chain={} handle={:08X} stopped: invalid progress geometry",
                                state.chainID, handle);
                        }
                    }
                    ReleaseClaim(handle, phase);
                    return;
                }
            }
            if (state.penetrationCount + state.reboundCount >= kAbsoluteContinuationCeiling) {
                REX::WARN("BPR stopped chain {} at the absolute continuation safety ceiling", state.chainID);
                ReleaseClaim(handle, phase);
                return;
            }
            if (std::isfinite(projectile.range) && projectile.range > 0.0F) {
                state.remainingRange = state.remainingRange < 0.0F ?
                    std::max(projectile.range - projectile.distanceMoved, 0.0F) :
                    std::max(state.remainingRange - projectile.distanceMoved, 0.0F);
                if (state.remainingRange <= 0.0F) {
                    ReleaseClaim(handle, phase);
                    return;
                }
            }
            const Core::Vector3 incoming = Engine::CurrentTravelDirection(projectile, *base);
            if (!Core::Normalize(incoming)) {
                ReleaseClaim(handle, phase);
                return;
            }

            RE::TESObjectREFR* owner = ImpactOwner(*impact);
            const Core::ImpactOwnerClass ownerClass = ClassifyOwner(owner);
            const std::uintptr_t actor = ActorIdentity(owner);
            if (settings.preventRepeatActor && actor != 0 && actor == state.lastActor) {
                ReleaseClaim(handle, phase);
                return;
            }
            const Core::ProjectileProfile projectileProfile =
                Runtime::AmmoProfile(*configuration, projectile.ammoSource, base);
            const ResolvedSurface surface = Runtime::Surface(*configuration, impact->materialType);

            if (settings.detailedLogging) {
                REX::INFO(
                    "[BPR-DIAG] impact phase={} chain={} handle={:08X} index={} damage={:.3f} power={:.5f} remaining={:.4f} deferredShooterApplied={} shooterClass={} ownerClass={} projectileFlags={:08X} profileDepth={:.3f} energyBeam={}",
                    phase == ImpactPhase::kImpactAdded ? "add" : "process",
                    state.chainID, handle, impactIndex, projectile.damage, projectile.power,
                    state.remainingFraction, deferredShooterApplied,
                    static_cast<unsigned>(state.shooterClass), static_cast<unsigned>(ownerClass),
                    base->data.flags, projectileProfile.penetrationScale,
                    projectileProfile.energyBeam);
            }

            if (TryRebound(
                    projectile, *impact, handle, token, incoming, owner, ownerClass,
                    settings, projectileProfile, surface, state,
                    Core::ReboundMode::kGlancingPriority)) {
                ReleaseClaim(handle, phase);
                return;
            }
            if (TryPenetration(
                    projectile, *impact, handle, token, incoming, *base, owner, *configuration,
                    settings, projectileProfile, surface, state)) {
                ReleaseClaim(handle, phase);
                return;
            }
            if (TryRebound(
                    projectile, *impact, handle, token, incoming, owner, ownerClass,
                    settings, projectileProfile, surface, state,
                    Core::ReboundMode::kResidualEnergy)) {
                ReleaseClaim(handle, phase);
                return;
            }
            ReleaseClaim(handle, phase);
        } catch (const std::exception& exception) {
            REX::ERROR("BPR impact processing failed safely: {}", exception.what());
        } catch (...) {
            REX::ERROR("BPR impact processing failed safely with an unknown error");
        }
    }
}
