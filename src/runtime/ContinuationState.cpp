/*
 * Continuation state is bounded and sharded. Fallout's native launch-time
 * shooter fields are authoritative. A one-use deferred handle is stored only
 * when post-launch validation proves that Fallout did not preserve them.
 */

#include "runtime/ContinuationState.h"

#include "pch.h"

#include <array>
#include <atomic>
#include <mutex>

namespace BPR::Runtime
{
    namespace
    {
        constexpr std::size_t kShardCount = 64;
        constexpr std::size_t kSlotsPerShard = 128;

        enum class SlotStatus : std::uint8_t
        {
            kEmpty,
            kOccupied,
            kDeleted
        };

        struct Slot
        {
            std::uint32_t handle{ 0 };
            ChainState chain;
            RE::ObjectRefHandle deferredShooter;
            std::uint64_t generation{ 0 };
            SlotStatus status{ SlotStatus::kEmpty };
        };

        struct Shard
        {
            std::array<Slot, kSlotsPerShard> slots{};
            std::mutex lock;
            std::uint64_t generation{ 0 };
        };

        std::array<Shard, kShardCount> g_shards;
        std::atomic_uint64_t g_nextChain{ 1 };
        std::atomic_uint64_t g_overflows{ 0 };

        std::uint32_t Mix(std::uint32_t value) noexcept
        {
            value ^= value >> 16U;
            value *= 0x7FEB352DU;
            value ^= value >> 15U;
            value *= 0x846CA68BU;
            return value ^ (value >> 16U);
        }

        Shard& ShardFor(std::uint32_t handle) noexcept
        {
            return g_shards[Mix(handle) % kShardCount];
        }

        std::size_t StartSlot(std::uint32_t handle) noexcept
        {
            return (Mix(handle) / kShardCount) % kSlotsPerShard;
        }

        Slot* Find(Shard& shard, std::uint32_t handle) noexcept
        {
            const std::size_t start = StartSlot(handle);
            for (std::size_t probe = 0; probe < shard.slots.size(); ++probe) {
                Slot& slot = shard.slots[(start + probe) % shard.slots.size()];
                if (slot.status == SlotStatus::kEmpty) {
                    return nullptr;
                }
                if (slot.status == SlotStatus::kOccupied && slot.handle == handle) {
                    return &slot;
                }
            }
            return nullptr;
        }

        Slot& Reserve(Shard& shard, std::uint32_t handle) noexcept
        {
            const std::size_t start = StartSlot(handle);
            Slot* deleted = nullptr;
            for (std::size_t probe = 0; probe < shard.slots.size(); ++probe) {
                Slot& slot = shard.slots[(start + probe) % shard.slots.size()];
                if (slot.status == SlotStatus::kOccupied && slot.handle == handle) {
                    return slot;
                }
                if (slot.status == SlotStatus::kDeleted && !deleted) {
                    deleted = &slot;
                }
                if (slot.status == SlotStatus::kEmpty) {
                    return deleted ? *deleted : slot;
                }
            }
            if (deleted) {
                return *deleted;
            }
            const std::uint64_t overflow = g_overflows.fetch_add(1, std::memory_order_relaxed) + 1;
            if ((overflow & (overflow - 1U)) == 0) {
                REX::WARN("BPR continuation state capacity reached ({} overflows)", overflow);
            }
            Slot* oldest = &shard.slots.front();
            for (Slot& slot : shard.slots) {
                if (slot.generation < oldest->generation) {
                    oldest = &slot;
                }
            }
            return *oldest;
        }

        void Assign(
            Shard& shard,
            Slot& slot,
            std::uint32_t handle,
            const ChainState& chain,
            RE::ObjectRefHandle deferredShooter = {}) noexcept
        {
            slot.handle = handle;
            slot.chain = chain;
            slot.deferredShooter = deferredShooter;
            slot.generation = ++shard.generation;
            slot.status = SlotStatus::kOccupied;
        }
    }

    std::uint32_t ProjectileHandleValue(RE::Projectile& projectile) noexcept
    {
        RE::ProjectileHandle handle =
            RE::BSPointerHandleManagerInterface<RE::Projectile>::GetHandle(&projectile);
        return handle ? handle.get_handle() : 0;
    }

    bool ClaimImpact(
        std::uint32_t projectileHandle,
        std::uint64_t impactToken,
        ChainState& output) noexcept
    {
        if (projectileHandle == 0 || impactToken == 0) {
            return false;
        }
        Shard& shard = ShardFor(projectileHandle);
        std::scoped_lock lock(shard.lock);
        Slot* slot = Find(shard, projectileHandle);
        if (!slot) {
            output = {};
            output.chainID = g_nextChain.fetch_add(1, std::memory_order_relaxed);
            output.lastImpactToken = impactToken;
            Assign(shard, Reserve(shard, projectileHandle), projectileHandle, output);
            return true;
        }
        if (slot->chain.lastImpactToken == impactToken) {
            return false;
        }
        slot->chain.lastImpactToken = impactToken;
        slot->generation = ++shard.generation;
        output = slot->chain;
        return true;
    }

    void RegisterContinuation(
        std::uint32_t continuationHandle,
        const ChainState& state,
        RE::ObjectRefHandle deferredShooter) noexcept
    {
        if (continuationHandle == 0) {
            return;
        }
        Shard& shard = ShardFor(continuationHandle);
        std::scoped_lock lock(shard.lock);
        ChainState copy = state;
        copy.lastImpactToken = 0;
        Assign(shard, Reserve(shard, continuationHandle), continuationHandle, copy, deferredShooter);
    }

    bool ApplyDeferredShooter(std::uint32_t projectileHandle, RE::Projectile& projectile) noexcept
    {
        if (projectileHandle == 0) {
            return false;
        }
        RE::ObjectRefHandle shooter;
        {
            Shard& shard = ShardFor(projectileHandle);
            std::scoped_lock lock(shard.lock);
            Slot* slot = Find(shard, projectileHandle);
            if (!slot || !slot->deferredShooter) {
                return false;
            }
            shooter = slot->deferredShooter;
            slot->deferredShooter.reset();
        }
        projectile.shooter = shooter;
        return true;
    }

    void EraseProjectile(std::uint32_t projectileHandle) noexcept
    {
        if (projectileHandle == 0) {
            return;
        }
        Shard& shard = ShardFor(projectileHandle);
        std::scoped_lock lock(shard.lock);
        if (Slot* slot = Find(shard, projectileHandle)) {
            slot->chain = {};
            slot->deferredShooter.reset();
            slot->status = SlotStatus::kDeleted;
        }
    }

    void ClearAllChains() noexcept
    {
        for (Shard& shard : g_shards) {
            std::scoped_lock lock(shard.lock);
            for (Slot& slot : shard.slots) {
                slot = {};
            }
            shard.generation = 0;
        }
        g_overflows.store(0, std::memory_order_relaxed);
    }
}
